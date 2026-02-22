/**
 * @file EncodingConverter.h
 * @brief GBK -> UTF-8 后台流式转换器。
 */

#pragma once

#include <cstdint>
#include <cstdio>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace ink {
class TextSource;
}

/// GBK -> UTF-8 后台流式转换器
struct EncodingConverter {
    static constexpr uint32_t FIRST_CHUNK_SRC_SIZE = 128 * 1024;  // 128KB GBK 源
    static constexpr uint32_t CONVERT_BLOCK_SIZE = 64 * 1024;     // 64KB 转换块

    EncodingConverter();
    ~EncodingConverter();

    // 不可拷贝
    EncodingConverter(const EncodingConverter&) = delete;
    EncodingConverter& operator=(const EncodingConverter&) = delete;

    /// 转换首块到内存缓冲区（同步，快速）
    bool convertFirstChunk(const char* srcPath, char* outBuf,
                           uint32_t outCapacity, uint32_t* outSize);

    /// 启动后台转换 task
    bool startBackgroundConversion(const char* srcPath, const char* dstPath,
                                   uint32_t srcFileSize, ink::TextSource* owner);

    /// 停止后台转换 task
    void stop();

    /// 是否转换完成
    bool isComplete() const { return complete_; }

private:
    TaskHandle_t taskHandle_ = nullptr;
    volatile bool stopRequested_ = false;
    volatile bool complete_ = false;

    // task 参数
    char srcPath_[256] = {};
    char dstPath_[256] = {};
    uint32_t srcFileSize_ = 0;
    ink::TextSource* owner_ = nullptr;

    /// FreeRTOS task 入口
    static void taskFunc(void* param);

    /// 执行流式转换
    void doConversion();
};
