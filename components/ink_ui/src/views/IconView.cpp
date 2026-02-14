/**
 * @file IconView.cpp
 * @brief IconView 实现 — 32x32 4bpp 图标渲染。
 */

#include "ink_ui/views/IconView.h"
#include "ink_ui/core/Canvas.h"

namespace ink {

IconView::IconView() {
    setRefreshHint(RefreshHint::Fast);
}

void IconView::setIcon(const uint8_t* data) {
    iconData_ = data;
    setNeedsDisplay();
}

void IconView::setTintColor(uint8_t color) {
    tintColor_ = color;
    setNeedsDisplay();
}

void IconView::onDraw(Canvas& canvas) {
    if (!iconData_) return;

    // 居中于 frame
    int x = (frame().w - kIconSize) / 2;
    int y = (frame().h - kIconSize) / 2;

    canvas.drawBitmapFg(iconData_, x, y, kIconSize, kIconSize, tintColor_);
}

Size IconView::intrinsicSize() const {
    return {kIconSize, kIconSize};
}

} // namespace ink
