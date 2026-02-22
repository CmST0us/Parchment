/**
 * @file TextWindow.h
 * @brief PSRAM 512KB 滑动窗口 — 按需从文件加载 UTF-8 文本块。
 */

#pragma once

#include <cstdint>
#include <cstdio>

#include "text_source/TextSource.h"

/// PSRAM 滑动窗口，缓存文件的一个 512KB 区间
struct TextWindow {
    static constexpr uint32_t WINDOW_SIZE = 512 * 1024;  // 512KB

    TextWindow();
    ~TextWindow();

    // 不可拷贝
    TextWindow(const TextWindow&) = delete;
    TextWindow& operator=(const TextWindow&) = delete;

    /// 分配 PSRAM 缓冲区
    bool init();

    /// 释放缓冲区
    void deinit();

    /// 设置文件源并清除内存源
    /// @param fileBaseOffset 文件中有效内容的起始偏移（如 UTF-8 BOM 需跳过 3 字节）
    void setFile(FILE* file, uint32_t fileSize, uint32_t fileBaseOffset = 0);

    /// 设置内存源（用于首块内存缓冲区）
    void setMemory(const char* buf, uint32_t size);

    /// 加载包含 offset 的窗口区间。文件模式下从磁盘读取，内存模式下为 no-op。
    bool load(uint32_t offset);

    /// 获取 offset 处的文本片段
    ink::TextSpan spanAt(uint32_t offset) const;

    /// 检查 offset 是否在当前窗口内
    bool contains(uint32_t offset) const;

    /// 获取当前窗口覆盖的文件大小上限
    uint32_t sourceSize() const;

private:
    char* buffer_ = nullptr;       // PSRAM 缓冲区
    uint32_t bufStart_ = 0;        // 缓冲区对应的文件起始 offset
    uint32_t bufSize_ = 0;         // 缓冲区中有效数据大小

    // 文件源
    FILE* file_ = nullptr;
    uint32_t fileSize_ = 0;        // 有效内容大小（不含 baseOffset）
    uint32_t fileBaseOffset_ = 0;  // 文件中有效内容起始偏移

    // 内存源
    const char* memBuf_ = nullptr;
    uint32_t memSize_ = 0;
    bool usingMemory_ = false;

    /// 对齐到 UTF-8 字符边界（向后扫描跳过 continuation bytes）
    static uint32_t alignUtf8Forward(const char* buf, uint32_t offset,
                                     uint32_t size);
};
