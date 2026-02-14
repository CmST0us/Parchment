/**
 * @file ProgressBarView.cpp
 * @brief ProgressBarView 实现 — 水平进度条。
 */

#include "ink_ui/views/ProgressBarView.h"
#include "ink_ui/core/Canvas.h"

namespace ink {

ProgressBarView::ProgressBarView() {
    setRefreshHint(RefreshHint::Quality);
}

void ProgressBarView::setValue(int v) {
    // 裁剪到 [0, 100]
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    if (value_ == v) return;
    value_ = v;
    setNeedsDisplay();
}

void ProgressBarView::setTrackColor(uint8_t color) {
    trackColor_ = color;
    setNeedsDisplay();
}

void ProgressBarView::setFillColor(uint8_t color) {
    fillColor_ = color;
    setNeedsDisplay();
}

void ProgressBarView::setTrackHeight(int height) {
    trackHeight_ = height;
    setNeedsDisplay();
}

void ProgressBarView::onDraw(Canvas& canvas) {
    int w = frame().w;
    int h = frame().h;

    // 轨道竖直居中
    int trackY = (h - trackHeight_) / 2;

    // 绘制轨道背景
    canvas.fillRect({0, trackY, w, trackHeight_}, trackColor_);

    // 绘制填充部分
    if (value_ > 0) {
        int fillW = w * value_ / 100;
        if (fillW > 0) {
            canvas.fillRect({0, trackY, fillW, trackHeight_}, fillColor_);
        }
    }
}

Size ProgressBarView::intrinsicSize() const {
    return {-1, trackHeight_};
}

} // namespace ink
