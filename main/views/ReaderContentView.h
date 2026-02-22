/**
 * @file ReaderContentView.h
 * @brief 阅读文本渲染 View — 折行、分页、逐行绘制。
 *
 * 专用于阅读场景，支持可配置行距和段间距。直接使用 Canvas::drawTextN
 * 逐行绘制，绕过 TextLabel。分页由后台 FreeRTOS task 异步构建。
 */

#pragma once

#include <cstdint>
#include <functional>

#include "ink_ui/core/View.h"
#include "views/PageIndex.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

extern "C" {
#include "epdiy.h"
}

namespace ink {
class TextSource;
struct TextSpan;
}

/// 阅读文本渲染 View
class ReaderContentView : public ink::View {
public:
    ReaderContentView();
    ~ReaderContentView() override;

    // ── 配置 API ──

    /// 设置文本数据源（非拥有指针，调用方负责生命周期）。替代原 setTextBuffer()。
    void setTextSource(ink::TextSource* source);

    /// 设置渲染字体
    void setFont(const EpdFont* font);

    /// 设置行距倍数（x10），如 16 表示 1.6 倍。范围 10~25。
    void setLineSpacing(uint8_t spacing10x);

    /// 设置段间距像素数。范围 0~24。
    void setParagraphSpacing(uint8_t px);

    /// 设置文字颜色
    void setTextColor(uint8_t color);

    /// 设置缓存目录路径（用于 PageIndex 的 pages.idx 文件）
    void setCacheDir(const char* cacheDirPath);

    // ── 分页 API ──

    /// 返回总页数。索引未完成返回 -1。
    int totalPages() const;

    /// 返回当前页码（0-indexed）
    int currentPage() const;

    /// 设置当前页并标记需要重绘
    void setCurrentPage(int page);

    /// 返回当前页的起始字节偏移
    uint32_t currentPageOffset() const;

    /// 根据字节偏移查找对应页码。PageIndex 有数据时二分搜索，否则返回 -1。
    int pageForByteOffset(uint32_t offset) const;

    /// 设置初始目标字节偏移（分页完成后转换为页码）
    void setInitialByteOffset(uint32_t offset);

    /// 页索引是否构建完成
    bool isPageIndexComplete() const;

    /// 页索引构建进度 [0.0, 1.0]
    float pageIndexProgress() const;

    /// 设置状态变化回调（后台 task 触发，用于更新页脚）
    void setStatusCallback(std::function<void()> callback);

    // ── 渲染 ──

    void onDraw(ink::Canvas& canvas) override;

private:
    // 文本数据源
    ink::TextSource* textSource_ = nullptr;

    // 渲染参数
    const EpdFont* font_ = nullptr;
    uint8_t lineSpacing10x_ = 16;
    uint8_t paragraphSpacing_ = 8;
    uint8_t textColor_ = 0x00;  // Black

    // 页索引
    PageIndex pageIndex_;
    int currentPage_ = 0;
    char cacheDirPath_[256] = {};

    // 后台分页 task
    TaskHandle_t paginateTask_ = nullptr;
    volatile bool paginateStopRequested_ = false;
    volatile bool paginateComplete_ = false;
    bool paginateStarted_ = false;

    // 缓存的 viewport 尺寸（后台 task 使用，避免从 View::bounds() 读取）
    int cachedViewportW_ = 0;
    int cachedViewportH_ = 0;

    // 初始目标
    uint32_t initialByteOffset_ = 0;
    bool hasInitialByteOffset_ = false;

    // 状态回调
    std::function<void()> statusCallback_;

    /// 一行的布局信息
    struct LineInfo {
        uint32_t start;           ///< 起始字节偏移
        uint32_t end;             ///< 结束字节偏移（不含）
        bool isParagraphEnd;      ///< 本行之后有段间距
    };

    /// 一页的布局结果
    struct PageLayout {
        LineInfo lines[64];       ///< 固定数组避免 heap 分配
        int lineCount = 0;
        uint32_t endOffset;       ///< 下一页起始偏移
    };

    /// 计算行高（像素）
    int lineHeight() const;

    /// 统一布局引擎：对一页进行折行和填充
    PageLayout layoutPage(uint32_t startOffset);

    /// 使页索引失效并停止后台 task
    void invalidatePages();

    /// 尝试加载缓存或启动后台分页
    void ensurePagination();

    /// 启动后台分页 task
    void startPaginateTask();

    /// 停止后台分页 task
    void stopPaginateTask();

    /// 计算排版参数哈希值
    uint32_t paramsHash() const;

    /// 获取 pages.idx 路径
    void getPagesIdxPath(char* buf, int bufSize) const;

    /// BMP 范围 (U+0000-U+FFFF) 的 glyph advance_x 缓存，PSRAM 分配
    uint8_t* glyphWidthCache_ = nullptr;

    /// 构建宽度缓存表
    void buildWidthCache();

    /// 释放宽度缓存表
    void freeWidthCache();

    /// 获取字符宽度（缓存命中为 O(1) 数组访问）
    int charWidth(uint32_t codepoint) const;

    /// 计算 UTF-8 字符字节长度
    static int utf8CharLen(uint8_t byte);

    /// 解码 UTF-8 codepoint
    static uint32_t decodeCodepoint(const char* p, int len);

    /// 后台分页 task 入口
    static void paginateTaskFunc(void* param);

    /// 执行后台分页
    void doPaginate();
};
