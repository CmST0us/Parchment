## Why

Wireframe spec 定义了多个页面共享的通用 UI 组件（header 标题栏、按钮、进度条、列表、分隔线）。如果每个页面直接用 `ui_canvas` 原语硬编码这些组件，会导致大量重复代码和不一致的交互行为。需要在实现具体页面之前，提取一层轻量级 widget 抽象。

## What Changes

- 新增 `ui_widget` 模块，提供 5 种可复用 UI 组件的绘制和命中检测 API
- widget_header: 48px 黑底白字标题栏，支持左右图标按钮
- widget_button: 3 种样式按钮（主/次/选中），支持文字和/或图标
- widget_progress: 进度条，可选百分比标签和触摸跳转
- widget_list: 虚拟化可滚动列表，通过回调委托每项绘制
- widget_separator: 分隔线（轻量封装，统一样式）
- 所有 widget 采用纯函数式 API（传入结构体描述 + fb 绘制），不持有内部状态

## Capabilities

### New Capabilities
- `ui-widgets`: 通用 UI widget 组件层，包含 header、button、progress、list、separator 的绘制与交互 API

### Modified Capabilities

（无修改，纯新增）

## Impact

- 新增文件: `components/ui_core/include/ui_widget.h`, `components/ui_core/ui_widget.c`
- 修改文件: `components/ui_core/CMakeLists.txt` (添加 ui_widget.c)
- 依赖: `ui_canvas`（绘图原语）、`ui_icon`（图标绘制）、`ui_font`（文字渲染）
- 无外部依赖变化
