/**
 * @file FlexLayout.h
 * @brief FlexBox 布局类型定义（仅类型，算法在后续 change 中实现）。
 */

#pragma once

#include "ink_ui/core/Geometry.h"

namespace ink {

/// FlexBox 排列方向
enum class FlexDirection {
    Column,     ///< 纵向排列（默认）
    Row,        ///< 横向排列
};

/// 对齐方式（交叉轴）
enum class Align {
    Auto,       ///< 继承父 View 的 alignItems
    Start,
    Center,
    End,
    Stretch,
};

/// 对齐方式（主轴）
enum class Justify {
    Start,
    Center,
    End,
    SpaceBetween,
    SpaceAround,
};

/// FlexBox 容器样式
struct FlexStyle {
    FlexDirection direction = FlexDirection::Column;
    Align alignItems = Align::Start;
    Justify justifyContent = Justify::Start;
    int gap = 0;            ///< 子 View 之间的间距
    Insets padding = {};    ///< 内边距
};

} // namespace ink
