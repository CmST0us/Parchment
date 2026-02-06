## ADDED Requirements

### Requirement: 脏区域标记
系统 SHALL 提供 `ui_render_mark_dirty(int x, int y, int w, int h)` 函数，标记需要刷新的区域。多次调用 SHALL 将区域合并为一个包围矩形（bounding box）。

#### Scenario: 单个脏区域
- **WHEN** 调用 `ui_render_mark_dirty(10, 20, 100, 50)`
- **THEN** 脏区域 SHALL 为 (10, 20, 100, 50)

#### Scenario: 多个脏区域合并
- **WHEN** 依次调用 `ui_render_mark_dirty(10, 20, 100, 50)` 和 `ui_render_mark_dirty(200, 300, 80, 40)`
- **THEN** 合并后的脏区域 SHALL 为 (10, 20, 270, 320)

### Requirement: 全屏脏标记
系统 SHALL 提供 `ui_render_mark_full_dirty()` 函数，将整个屏幕标记为脏区域，强制 GC16 全屏刷新。

#### Scenario: 全屏标记
- **WHEN** 调用 `ui_render_mark_full_dirty()`
- **THEN** 脏区域 SHALL 覆盖整个屏幕 (0, 0, 540, 960)，且刷新模式 SHALL 为全屏刷新

### Requirement: 脏区域查询
系统 SHALL 提供 `ui_render_is_dirty()` 函数，返回是否存在待刷新区域。

#### Scenario: 无脏区域
- **WHEN** 未调用过 mark_dirty 或已完成刷新
- **THEN** `ui_render_is_dirty()` SHALL 返回 false

#### Scenario: 有脏区域
- **WHEN** 调用 mark_dirty 后未刷新
- **THEN** `ui_render_is_dirty()` SHALL 返回 true

### Requirement: 渲染刷新执行
系统 SHALL 提供 `ui_render_flush()` 函数，根据当前脏区域执行屏幕刷新：
- 如果脏区域面积超过屏幕面积的 60%，SHALL 调用 `epd_driver_update_screen()` 执行全屏刷新
- 否则 SHALL 调用 `epd_driver_update_area()` 执行局部刷新
- 刷新完成后 SHALL 清除脏区域状态

#### Scenario: 小区域局部刷新
- **WHEN** 脏区域为 (100, 200, 200, 100)，面积 20000 < 屏幕面积 60%
- **THEN** SHALL 调用 `epd_driver_update_area(100, 200, 200, 100)` 进行局部刷新

#### Scenario: 大区域退化为全屏
- **WHEN** 脏区域面积超过屏幕面积 60%
- **THEN** SHALL 调用 `epd_driver_update_screen()` 进行全屏刷新

#### Scenario: 无脏区域时不刷新
- **WHEN** 没有脏区域时调用 `ui_render_flush()`
- **THEN** SHALL 不调用任何刷新函数

### Requirement: UI 主循环
系统 SHALL 提供 `ui_core_run()` 函数，启动 UI 主循环任务。主循环 SHALL：
1. 从事件队列阻塞等待事件（超时 30 秒）
2. 将事件分发给当前页面的 `on_event` 回调
3. 如果存在脏区域，调用当前页面的 `on_render` 回调，然后执行 `ui_render_flush()`

#### Scenario: 事件驱动渲染
- **WHEN** 事件到达且页面处理后标记了脏区域
- **THEN** SHALL 调用 on_render 回调后刷新屏幕

#### Scenario: 无事件时不渲染
- **WHEN** 30 秒内无事件
- **THEN** SHALL 不调用 on_render，不刷新屏幕

### Requirement: UI 初始化入口
系统 SHALL 提供 `ui_core_init()` 函数，按顺序初始化事件系统、页面栈和渲染状态。

#### Scenario: 完整初始化
- **WHEN** 调用 `ui_core_init()`
- **THEN** 事件队列、页面栈 SHALL 已就绪，返回 `ESP_OK`
