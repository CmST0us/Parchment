/**
 * @file TextLabel.cpp
 * @brief TextLabel 实现 — 单行/多行文字显示。
 */

#include "ink_ui/views/TextLabel.h"
#include "ink_ui/core/Canvas.h"

#include <cstring>
#include <vector>

namespace ink {

TextLabel::TextLabel() {
    setBackgroundColor(Color::Transparent);
    setRefreshHint(RefreshHint::Quality);
}

void TextLabel::setText(const std::string& text) {
    if (text_ == text) return;
    text_ = text;
    setNeedsDisplay();
}

void TextLabel::setFont(const EpdFont* font) {
    font_ = font;
    setNeedsDisplay();
}

void TextLabel::setColor(uint8_t color) {
    color_ = color;
    setNeedsDisplay();
}

void TextLabel::setAlignment(Align alignment) {
    alignment_ = alignment;
    setNeedsDisplay();
}

void TextLabel::setMaxLines(int maxLines) {
    maxLines_ = maxLines;
    setNeedsDisplay();
}

void TextLabel::onDraw(Canvas& canvas) {
    if (!font_ || text_.empty()) return;

    int lineHeight = font_->advance_y;
    if (lineHeight <= 0) return;

    // 分行：按 \n 切割
    std::vector<std::pair<const char*, int>> lines; // (start, byteLen)
    const char* p = text_.c_str();
    const char* lineStart = p;
    while (*p) {
        if (*p == '\n') {
            lines.push_back({lineStart, static_cast<int>(p - lineStart)});
            p++;
            lineStart = p;
        } else {
            p++;
        }
    }
    if (lineStart <= p) {
        lines.push_back({lineStart, static_cast<int>(p - lineStart)});
    }

    // 限制行数
    int maxL = maxLines_;
    if (maxL == 0) maxL = static_cast<int>(lines.size());
    int numLines = static_cast<int>(lines.size());
    if (numLines > maxL) numLines = maxL;

    // 竖直居中：文字块整体居中于 frame 高度
    int totalTextHeight = numLines * lineHeight;
    int startY = (frame().h - totalTextHeight) / 2;

    for (int i = 0; i < numLines; i++) {
        auto& [linePtr, lineLen] = lines[i];
        if (lineLen <= 0) continue;

        // 临时构造 null-terminated 行字符串
        // measureText 需要 null-terminated string
        std::string lineStr(linePtr, lineLen);

        int textWidth = canvas.measureText(font_, lineStr.c_str());

        // 水平对齐
        int x = 0;
        switch (alignment_) {
            case Align::Start:
            case Align::Auto:
            case Align::Stretch:
                x = 0;
                break;
            case Align::Center:
                x = (frame().w - textWidth) / 2;
                break;
            case Align::End:
                x = frame().w - textWidth;
                break;
        }

        // 基线 y = startY + ascender（从行顶部到基线的距离）
        int baselineY = startY + i * lineHeight + font_->ascender;
        canvas.drawTextN(font_, linePtr, lineLen, x, baselineY, color_);
    }
}

Size TextLabel::intrinsicSize() const {
    if (!font_ || text_.empty()) return {0, 0};

    int lineHeight = font_->advance_y;

    // 计算行数和最大宽度
    int numLines = 1;
    const char* p = text_.c_str();
    while (*p) {
        if (*p == '\n') numLines++;
        p++;
    }

    int maxL = maxLines_;
    if (maxL > 0 && numLines > maxL) numLines = maxL;

    // 测量宽度：取最宽的行
    int maxWidth = 0;
    const char* lineStart = text_.c_str();
    int lineIdx = 0;
    p = text_.c_str();
    while (true) {
        if (*p == '\n' || *p == '\0') {
            if (lineIdx < numLines) {
                std::string lineStr(lineStart, p - lineStart);
                // measureText 需要一个 Canvas 实例，但 intrinsicSize 是 const 的
                // 我们用 measureText 的静态实现：遍历 glyph advance_x
                int w = 0;
                const char* tp = lineStr.c_str();
                while (*tp) {
                    // UTF-8 解码
                    uint32_t cp = 0;
                    uint8_t byte = static_cast<uint8_t>(*tp);
                    int len = 1;
                    if (byte < 0x80) {
                        cp = byte;
                    } else if ((byte & 0xE0) == 0xC0) {
                        cp = byte & 0x1F;
                        len = 2;
                    } else if ((byte & 0xF0) == 0xE0) {
                        cp = byte & 0x0F;
                        len = 3;
                    } else if ((byte & 0xF8) == 0xF0) {
                        cp = byte & 0x07;
                        len = 4;
                    }
                    for (int i = 1; i < len && tp[i]; i++) {
                        cp = (cp << 6) | (static_cast<uint8_t>(tp[i]) & 0x3F);
                    }
                    tp += len;

                    const EpdGlyph* glyph = epd_get_glyph(font_, cp);
                    if (glyph) {
                        w += glyph->advance_x;
                    }
                }
                if (w > maxWidth) maxWidth = w;
            }
            lineIdx++;
            if (*p == '\0') break;
            lineStart = p + 1;
        }
        p++;
    }

    return {maxWidth, numLines * lineHeight};
}

} // namespace ink
