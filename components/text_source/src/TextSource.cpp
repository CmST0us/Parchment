/**
 * @file TextSource.cpp
 * @brief TextSource 实现 — 流式 UTF-8 文本源。
 */

#include "text_source/TextSource.h"
#include "TextWindow.h"
#include "EncodingConverter.h"

#include <cstring>
#include <cstdio>
#include <sys/stat.h>

extern "C" {
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "text_encoding.h"
}

static const char* TAG = "TextSource";

namespace ink {

// ════════════════════════════════════════════════════════════════
//  构造与析构
// ════════════════════════════════════════════════════════════════

TextSource::TextSource() = default;

TextSource::~TextSource() {
    close();
}

// ════════════════════════════════════════════════════════════════
//  open / close
// ════════════════════════════════════════════════════════════════

bool TextSource::open(const char* filePath, const char* cacheDirPath) {
    close();

    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = TextSourceState::Preparing;

        // 保存路径
        strncpy(filePath_, filePath, sizeof(filePath_) - 1);
        strncpy(cacheDirPath_, cacheDirPath, sizeof(cacheDirPath_) - 1);

        // 构造缓存文件路径
        snprintf(utf8FilePath_, sizeof(utf8FilePath_), "%s/text.utf8", cacheDirPath);
    }

    // 检测编码并初始化（不持有 mutex，因为会启动后台 task）
    if (!initEncoding()) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = TextSourceState::Error;
        return false;
    }

    return true;
}

void TextSource::close() {
    // 先标记关闭状态，防止后台 task 回调修改共享状态
    {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = TextSourceState::Closed;
    }

    // 停止后台转换（stop() 内部等待 task 退出）
    if (converter_) {
        converter_->stop();
        delete converter_;
        converter_ = nullptr;
    }

    // 释放窗口（此时后台 task 已停止，安全操作）
    if (window_) {
        window_->deinit();
        delete window_;
        window_ = nullptr;
    }

    // 释放首块缓冲区
    if (firstChunkBuf_) {
        heap_caps_free(firstChunkBuf_);
        firstChunkBuf_ = nullptr;
        firstChunkSize_ = 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    totalSize_ = 0;
    availableSize_ = 0;
    usingFirstChunk_ = false;
    filePath_[0] = '\0';
    cacheDirPath_[0] = '\0';
    utf8FilePath_[0] = '\0';
}

// ════════════════════════════════════════════════════════════════
//  read
// ════════════════════════════════════════════════════════════════

TextSpan TextSource::read(uint32_t offset) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (state_ == TextSourceState::Closed || state_ == TextSourceState::Error) {
        return {nullptr, 0};
    }

    // 检查是否超出当前可用范围
    if (offset >= availableSize_) {
        return {nullptr, 0};
    }

    if (!window_) return {nullptr, 0};

    // 如果 offset 不在当前窗口内，重新加载
    if (!window_->contains(offset)) {
        if (!window_->load(offset)) {
            return {nullptr, 0};
        }
    }

    return window_->spanAt(offset);
}

// ════════════════════════════════════════════════════════════════
//  查询方法
// ════════════════════════════════════════════════════════════════

TextSourceState TextSource::state() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

uint32_t TextSource::totalSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalSize_;
}

uint32_t TextSource::availableSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return availableSize_;
}

float TextSource::progress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == TextSourceState::Ready) return 1.0f;
    if (state_ == TextSourceState::Closed || state_ == TextSourceState::Error) {
        return 0.0f;
    }
    if (originalFileSize_ == 0) return 0.0f;
    // GBK 转换进度基于已处理的源文件字节数
    // availableSize_ 是 UTF-8 输出大小，用原始文件大小估算
    return static_cast<float>(availableSize_) /
           static_cast<float>(originalFileSize_ * 3 / 2);
}

int TextSource::detectedEncoding() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return detectedEncoding_;
}

uint32_t TextSource::originalFileSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return originalFileSize_;
}

