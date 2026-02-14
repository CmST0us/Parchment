/**
 * @file SeparatorView.cpp
 * @brief SeparatorView 实现 — 水平/垂直分隔线。
 */

#include "ink_ui/views/SeparatorView.h"
#include "ink_ui/core/Canvas.h"

namespace ink {

SeparatorView::SeparatorView(Orientation orientation)
    : orientation_(orientation) {
    setRefreshHint(RefreshHint::Fast);
}

void SeparatorView::setOrientation(Orientation orientation) {
    orientation_ = orientation;
    setNeedsDisplay();
}

void SeparatorView::setLineColor(uint8_t color) {
    lineColor_ = color;
    setNeedsDisplay();
}

void SeparatorView::setThickness(int thickness) {
    thickness_ = thickness;
    setNeedsDisplay();
}

void SeparatorView::onDraw(Canvas& canvas) {
    int w = frame().w;
    int h = frame().h;

    if (orientation_ == Orientation::Horizontal) {
        int y = (h - thickness_) / 2;
        for (int t = 0; t < thickness_; t++) {
            canvas.drawHLine(0, y + t, w, lineColor_);
        }
    } else {
        int x = (w - thickness_) / 2;
        for (int t = 0; t < thickness_; t++) {
            canvas.drawVLine(x + t, 0, h, lineColor_);
        }
    }
}

Size SeparatorView::intrinsicSize() const {
    if (orientation_ == Orientation::Horizontal) {
        return {-1, thickness_};
    }
    return {thickness_, -1};
}

} // namespace ink
