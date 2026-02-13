## ADDED Requirements

### Requirement: Slider 数据结构
系统 SHALL 提供 `ui_slider_t` 结构体，包含位置尺寸（x, y, w, h）、值域（min_val, max_val）、当前值（value）和步进值（step）。

#### Scenario: 结构体定义
- **WHEN** 包含 `ui_widget.h`
- **THEN** SHALL 可声明 `ui_slider_t` 类型变量，包含 `int x, y, w, h, min_val, max_val, value, step` 字段

### Requirement: Slider 绘制
系统 SHALL 提供 `ui_widget_draw_slider()` 函数，在 framebuffer 上绘制水平滑块。轨道为 LIGHT 色矩形，已填充区域为 BLACK 色，滑块把手为 BLACK 色圆形或矩形。

#### Scenario: 正常绘制
- **WHEN** 调用 `ui_widget_draw_slider(fb, &slider)` 且 slider.value 在 min_val 和 max_val 之间
- **THEN** SHALL 在 (x, y) 位置绘制 w×h 大小的滑块，把手位置对应当前 value

#### Scenario: 边界值绘制
- **WHEN** slider.value 等于 min_val
- **THEN** 把手 SHALL 位于轨道最左端

#### Scenario: 最大值绘制
- **WHEN** slider.value 等于 max_val
- **THEN** 把手 SHALL 位于轨道最右端

### Requirement: Slider 触摸交互
系统 SHALL 提供 `ui_widget_slider_touch()` 函数，将触摸坐标映射到滑块值。触摸区域在 y 方向 ±20px 扩展。返回值 SHALL 按 step 量化到最近的离散值。

#### Scenario: 触摸命中
- **WHEN** 在滑块区域（y 方向 ±20px）内触摸
- **THEN** SHALL 返回触摸 x 坐标对应的值（min_val 到 max_val 之间），按 step 量化

#### Scenario: 触摸未命中
- **WHEN** 在滑块扩展区域外触摸
- **THEN** SHALL 返回 INT_MIN

#### Scenario: step 量化
- **WHEN** step 为 4，min_val 为 0，max_val 为 48，触摸对应原始值为 13
- **THEN** SHALL 返回 12（最近的 step 整数倍）
