/**
 * @file TextLabel.cpp
 * @brief TextLabel 实现 — 单行/多行文字显示。
 */

#include "ink_ui/views/TextLabel.h"
#include "ink_ui/core/Canvas.h"

extern "C" {
#include "font_engine/font_engine.h"
}

#include <cstring>
#include <vector>

namespace ink {

TextLabel::TextLabel() {
    setRefreshHint(RefreshHint::Quality);
}

void TextLabel::setText(const std::string& text) {
    if (text_ == text) return;
    text_ = text;
    setNeedsDisplay();
}

void TextLabel::setFont(font_engine_t* engine, uint8_t fontSize) {
    engine_ = engine;
    fontSize_ = fontSize;
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

int TextLabel::scaledAscender() const {
    if (!engine_ || fontSize_ == 0) return 0;
    return engine_->header.ascender * fontSize_ / engine_->header.font_height;
}

void TextLabel::onDraw(Canvas& canvas) {
    if (!engine_ || fontSize_ == 0 || text_.empty()) return;

    int lineHeight = fontSize_;
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

        // 构造 null-terminated 行字符串
        std::string lineStr(linePtr, lineLen);

        int textWidth = canvas.measureText(engine_, lineStr.c_str(), fontSize_);

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
        int baselineY = startY + i * lineHeight + scaledAscender();
        canvas.drawText(engine_, lineStr.c_str(), x, baselineY, fontSize_, color_);
    }
}

Size TextLabel::intrinsicSize() const {
    if (!engine_ || fontSize_ == 0 || text_.empty()) return {0, 0};

    int lineHeight = fontSize_;

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
                // 通过 font_engine_get_scaled_glyph 逐字符测量
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

                    font_scaled_glyph_t glyph;
                    if (font_engine_get_scaled_glyph(engine_, cp, fontSize_, &glyph)) {
                        w += glyph.advance_x;
                        if (glyph.bitmap) {
                            free(glyph.bitmap);
                        }
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
