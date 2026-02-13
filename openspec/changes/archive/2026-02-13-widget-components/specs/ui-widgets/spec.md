## MODIFIED Requirements

### Requirement: 通用 UI Widget 组件层
系统 SHALL 在 `ui_widget.h` 中提供 header、button、progress、list、separator、slider、selection group、modal dialog 八种可复用组件的绘制和命中检测 API。所有 widget 采用纯函数式接口，不持有内部状态。

#### Scenario: 头文件可用
- **WHEN** 包含 `ui_widget.h`
- **THEN** SHALL 可使用所有 8 种 widget 的 struct 类型和 draw/hit_test 函数

#### Scenario: 新增 widget 函数可调用
- **WHEN** 链接 ui_core 组件
- **THEN** SHALL 可调用 `ui_widget_draw_slider()`、`ui_widget_slider_touch()`、`ui_widget_draw_sel_group()`、`ui_widget_sel_group_hit_test()`、`ui_widget_draw_dialog()`、`ui_widget_dialog_hit_test()`
