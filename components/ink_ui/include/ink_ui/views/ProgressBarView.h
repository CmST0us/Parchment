/**
 * @file ProgressBarView.h
 * @brief ProgressBarView — 水平进度条 View。
 */

#pragma once

#include "ink_ui/core/Canvas.h"
#include "ink_ui/core/View.h"

namespace ink {

/// ProgressBarView: 灰度进度条
class ProgressBarView : public View {
public:
    ProgressBarView();
    ~ProgressBarView() override = default;

    /// 设置进度值（0-100，超出范围自动裁剪；相同值不触发重绘）
    void setValue(int v);

    /// 设置轨道颜色
    void setTrackColor(uint8_t color);

    /// 设置填充颜色
    void setFillColor(uint8_t color);

    /// 设置轨道高度
    void setTrackHeight(int height);

    int value() const { return value_; }
    uint8_t trackColor() const { return trackColor_; }
    uint8_t fillColor() const { return fillColor_; }
    int trackHeight() const { return trackHeight_; }

    // ── View 覆写 ──

    void onDraw(Canvas& canvas) override;
    Size intrinsicSize() const override;

private:
    int value_ = 0;
    uint8_t trackColor_ = Color::Light;
    uint8_t fillColor_ = Color::Black;
    int trackHeight_ = 8;
};

} // namespace ink
