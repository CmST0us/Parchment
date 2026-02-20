/**
 * @file ReaderContentView.cpp
 * @brief 阅读文本渲染 View 实现 — 统一布局引擎驱动分页和绘制。
 */

#include "views/ReaderContentView.h"

#include "ink_ui/core/Canvas.h"

extern "C" {
#include "esp_log.h"
}

static const char* TAG = "ReaderContent";

// ════════════════════════════════════════════════════════════════
//  构造
// ════════════════════════════════════════════════════════════════

ReaderContentView::ReaderContentView() {
    setBackgroundColor(ink::Color::White);
}

// ════════════════════════════════════════════════════════════════
//  配置 API
// ════════════════════════════════════════════════════════════════

void ReaderContentView::setTextBuffer(const char* buf, uint32_t size) {
    textBuf_ = buf;
    textSize_ = size;
    invalidatePages();
}

void ReaderContentView::setFont(const EpdFont* font) {
    font_ = font;
    invalidatePages();
}

void ReaderContentView::setLineSpacing(uint8_t spacing10x) {
    if (spacing10x < 10) spacing10x = 10;
    if (spacing10x > 25) spacing10x = 25;
    lineSpacing10x_ = spacing10x;
    invalidatePages();
}

void ReaderContentView::setParagraphSpacing(uint8_t px) {
    if (px > 24) px = 24;
    paragraphSpacing_ = px;
    invalidatePages();
}

void ReaderContentView::setTextColor(uint8_t color) {
    textColor_ = color;
    setNeedsDisplay();
}

void ReaderContentView::invalidatePages() {
    pages_.clear();
    setNeedsDisplay();
}

// ════════════════════════════════════════════════════════════════
//  分页 API
// ════════════════════════════════════════════════════════════════

int ReaderContentView::totalPages() const {
    return static_cast<int>(pages_.size());
}

int ReaderContentView::currentPage() const {
    return currentPage_;
}

void ReaderContentView::setCurrentPage(int page) {
    if (pages_.empty()) {
        // 分页未完成，记录目标页码
        initialPage_ = page;
        return;
    }
    int maxPage = static_cast<int>(pages_.size()) - 1;
    if (page < 0) page = 0;
    if (page > maxPage) page = maxPage;
    if (page != currentPage_) {
        currentPage_ = page;
        setNeedsDisplay();
    }
}

uint32_t ReaderContentView::currentPageOffset() const {
    if (pages_.empty()) return 0;
    return pages_[currentPage_];
}

int ReaderContentView::pageForByteOffset(uint32_t offset) const {
    if (pages_.empty()) return 0;
    // 二分查找：找到最后一个 pages_[i] <= offset 的 i
    int lo = 0, hi = static_cast<int>(pages_.size()) - 1;
    int result = 0;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        if (pages_[mid] <= offset) {
            result = mid;
            lo = mid + 1;
        } else {
            hi = mid - 1;
        }
    }
    return result;
}

void ReaderContentView::setInitialPage(int page) {
    initialPage_ = page;
}

void ReaderContentView::setInitialByteOffset(uint32_t offset) {
    initialByteOffset_ = offset;
    hasInitialByteOffset_ = true;
}

// ════════════════════════════════════════════════════════════════
//  统一布局引擎
// ════════════════════════════════════════════════════════════════

int ReaderContentView::lineHeight() const {
    if (!font_) return 0;
    int lh = font_->advance_y * lineSpacing10x_ / 10;
    if (lh < font_->advance_y) lh = font_->advance_y;
    return lh;
}

int ReaderContentView::charWidth(uint32_t codepoint) const {
    if (!font_) return 0;
    const EpdGlyph* glyph = epd_get_glyph(font_, codepoint);
    return glyph ? glyph->advance_x : font_->advance_y / 2;
}

int ReaderContentView::utf8CharLen(uint8_t byte) {
    if (byte < 0x80) return 1;
    if ((byte & 0xE0) == 0xC0) return 2;
    if ((byte & 0xF0) == 0xE0) return 3;
    if ((byte & 0xF8) == 0xF0) return 4;
    return 1;  // 无效字节，跳过 1 字节
}

uint32_t ReaderContentView::decodeCodepoint(const char* p, int len) {
    uint8_t b = static_cast<uint8_t>(p[0]);
    uint32_t cp = 0;
    if (b < 0x80) {
        cp = b;
    } else if ((b & 0xE0) == 0xC0) {
        cp = b & 0x1F;
        for (int i = 1; i < len; i++)
            cp = (cp << 6) | (static_cast<uint8_t>(p[i]) & 0x3F);
    } else if ((b & 0xF0) == 0xE0) {
        cp = b & 0x0F;
        for (int i = 1; i < len; i++)
            cp = (cp << 6) | (static_cast<uint8_t>(p[i]) & 0x3F);
    } else if ((b & 0xF8) == 0xF0) {
        cp = b & 0x07;
        for (int i = 1; i < len; i++)
            cp = (cp << 6) | (static_cast<uint8_t>(p[i]) & 0x3F);
    } else {
        cp = 0xFFFD;  // replacement character
    }
    return cp;
}

