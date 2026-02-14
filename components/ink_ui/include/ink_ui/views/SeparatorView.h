/**
 * @file SeparatorView.h
 * @brief SeparatorView — 水平/垂直分隔线 View。
 */

#pragma once

#include "ink_ui/core/Canvas.h"
#include "ink_ui/core/View.h"

namespace ink {

/// 方向枚举
enum class Orientation {
    Horizontal,
    Vertical,
};

/// SeparatorView: 水平或垂直分隔线
class SeparatorView : public View {
public:
    explicit SeparatorView(Orientation orientation = Orientation::Horizontal);
    ~SeparatorView() override = default;

    void setOrientation(Orientation orientation);
    void setLineColor(uint8_t color);
    void setThickness(int thickness);

    Orientation orientation() const { return orientation_; }
    uint8_t lineColor() const { return lineColor_; }
    int thickness() const { return thickness_; }

    // ── View 覆写 ──

    void onDraw(Canvas& canvas) override;
    Size intrinsicSize() const override;

private:
    Orientation orientation_;
    uint8_t lineColor_ = Color::Medium;
    int thickness_ = 1;
};

} // namespace ink
