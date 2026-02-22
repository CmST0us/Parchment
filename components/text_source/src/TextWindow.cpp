/**
 * @file TextWindow.cpp
 * @brief TextWindow 实现 — PSRAM 512KB 滑动窗口。
 */

#include "TextWindow.h"

extern "C" {
#include "esp_heap_caps.h"
#include "esp_log.h"
}

static const char* TAG = "TextWindow";

TextWindow::TextWindow() = default;

TextWindow::~TextWindow() {
    deinit();
}

bool TextWindow::init() {
    if (buffer_) return true;
    buffer_ = static_cast<char*>(
        heap_caps_malloc(WINDOW_SIZE, MALLOC_CAP_SPIRAM));
    if (!buffer_) {
        ESP_LOGE(TAG, "Failed to allocate %lu bytes in PSRAM",
                 (unsigned long)WINDOW_SIZE);
        return false;
    }
    bufStart_ = 0;
    bufSize_ = 0;
    return true;
}

void TextWindow::deinit() {
    if (buffer_) {
        heap_caps_free(buffer_);
        buffer_ = nullptr;
    }
    bufStart_ = 0;
    bufSize_ = 0;
    if (file_) {
        fclose(file_);
        file_ = nullptr;
    }
    fileSize_ = 0;
    fileBaseOffset_ = 0;
    memBuf_ = nullptr;
    memSize_ = 0;
    usingMemory_ = false;
}

void TextWindow::setFile(FILE* file, uint32_t fileSize,
                         uint32_t fileBaseOffset) {
    if (file_ && file_ != file) {
        fclose(file_);
    }
    file_ = file;
    fileSize_ = fileSize;
    fileBaseOffset_ = fileBaseOffset;
    memBuf_ = nullptr;
    memSize_ = 0;
    usingMemory_ = false;
    // 清除当前窗口内容，下次 spanAt 时触发重新加载
    bufStart_ = 0;
    bufSize_ = 0;
}

void TextWindow::setMemory(const char* buf, uint32_t size) {
    memBuf_ = buf;
    memSize_ = size;
    usingMemory_ = true;
    // 内存模式下窗口直接映射整个缓冲区
    bufStart_ = 0;
    bufSize_ = size;
}

bool TextWindow::load(uint32_t offset) {
    // 内存模式：数据已在内存中
    if (usingMemory_) return true;

    if (!file_ || !buffer_) return false;

    uint32_t totalSize = fileSize_;
    if (offset >= totalSize) return false;

    // 计算窗口起始：将 offset 放在窗口中心附近
    uint32_t halfWindow = WINDOW_SIZE / 2;
    uint32_t start = 0;
    if (offset > halfWindow) {
        start = offset - halfWindow;
    }

    // 确保不超出文件末尾
    uint32_t readSize = WINDOW_SIZE;
    if (start + readSize > totalSize) {
        readSize = totalSize - start;
    }

    // 对齐起始位置到 UTF-8 字符边界
    if (start > 0) {
        fseek(file_, fileBaseOffset_ + start, SEEK_SET);
        char probe[4];
        size_t probeRead = fread(probe, 1, sizeof(probe), file_);
        if (probeRead > 0) {
            uint32_t skip = alignUtf8Forward(probe, 0, probeRead);
            start += skip;
            if (readSize > skip) {
                readSize -= skip;
            }
        }
    }

    // 读取数据到缓冲区
    fseek(file_, fileBaseOffset_ + start, SEEK_SET);
    size_t bytesRead = fread(buffer_, 1, readSize, file_);
    if (bytesRead == 0) {
        ESP_LOGE(TAG, "Failed to read at offset %lu", (unsigned long)start);
        return false;
    }

    bufStart_ = start;
    bufSize_ = static_cast<uint32_t>(bytesRead);

    ESP_LOGD(TAG, "Window loaded: [%lu, %lu) (%lu bytes)",
             (unsigned long)bufStart_,
             (unsigned long)(bufStart_ + bufSize_),
             (unsigned long)bufSize_);

    return true;
}

ink::TextSpan TextWindow::spanAt(uint32_t offset) const {
    if (usingMemory_) {
        if (offset >= memSize_) {
            return {nullptr, 0};
        }
        return {memBuf_ + offset, memSize_ - offset};
    }

    if (!buffer_ || bufSize_ == 0) {
        return {nullptr, 0};
    }

    if (offset < bufStart_ || offset >= bufStart_ + bufSize_) {
        return {nullptr, 0};
    }

    uint32_t pos = offset - bufStart_;
    return {buffer_ + pos, bufSize_ - pos};
}

bool TextWindow::contains(uint32_t offset) const {
    if (usingMemory_) {
        return offset < memSize_;
    }
    return buffer_ && bufSize_ > 0 &&
           offset >= bufStart_ && offset < bufStart_ + bufSize_;
}

uint32_t TextWindow::sourceSize() const {
    if (usingMemory_) return memSize_;
    return fileSize_;
}

uint32_t TextWindow::alignUtf8Forward(const char* buf, uint32_t offset,
                                       uint32_t size) {
    // 跳过 UTF-8 continuation bytes (10xxxxxx) 找到字符起始
    uint32_t skip = 0;
    while (offset + skip < size) {
        uint8_t b = static_cast<uint8_t>(buf[offset + skip]);
        if ((b & 0xC0) != 0x80) {
            // 非 continuation byte，这是字符起始
            break;
        }
        skip++;
        if (skip >= 3) break;  // UTF-8 最多 3 个 continuation bytes
    }
    return skip;
}
