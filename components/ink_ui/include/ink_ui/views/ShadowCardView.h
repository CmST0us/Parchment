/**
 * @file ShadowCardView.h
 * @brief 带投影阴影的卡片容器 View。
 *
 * 用于模态视图（Alert, Toast 等），在 4bpp 灰度墨水屏上
 * 通过灰度渐变实现投影悬浮效果。
 */

#pragma once

#include "ink_ui/core/View.h"

namespace ink {

/// 带投影阴影的卡片容器
class ShadowCardView : public View {
public:
    ShadowCardView();

    /// 绘制阴影 + 白色卡片
    void onDraw(Canvas& canvas) override;

    /// 计算卡片内部区域并对子 View 执行 FlexBox 布局
    void onLayout() override;

    /// 返回内容尺寸 + 阴影 padding
    Size intrinsicSize() const override;

    /// 阴影扩散距离（像素），模拟 Figma blur=24 + spread=4 的效果
    static constexpr int kShadowSpread = 14;

    /// 阴影偏移（Figma: X=0, Y=0）
    static constexpr int kShadowOffsetX = 0;
    static constexpr int kShadowOffsetY = 0;

    /// 阴影最大深度（距白色的 4bpp 级数，3 = 最深 0xC0）
    static constexpr int kMaxShadowLevels = 3;

private:
    /// 计算卡片内部区域（去除阴影 padding）
    Rect cardRect() const;
};

} // namespace ink
