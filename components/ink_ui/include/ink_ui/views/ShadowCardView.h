/**
 * @file ShadowCardView.h
 * @brief 带黑色边框的卡片容器 View。
 *
 * 用于模态视图（Alert, Toast 等），绘制 1px 纯黑边框。
 */

#pragma once

#include "ink_ui/core/View.h"

namespace ink {

/// 带黑色边框的卡片容器
class ShadowCardView : public View {
public:
    ShadowCardView();

    /// 绘制黑色边框 + 白色卡片
    void onDraw(Canvas& canvas) override;

    /// 计算卡片内部区域并对子 View 执行 FlexBox 布局
    void onLayout() override;

    /// 返回内容尺寸 + 边框 padding
    Size intrinsicSize() const override;

    /// 边框宽度（像素）
    static constexpr int kBorderWidth = 1;

    /// 边框外边距（像素），控制卡片与外框之间的间距
    static constexpr int kBorderMargin = 2;

    /// 总边缘占用（边距 + 边框）
    static constexpr int kEdgeTotal = kBorderMargin + kBorderWidth;

private:
    /// 计算卡片内部区域（去除边框 padding）
    Rect cardRect() const;
};

} // namespace ink
