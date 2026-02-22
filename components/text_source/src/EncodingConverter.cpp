/**
 * @file EncodingConverter.cpp
 * @brief EncodingConverter 实现 — GBK -> UTF-8 后台流式转换。
 */

#include "EncodingConverter.h"
#include "text_source/TextSource.h"

#include <cstring>

extern "C" {
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "text_encoding.h"
}

static const char* TAG = "EncConverter";

EncodingConverter::EncodingConverter() = default;

EncodingConverter::~EncodingConverter() {
    stop();
}

// ════════════════════════════════════════════════════════════════
//  首块同步转换
// ════════════════════════════════════════════════════════════════

bool EncodingConverter::convertFirstChunk(const char* srcPath, char* outBuf,
                                           uint32_t outCapacity,
                                           uint32_t* outSize) {
    FILE* f = fopen(srcPath, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open source: %s", srcPath);
        return false;
    }

    // 读取首块 GBK 数据
    uint32_t readSize = FIRST_CHUNK_SRC_SIZE;
    char* srcBuf = static_cast<char*>(
        heap_caps_malloc(readSize, MALLOC_CAP_SPIRAM));
    if (!srcBuf) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to allocate src buffer for first chunk");
        return false;
    }

    size_t bytesRead = fread(srcBuf, 1, readSize, f);
    fclose(f);

    if (bytesRead == 0) {
        heap_caps_free(srcBuf);
        return false;
    }

    // 回退到 GBK 双字节字符边界（避免截断）
    uint32_t safeLen = static_cast<uint32_t>(bytesRead);
    if (safeLen > 0) {
        // GBK 首字节 0x81-0xFE，次字节 0x40-0x7E/0x80-0xFE（次字节与首字节范围重叠）。
        // 从末尾向前数连续 >=0x81 的字节：奇数个 = 末尾有孤立首字节需截掉。
        int highCount = 0;
        for (int i = static_cast<int>(safeLen) - 1;
             i >= 0 && static_cast<uint8_t>(srcBuf[i]) >= 0x81; i--) {
            highCount++;
        }
        if (highCount % 2 == 1) {
            safeLen--;
        }
    }

    // 转换 GBK -> UTF-8
    size_t dstLen = outCapacity;
    esp_err_t err = text_encoding_gbk_to_utf8(srcBuf, safeLen, outBuf, &dstLen);
    heap_caps_free(srcBuf);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "First chunk conversion failed: %d", (int)err);
        return false;
    }

    *outSize = static_cast<uint32_t>(dstLen);
    ESP_LOGI(TAG, "First chunk: %lu GBK -> %lu UTF-8 bytes",
             (unsigned long)safeLen, (unsigned long)dstLen);
    return true;
}

// ════════════════════════════════════════════════════════════════
//  后台转换
// ════════════════════════════════════════════════════════════════

bool EncodingConverter::startBackgroundConversion(const char* srcPath,
                                                   const char* dstPath,
                                                   uint32_t srcFileSize,
                                                   ink::TextSource* owner) {
    strncpy(srcPath_, srcPath, sizeof(srcPath_) - 1);
    strncpy(dstPath_, dstPath, sizeof(dstPath_) - 1);
    srcFileSize_ = srcFileSize;
    owner_ = owner;
    stopRequested_ = false;
    complete_ = false;

    BaseType_t ret = xTaskCreatePinnedToCore(
        taskFunc, "enc_conv", 8192, this,
        tskIDLE_PRIORITY + 2, &taskHandle_, 1);

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create conversion task");
        return false;
    }

    ESP_LOGI(TAG, "Background conversion started: %s -> %s", srcPath, dstPath);
    return true;
}

void EncodingConverter::stop() {
    if (!taskHandle_) return;

    stopRequested_ = true;

    // 等待 task 退出（最多 10 秒）
    for (int i = 0; i < 100 && !complete_; i++) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (!complete_) {
        // task 未能正常退出，强制删除
        ESP_LOGW(TAG, "Conversion task did not exit cleanly, forcing delete");
        vTaskDelete(taskHandle_);
    }
    taskHandle_ = nullptr;
}

void EncodingConverter::taskFunc(void* param) {
    auto* self = static_cast<EncodingConverter*>(param);
    self->doConversion();
    self->taskHandle_ = nullptr;
    vTaskDelete(nullptr);
}

