/**
 * @file BootLogoView.h
 * @brief 启动画面书本 Logo 自定义 View。
 *
 * 使用 Canvas API 绘制简化的书本图标，100×100px。
 */

#pragma once

#include "ink_ui/core/View.h"

/// 启动画面书本 Logo 视图
class BootLogoView : public ink::View {
public:
    BootLogoView();

    void onDraw(ink::Canvas& canvas) override;
    ink::Size intrinsicSize() const override;

private:
    static constexpr int kLogoSize = 100;
};
