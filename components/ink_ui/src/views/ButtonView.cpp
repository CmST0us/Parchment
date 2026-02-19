/**
 * @file ButtonView.cpp
 * @brief ButtonView 实现 — 可点击按钮。
 */

#include "ink_ui/views/ButtonView.h"
#include "ink_ui/core/Canvas.h"

extern "C" {
#include "font_engine/font_engine.h"
}

namespace ink {

ButtonView::ButtonView() {
    setRefreshHint(RefreshHint::Fast);
}

void ButtonView::setLabel(const std::string& label) {
    if (label_ == label) return;
    label_ = label;
    setNeedsDisplay();
}

void ButtonView::setFont(font_engine_t* engine, uint8_t fontSize) {
    engine_ = engine;
    fontSize_ = fontSize;
    setNeedsDisplay();
}

void ButtonView::setIcon(const uint8_t* iconData) {
    icon_ = iconData;
    setNeedsDisplay();
}

void ButtonView::setStyle(ButtonStyle style) {
    style_ = style;
    setNeedsDisplay();
}

void ButtonView::setOnTap(std::function<void()> onTap) {
    onTap_ = std::move(onTap);
}

void ButtonView::setEnabled(bool enabled) {
    if (enabled_ == enabled) return;
    enabled_ = enabled;
    setNeedsDisplay();
}

void ButtonView::onDraw(Canvas& canvas) {
    Rect r = bounds();

    switch (style_) {
        case ButtonStyle::Primary: {
            uint8_t bgColor = enabled_ ? Color::Black : Color::Medium;
            uint8_t fgColor = enabled_ ? Color::White : Color::Light;
            canvas.fillRect(r, bgColor);
            if (engine_ && fontSize_ > 0 && !label_.empty()) {
                int textW = canvas.measureText(engine_, label_.c_str(), fontSize_);
                int x = (r.w - textW) / 2;
                int ascender = engine_->header.ascender * fontSize_ / engine_->header.font_height;
                int y = (r.h - fontSize_) / 2 + ascender;
                canvas.drawText(engine_, label_.c_str(), x, y, fontSize_, fgColor);
            }
            break;
        }
        case ButtonStyle::Secondary: {
            uint8_t borderColor = enabled_ ? Color::Black : Color::Medium;
            uint8_t fgColor = enabled_ ? Color::Black : Color::Medium;
            canvas.fillRect(r, Color::White);
            canvas.drawRect(r, borderColor, 2);
            if (engine_ && fontSize_ > 0 && !label_.empty()) {
                int textW = canvas.measureText(engine_, label_.c_str(), fontSize_);
                int x = (r.w - textW) / 2;
                int ascender = engine_->header.ascender * fontSize_ / engine_->header.font_height;
                int y = (r.h - fontSize_) / 2 + ascender;
                canvas.drawText(engine_, label_.c_str(), x, y, fontSize_, fgColor);
            }
            break;
        }
        case ButtonStyle::Icon: {
            // 只绘制图标，背景由 RenderEngine 的 clear 处理
            if (icon_) {
                // 根据背景色自动选择对比前景色
                uint8_t tint;
                if (enabled_) {
                    uint8_t bg = backgroundColor();
                    tint = (bg != Color::Clear && bg < 0x80) ? Color::White
                                                             : Color::Black;
                } else {
                    tint = Color::Medium;
                }
                int x = (r.w - kIconSize) / 2;
                int y = (r.h - kIconSize) / 2;
                canvas.drawBitmapFg(icon_, x, y, kIconSize, kIconSize, tint);
            }
            break;
        }
    }
}

bool ButtonView::onTouchEvent(const TouchEvent& event) {
    if (event.type == TouchType::Tap) {
        if (!enabled_) return false;
        if (onTap_) {
            onTap_();
        }
        return true;
    }
    return false;
}

Size ButtonView::intrinsicSize() const {
    if (style_ == ButtonStyle::Icon) {
        return {kHeight, kHeight}; // 48x48
    }
    // Primary/Secondary: 文字宽 + padding
    int textW = 0;
    if (engine_ && fontSize_ > 0 && !label_.empty()) {
        const char* p = label_.c_str();
        while (*p) {
            uint32_t cp = 0;
            uint8_t byte = static_cast<uint8_t>(*p);
            int len = 1;
            if (byte < 0x80) {
                cp = byte;
            } else if ((byte & 0xE0) == 0xC0) {
                cp = byte & 0x1F; len = 2;
            } else if ((byte & 0xF0) == 0xE0) {
                cp = byte & 0x0F; len = 3;
            } else if ((byte & 0xF8) == 0xF0) {
                cp = byte & 0x07; len = 4;
            }
            for (int i = 1; i < len && p[i]; i++) {
                cp = (cp << 6) | (static_cast<uint8_t>(p[i]) & 0x3F);
            }
            p += len;
            font_scaled_glyph_t glyph;
            if (font_engine_get_scaled_glyph(engine_, cp, fontSize_, &glyph)) {
                textW += glyph.advance_x;
                if (glyph.bitmap) {
                    free(glyph.bitmap);
                }
            }
        }
    }
    return {textW + kPaddingH * 2, kHeight};
}

} // namespace ink
