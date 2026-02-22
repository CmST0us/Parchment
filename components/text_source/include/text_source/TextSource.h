/**
 * @file TextSource.h
 * @brief 流式 UTF-8 文本访问层 — 滑动窗口 + GBK 后台转换。
 *
 * 提供统一的 UTF-8 文本按 offset 读取接口。内部使用 512KB PSRAM
 * 滑动缓冲区按需加载文本，GBK 文件通过后台 FreeRTOS task 流式转换
 * 到 SD 卡缓存。
 */

#pragma once

#include <cstdint>
#include <mutex>

// Forward declarations for internal types
struct TextWindow;
struct EncodingConverter;

namespace ink {

/// TextSource 状态
enum class TextSourceState {
    Closed,      ///< 未打开
    Preparing,   ///< 正在检测编码、加载首块
    Available,   ///< 首块已就绪，可顺序阅读
    Converting,  ///< GBK 后台转换进行中（首块已可读）
    Ready,       ///< 全部文本可访问
    Error,       ///< 打开失败
};

/// 文本片段 — 指向内部缓冲区的 UTF-8 文本
struct TextSpan {
    const char* data;   ///< 指向内部缓冲区，下次 read() 或 close() 前有效
    uint32_t length;    ///< 从该 offset 起可用的连续字节数
};

/// 流式 UTF-8 文本源
class TextSource {
public:
    TextSource();
    ~TextSource();

    // 不可拷贝、不可移动
    TextSource(const TextSource&) = delete;
    TextSource& operator=(const TextSource&) = delete;
    TextSource(TextSource&&) = delete;
    TextSource& operator=(TextSource&&) = delete;

    /// 打开文件。快速返回（< 100ms），只加载首块。
    /// @param filePath 原始文件路径（SD 卡上的 TXT 文件）
    /// @param cacheDirPath 缓存目录路径（如 /sdcard/.cache/<hash>）
    /// @return true 如果打开成功
    bool open(const char* filePath, const char* cacheDirPath);

    /// 关闭并释放所有资源（PSRAM 缓冲区、文件句柄、后台 task）
    void close();

    /// 读取指定 offset 处的 UTF-8 文本。
    /// @param offset UTF-8 字节偏移量
    /// @return TextSpan，offset 超出可用范围时返回 {nullptr, 0}
    TextSpan read(uint32_t offset);

    /// 当前状态
    TextSourceState state() const;

    /// UTF-8 文本总大小（字节）。GBK 转换未完成时返回 0。
    uint32_t totalSize() const;

    /// 当前可访问的 UTF-8 字节范围上限
    uint32_t availableSize() const;

    /// 后台准备进度 [0.0, 1.0]
    float progress() const;

    /// 检测到的原始编码
    int detectedEncoding() const;

    /// 原始文件大小（字节）
    uint32_t originalFileSize() const;

private:
    mutable std::mutex mutex_;
    TextSourceState state_ = TextSourceState::Closed;

    // 文件信息
    char filePath_[256] = {};
    char cacheDirPath_[256] = {};
    uint32_t originalFileSize_ = 0;
    int detectedEncoding_ = 0;  // text_encoding_t

    // 文本访问
    TextWindow* window_ = nullptr;

    // GBK 转换
    EncodingConverter* converter_ = nullptr;

    // UTF-8 文件路径（原始 UTF-8 文件或缓存的 text.utf8）
    char utf8FilePath_[256] = {};
    uint32_t totalSize_ = 0;       // UTF-8 总大小
    uint32_t availableSize_ = 0;   // 当前可用大小

    // 首块内存缓冲区（GBK 转换首 128KB 用于立即显示）
    char* firstChunkBuf_ = nullptr;
    uint32_t firstChunkSize_ = 0;
    bool usingFirstChunk_ = true;  // 是否仍从首块内存读取

    /// 检测编码并初始化
    bool initEncoding();

    /// 初始化 UTF-8 文件（直接使用原文件）
    bool initUtf8();

    /// 初始化 GBK 文件（首块转换 + 后台 task）
    bool initGbk();

    /// 检查缓存是否有效
    bool checkCache();

    /// 后台转换完成回调（由 EncodingConverter 调用）
    void onConversionComplete(uint32_t utf8Size);

    /// 更新可用大小（由 EncodingConverter 的后台 task 调用）
    void updateAvailableSize(uint32_t size);

    friend struct ::EncodingConverter;
};

}  // namespace ink