ReaderContentView::PageLayout ReaderContentView::layoutPage(
    uint32_t startOffset) const {
    PageLayout result;
    result.endOffset = startOffset;

    if (!font_ || !textBuf_ || startOffset >= textSize_) return result;

    int maxWidth = bounds().w;
    int maxHeight = bounds().h;
    int lh = lineHeight();
    if (lh <= 0) return result;

    int remainingHeight = maxHeight;
    uint32_t offset = startOffset;

    while (offset < textSize_ && remainingHeight >= lh) {
        // 处理 \r\n 或 \n（段落分隔符）— 空行也消耗 lineHeight
        if (textBuf_[offset] == '\n') {
            result.lines.push_back({offset, offset, true});
            offset++;
            remainingHeight -= lh + paragraphSpacing_;
            continue;
        }
        if (textBuf_[offset] == '\r') {
            uint32_t crPos = offset;
            offset++;
            if (offset < textSize_ && textBuf_[offset] == '\n') {
                offset++;
            }
            result.lines.push_back({crPos, crPos, true});
            remainingHeight -= lh + paragraphSpacing_;
            continue;
        }

        // 折行：逐字符测量宽度
        uint32_t lineStart = offset;
        int lineWidth = 0;

        while (offset < textSize_ &&
               textBuf_[offset] != '\n' &&
               textBuf_[offset] != '\r') {
            int cLen = utf8CharLen(static_cast<uint8_t>(textBuf_[offset]));
            if (offset + cLen > textSize_) break;

            uint32_t cp = decodeCodepoint(textBuf_ + offset, cLen);
            int cw = charWidth(cp);

            if (lineWidth + cw > maxWidth && offset > lineStart) {
                break;  // 行满
            }

            lineWidth += cw;
            offset += cLen;
        }

        // 安全措施：至少前进一个字符
        if (offset == lineStart && offset < textSize_) {
            offset += utf8CharLen(static_cast<uint8_t>(textBuf_[offset]));
        }

        // line.end = 文本内容结束位置（不含换行符）
        uint32_t lineEnd = offset;

        // 判断是否段落结尾，跳过换行符
        bool isParagraphEnd = false;
        if (offset < textSize_ && textBuf_[offset] == '\n') {
            isParagraphEnd = true;
            offset++;
        } else if (offset < textSize_ && textBuf_[offset] == '\r') {
            isParagraphEnd = true;
            offset++;
            if (offset < textSize_ && textBuf_[offset] == '\n') {
                offset++;
            }
        }

        result.lines.push_back({lineStart, lineEnd, isParagraphEnd});

        remainingHeight -= lh;
        if (isParagraphEnd) {
            remainingHeight -= paragraphSpacing_;
        }
    }

    result.endOffset = offset;
    return result;
}

// ════════════════════════════════════════════════════════════════
//  分页
// ════════════════════════════════════════════════════════════════

void ReaderContentView::paginate() {
    pages_.clear();
    if (!font_ || !textBuf_ || textSize_ == 0) return;

    uint32_t offset = 0;
    while (offset < textSize_) {
        pages_.push_back(offset);
        PageLayout layout = layoutPage(offset);
        if (layout.endOffset <= offset) {
            // 安全措施：避免死循环
            break;
        }
        offset = layout.endOffset;
    }

    ESP_LOGI(TAG, "Paginated: %d pages (lineHeight=%d, paraSpacing=%d)",
             static_cast<int>(pages_.size()), lineHeight(), (int)paragraphSpacing_);

    // 应用初始目标页码
    if (hasInitialByteOffset_) {
        currentPage_ = pageForByteOffset(initialByteOffset_);
        hasInitialByteOffset_ = false;
        ESP_LOGI(TAG, "Restored to page %d (byte offset %lu)",
                 currentPage_, (unsigned long)initialByteOffset_);
    } else if (initialPage_ >= 0) {
        int maxPage = static_cast<int>(pages_.size()) - 1;
        currentPage_ = (initialPage_ > maxPage) ? maxPage : initialPage_;
        initialPage_ = -1;
        ESP_LOGI(TAG, "Restored to page %d", currentPage_);
    } else {
        // 确保 currentPage_ 在范围内
        int maxPage = static_cast<int>(pages_.size()) - 1;
        if (currentPage_ > maxPage) currentPage_ = maxPage;
        if (currentPage_ < 0) currentPage_ = 0;
    }

    // 通知外部分页完成
    if (onPaginationComplete) {
        onPaginationComplete();
    }
}

// ════════════════════════════════════════════════════════════════
//  渲染
// ════════════════════════════════════════════════════════════════

void ReaderContentView::onDraw(ink::Canvas& canvas) {
    if (!font_ || !textBuf_ || textSize_ == 0) return;

    // 懒分页
    if (pages_.empty()) {
        paginate();
        if (pages_.empty()) return;
    }

    PageLayout layout = layoutPage(pages_[currentPage_]);

    int lh = lineHeight();
    int y = 0;

    for (const auto& line : layout.lines) {
        int len = static_cast<int>(line.end - line.start);
        if (len > 0) {
            int baselineY = y + font_->ascender;
            canvas.drawTextN(font_, textBuf_ + line.start, len,
                             0, baselineY, textColor_);
        }
        y += lh;
        if (line.isParagraphEnd) {
            y += paragraphSpacing_;
        }
    }
}
