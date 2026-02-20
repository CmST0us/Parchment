/**
 * @file ReaderContentView.h
 * @brief 阅读文本渲染 View — 折行、分页、逐行绘制。
 *
 * 专用于阅读场景，支持可配置行距和段间距。直接使用 Canvas::drawTextN
 * 逐行绘制，绕过 TextLabel。分页在首次 onDraw 时懒执行。
 */

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "ink_ui/core/View.h"

extern "C" {
#include "epdiy.h"
}

/// 阅读文本渲染 View
class ReaderContentView : public ink::View {
public:
    ReaderContentView();

    // ── 配置 API ──

    /// 设置 UTF-8 文本缓冲区（非拥有指针，调用方负责生命周期）
    void setTextBuffer(const char* buf, uint32_t size);

    /// 设置渲染字体
    void setFont(const EpdFont* font);

    /// 设置行距倍数（×10），如 16 表示 1.6 倍。范围 10~25。
    void setLineSpacing(uint8_t spacing10x);

    /// 设置段间距像素数。范围 0~24。
    void setParagraphSpacing(uint8_t px);

    /// 设置文字颜色
    void setTextColor(uint8_t color);

    // ── 分页 API ──

    /// 返回总页数
    int totalPages() const;

    /// 返回当前页码（0-indexed）
    int currentPage() const;

    /// 设置当前页并标记需要重绘
    void setCurrentPage(int page);

    /// 返回当前页的起始字节偏移
    uint32_t currentPageOffset() const;

    /// 根据字节偏移查找对应页码（用于恢复进度）
    int pageForByteOffset(uint32_t offset) const;

    /// 设置初始目标页码（懒分页完成后应用）
    void setInitialPage(int page);

    /// 设置初始目标字节偏移（懒分页完成后转换为页码）
    void setInitialByteOffset(uint32_t offset);

    /// 分页完成后的回调（用于通知 ViewController 更新页脚等）
    std::function<void()> onPaginationComplete;

    // ── 渲染 ──

    void onDraw(ink::Canvas& canvas) override;

private:
    // 文本数据
    const char* textBuf_ = nullptr;
    uint32_t textSize_ = 0;

    // 渲染参数
    const EpdFont* font_ = nullptr;
    uint8_t lineSpacing10x_ = 16;
    uint8_t paragraphSpacing_ = 8;
    uint8_t textColor_ = 0x00;  // Black

    // 分页表: pages_[i] = 第 i 页的起始字节偏移
    std::vector<uint32_t> pages_;
    int currentPage_ = 0;

    // 懒分页：初始目标
    int initialPage_ = -1;          // -1 表示未设置
    uint32_t initialByteOffset_ = 0;
    bool hasInitialByteOffset_ = false;

    /// 一行的布局信息
    struct LineInfo {
        uint32_t start;           ///< 起始字节偏移
        uint32_t end;             ///< 结束字节偏移（不含）
        bool isParagraphEnd;      ///< 本行之后有段间距
    };

    /// 一页的布局结果
    struct PageLayout {
        std::vector<LineInfo> lines;
        uint32_t endOffset;       ///< 下一页起始偏移
    };

    /// 计算行高（像素）
    int lineHeight() const;

    /// 统一布局引擎：对一页进行折行和填充
    PageLayout layoutPage(uint32_t startOffset) const;

    /// 构建完整分页表
    void paginate();

    /// 使分页表失效
    void invalidatePages();

    /// 获取字符宽度
    int charWidth(uint32_t codepoint) const;

    /// 计算 UTF-8 字符字节长度
    static int utf8CharLen(uint8_t byte);

    /// 解码 UTF-8 codepoint
    static uint32_t decodeCodepoint(const char* p, int len);
};
