## Why

现有 widget 层（header、button、progress、list、separator）覆盖了基础 UI 元素，但设置页面和字体选择页面需要 slider（滑块）、selection group（多选按钮组）和 modal dialog（模态弹窗）三种交互组件。这些组件是实现 reading-settings 和 font-selector 页面的前置依赖。

## What Changes

- 新增 **slider** widget：水平滑块控件，支持连续值调节（字号、行距、边距等），支持触摸拖拽和点击定位
- 新增 **selection group** widget：互斥多选按钮组（如边距 narrow/medium/wide），支持触摸选择和当前选中状态高亮
- 新增 **modal dialog** widget：模态弹窗容器，半透明遮罩 + 居中内容区域，支持滚动列表和关闭操作

## Capabilities

### New Capabilities
- `widget-slider`: 水平滑块控件的绘制、触摸交互和值计算
- `widget-selection-group`: 互斥多选按钮组的绘制、选中状态管理和触摸命中检测
- `widget-modal-dialog`: 模态弹窗的绘制（遮罩 + 内容区）、触摸事件分发和关闭逻辑

### Modified Capabilities
- `ui-widgets`: 在已有 widget 规格中补充 slider、selection group、modal dialog 的注册（或在新 capability 中独立定义）

## Impact

- **代码**: `components/ui_core/ui_widget.c` 和 `ui_widget.h` 新增 3 种 widget 的 struct、draw、hit-test 函数
- **API**: 新增 `ui_widget_draw_slider()`、`ui_widget_slider_touch()`、`ui_widget_draw_selection_group()`、`ui_widget_selection_group_hit_test()`、`ui_widget_draw_dialog()`、`ui_widget_dialog_hit_test()` 等函数
- **依赖**: 无新增外部依赖，复用现有 ui_canvas、ui_font、ui_icon
- **下游页面**: reading-settings 页面和 font-selector 页面将直接使用这些 widget
