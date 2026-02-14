/**
 * @file FlexLayout.h
 * @brief FlexBox 布局类型定义和算法声明。
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
    Align alignItems = Align::Stretch;
    Justify justifyContent = Justify::Start;
    int gap = 0;            ///< 子 View 之间的间距
    Insets padding = {};    ///< 内边距
};

class View;  // forward declaration

/// 对 container 的可见子 View 执行 FlexBox 布局算法
void flexLayout(View* container);

} // namespace ink
