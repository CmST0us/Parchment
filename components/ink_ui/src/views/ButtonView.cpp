/**
 * @file ButtonView.cpp
 * @brief ButtonView 实现 — 可点击按钮。
 */

#include "ink_ui/views/ButtonView.h"
#include "ink_ui/core/Canvas.h"

namespace ink {

ButtonView::ButtonView() {
    setRefreshHint(RefreshHint::Fast);
}

void ButtonView::setLabel(const std::string& label) {
    if (label_ == label) return;
    label_ = label;
    setNeedsDisplay();
}

void ButtonView::setFont(const EpdFont* font) {
    font_ = font;
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
            if (font_ && !label_.empty()) {
                int textW = canvas.measureText(font_, label_.c_str());
                int x = (r.w - textW) / 2;
                int y = (r.h - font_->advance_y) / 2 + font_->ascender;
                canvas.drawText(font_, label_.c_str(), x, y, fgColor);
            }
            break;
        }
        case ButtonStyle::Secondary: {
            uint8_t borderColor = enabled_ ? Color::Black : Color::Medium;
            uint8_t fgColor = enabled_ ? Color::Black : Color::Medium;
            canvas.fillRect(r, Color::White);
            canvas.drawRect(r, borderColor, 2);
            if (font_ && !label_.empty()) {
                int textW = canvas.measureText(font_, label_.c_str());
                int x = (r.w - textW) / 2;
                int y = (r.h - font_->advance_y) / 2 + font_->ascender;
                canvas.drawText(font_, label_.c_str(), x, y, fgColor);
            }
            break;
        }
        case ButtonStyle::Icon: {
            // 透明底，只绘制图标
            if (icon_) {
                uint8_t tint = enabled_ ? Color::Black : Color::Medium;
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
    if (font_ && !label_.empty()) {
        // 手动计算文字宽度（const 方法无法使用 Canvas）
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
            const EpdGlyph* glyph = epd_get_glyph(font_, cp);
            if (glyph) textW += glyph->advance_x;
        }
    }
    return {textW + kPaddingH * 2, kHeight};
}

} // namespace ink