void EncodingConverter::doConversion() {
    FILE* srcFile = fopen(srcPath_, "rb");
    if (!srcFile) {
        ESP_LOGE(TAG, "BG: Failed to open source: %s", srcPath_);
        complete_ = true;
        return;
    }

    FILE* dstFile = fopen(dstPath_, "wb");
    if (!dstFile) {
        ESP_LOGE(TAG, "BG: Failed to create output: %s", dstPath_);
        fclose(srcFile);
        complete_ = true;
        return;
    }

    // 分配转换缓冲区（GBK 输入 + UTF-8 输出）
    char* srcBuf = static_cast<char*>(
        heap_caps_malloc(CONVERT_BLOCK_SIZE, MALLOC_CAP_SPIRAM));
    // UTF-8 最坏情况：每个 GBK 字节 → 3 字节 UTF-8
    uint32_t dstBufSize = CONVERT_BLOCK_SIZE * 3 / 2 + 16;
    char* dstBuf = static_cast<char*>(
        heap_caps_malloc(dstBufSize, MALLOC_CAP_SPIRAM));

    if (!srcBuf || !dstBuf) {
        ESP_LOGE(TAG, "BG: Failed to allocate conversion buffers");
        if (srcBuf) heap_caps_free(srcBuf);
        if (dstBuf) heap_caps_free(dstBuf);
        fclose(srcFile);
        fclose(dstFile);
        complete_ = true;
        return;
    }

    uint32_t totalWritten = 0;
    char pendingByte = 0;
    bool hasPending = false;

    while (!stopRequested_) {
        // 如果上一块有未完成的 GBK 双字节首字节，先放到缓冲区开头
        uint32_t prefixLen = 0;
        if (hasPending) {
            srcBuf[0] = pendingByte;
            prefixLen = 1;
            hasPending = false;
        }

        size_t bytesRead = fread(srcBuf + prefixLen, 1,
                                  CONVERT_BLOCK_SIZE - prefixLen, srcFile);
        if (bytesRead == 0 && prefixLen == 0) break;

        uint32_t blockLen = static_cast<uint32_t>(bytesRead) + prefixLen;

        // 检查末尾是否截断 GBK 双字节字符
        // GBK 次字节范围 0x80-0xFE 与首字节 0x81-0xFE 重叠，不能只看末字节。
        // 从末尾向前数连续 >=0x81 的字节：奇数个 = 末尾有孤立首字节。
        if (blockLen > 0) {
            int highCount = 0;
            for (int i = static_cast<int>(blockLen) - 1;
                 i >= 0 && static_cast<uint8_t>(srcBuf[i]) >= 0x81; i--) {
                highCount++;
            }
            if (highCount % 2 == 1) {
                pendingByte = srcBuf[blockLen - 1];
                hasPending = true;
                blockLen--;
            }
        }

        if (blockLen == 0) continue;

        // 转换这一块
        size_t outLen = dstBufSize;
        esp_err_t err = text_encoding_gbk_to_utf8(srcBuf, blockLen,
                                                    dstBuf, &outLen);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "BG: Conversion error at offset ~%lu",
                     (unsigned long)totalWritten);
            break;
        }

        // 写入输出文件
        size_t written = fwrite(dstBuf, 1, outLen, dstFile);
        if (written != outLen) {
            ESP_LOGE(TAG, "BG: Write error");
            break;
        }

        totalWritten += static_cast<uint32_t>(written);

        // 更新 TextSource 的可用大小
        if (owner_) {
            owner_->updateAvailableSize(totalWritten);
        }

        // 让出 CPU，确保 IDLE task 能喂看门狗
        vTaskDelay(1);
    }

    if (!stopRequested_) {
        // 写入完成标记 "DONE"
        fwrite("DONE", 1, 4, dstFile);
        fflush(dstFile);

        ESP_LOGI(TAG, "BG: Conversion complete: %lu bytes UTF-8",
                 (unsigned long)totalWritten);
    }

    heap_caps_free(srcBuf);
    heap_caps_free(dstBuf);
    fclose(srcFile);
    fclose(dstFile);

    if (!stopRequested_ && owner_) {
        owner_->onConversionComplete(totalWritten);
    }

    complete_ = true;
}