// ════════════════════════════════════════════════════════════════
//  内部：编码检测与初始化
// ════════════════════════════════════════════════════════════════

bool TextSource::initEncoding() {
    // 打开原始文件获取大小
    FILE* f = fopen(filePath_, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s", filePath_);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    fclose(f);

    if (fsize <= 0) {
        ESP_LOGE(TAG, "File size invalid: %ld", fsize);
        return false;
    }
    originalFileSize_ = static_cast<uint32_t>(fsize);

    // 读取前几 KB 用于编码检测
    f = fopen(filePath_, "rb");
    if (!f) return false;

    constexpr uint32_t PROBE_SIZE = 8 * 1024;
    uint32_t probeSize = (originalFileSize_ < PROBE_SIZE)
                             ? originalFileSize_ : PROBE_SIZE;
    char* probeBuf = static_cast<char*>(heap_caps_malloc(PROBE_SIZE, MALLOC_CAP_INTERNAL));
    if (!probeBuf) {
        fclose(f);
        return false;
    }
    size_t readBytes = fread(probeBuf, 1, probeSize, f);
    fclose(f);

    if (readBytes == 0) {
        heap_caps_free(probeBuf);
        return false;
    }

    text_encoding_t enc = text_encoding_detect(probeBuf, readBytes);
    heap_caps_free(probeBuf);
    detectedEncoding_ = static_cast<int>(enc);

    ESP_LOGI(TAG, "Detected encoding: %d, file size: %lu bytes",
             (int)enc, (unsigned long)originalFileSize_);

    if (enc == TEXT_ENCODING_GBK) {
        return initGbk();
    } else {
        return initUtf8();
    }
}

bool TextSource::initUtf8() {
    // UTF-8 或 UTF-8 BOM：直接使用原文件
    strncpy(utf8FilePath_, filePath_, sizeof(utf8FilePath_) - 1);

    uint32_t skipBytes = 0;
    if (detectedEncoding_ == static_cast<int>(TEXT_ENCODING_UTF8_BOM)) {
        skipBytes = 3;  // 跳过 BOM
    }

    totalSize_ = originalFileSize_ - skipBytes;
    availableSize_ = totalSize_;

    // 初始化滑动窗口
    window_ = new TextWindow();
    if (!window_->init()) {
        delete window_;
        window_ = nullptr;
        return false;
    }

    // 打开文件供窗口使用
    FILE* f = fopen(utf8FilePath_, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open UTF-8 file: %s", utf8FilePath_);
        return false;
    }

    window_->setFile(f, totalSize_, skipBytes);

    // 预加载首块
    window_->load(0);

    state_ = TextSourceState::Ready;
    ESP_LOGI(TAG, "UTF-8 file ready: %lu bytes", (unsigned long)totalSize_);
    return true;
}

bool TextSource::initGbk() {
    // 检查缓存是否可用
    if (checkCache()) {
        // 缓存命中，直接使用 text.utf8
        window_ = new TextWindow();
        if (!window_->init()) {
            delete window_;
            window_ = nullptr;
            return false;
        }

        FILE* f = fopen(utf8FilePath_, "rb");
        if (!f) {
            ESP_LOGE(TAG, "Failed to open cached UTF-8: %s", utf8FilePath_);
            return false;
        }

        // 获取缓存文件大小（减去 4 字节 DONE 标记）
        fseek(f, 0, SEEK_END);
        long cacheSize = ftell(f);
        fseek(f, 0, SEEK_SET);
        totalSize_ = static_cast<uint32_t>(cacheSize - 4);
        availableSize_ = totalSize_;

        window_->setFile(f, totalSize_);
        window_->load(0);

        state_ = TextSourceState::Ready;
        ESP_LOGI(TAG, "GBK cache hit: %lu bytes UTF-8",
                 (unsigned long)totalSize_);
        return true;
    }

    // 缓存未命中：首块内存转换 + 启动后台 task
    // 分配首块缓冲区（128KB 源 → 最大 192KB UTF-8）
    constexpr uint32_t FIRST_CHUNK_CAP = 192 * 1024;
    firstChunkBuf_ = static_cast<char*>(
        heap_caps_malloc(FIRST_CHUNK_CAP, MALLOC_CAP_SPIRAM));
    if (!firstChunkBuf_) {
        ESP_LOGE(TAG, "Failed to allocate first chunk buffer");
        return false;
    }

    // 同步转换首块
    converter_ = new EncodingConverter();
    if (!converter_->convertFirstChunk(filePath_, firstChunkBuf_,
                                       FIRST_CHUNK_CAP, &firstChunkSize_)) {
        ESP_LOGE(TAG, "Failed to convert first chunk");
        heap_caps_free(firstChunkBuf_);
        firstChunkBuf_ = nullptr;
        delete converter_;
        converter_ = nullptr;
        return false;
    }

    ESP_LOGI(TAG, "First chunk converted: %lu bytes UTF-8",
             (unsigned long)firstChunkSize_);

    // 初始化窗口为内存模式
    window_ = new TextWindow();
    if (!window_->init()) {
        delete window_;
        window_ = nullptr;
        return false;
    }
    window_->setMemory(firstChunkBuf_, firstChunkSize_);
    availableSize_ = firstChunkSize_;
    usingFirstChunk_ = true;

    // 创建缓存目录
    mkdir(cacheDirPath_, 0755);

    // 启动后台转换
    if (!converter_->startBackgroundConversion(filePath_, utf8FilePath_,
                                                originalFileSize_, this)) {
        ESP_LOGE(TAG, "Failed to start background conversion");
        // 首块仍然可用，降级为仅首块模式
        state_ = TextSourceState::Available;
        return true;
    }

    state_ = TextSourceState::Converting;
    return true;
}

bool TextSource::checkCache() {
    FILE* f = fopen(utf8FilePath_, "rb");
    if (!f) return false;

    // 检查文件末尾是否有 DONE 标记
    fseek(f, -4, SEEK_END);
    long fileSize = ftell(f) + 4;
    if (fileSize <= 4) {
        fclose(f);
        return false;
    }

    char magic[4];
    if (fread(magic, 1, 4, f) != 4) {
        fclose(f);
        return false;
    }
    fclose(f);

    if (memcmp(magic, "DONE", 4) != 0) {
        // 不完整缓存，删除
        ESP_LOGW(TAG, "Incomplete cache, removing: %s", utf8FilePath_);
        remove(utf8FilePath_);
        return false;
    }

    ESP_LOGI(TAG, "Valid cache found: %s (%ld bytes)", utf8FilePath_, fileSize);
    return true;
}

// ════════════════════════════════════════════════════════════════
//  后台转换回调
// ════════════════════════════════════════════════════════════════

void TextSource::onConversionComplete(uint32_t utf8Size) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 如果已关闭，忽略回调
    if (state_ == TextSourceState::Closed) return;

    totalSize_ = utf8Size;
    availableSize_ = utf8Size;

    // 切换窗口到文件模式
    FILE* f = fopen(utf8FilePath_, "rb");
    if (f && window_) {
        window_->setFile(f, utf8Size);
        window_->load(0);
    }

    // 释放首块缓冲区
    if (firstChunkBuf_) {
        heap_caps_free(firstChunkBuf_);
        firstChunkBuf_ = nullptr;
        firstChunkSize_ = 0;
    }
    usingFirstChunk_ = false;

    state_ = TextSourceState::Ready;
    ESP_LOGI(TAG, "Conversion complete: %lu bytes UTF-8",
             (unsigned long)utf8Size);
}

void TextSource::updateAvailableSize(uint32_t size) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (state_ == TextSourceState::Closed) return;
    availableSize_ = size;
}

}  // namespace ink
