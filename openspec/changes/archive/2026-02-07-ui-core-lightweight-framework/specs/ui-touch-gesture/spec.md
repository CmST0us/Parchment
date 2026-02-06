## ADDED Requirements

### Requirement: 触摸中断配置
系统 SHALL 将 GT911 INT 引脚 (GPIO 48) 配置为下降沿中断（`GPIO_INTR_NEGEDGE`）。中断触发时 SHALL 通过信号量通知触摸任务。

#### Scenario: 中断唤醒触摸任务
- **WHEN** GT911 有触摸数据就绪并拉低 INT 引脚
- **THEN** 中断 ISR SHALL 释放信号量，触摸任务 SHALL 被唤醒

### Requirement: 触摸任务启动
系统 SHALL 提供 `ui_touch_start()` 函数，创建 FreeRTOS 触摸任务。该任务 SHALL：
- 空闲时阻塞等待 INT 中断信号量（不消耗 CPU）
- 被中断唤醒后读取 GT911 触摸数据
- 手指按下期间，切换为 20ms 间隔轮询以持续追踪手指移动
- 手指释放后，重新回到中断等待模式

任务优先级 SHALL 高于 UI 主任务。

#### Scenario: 启动触摸任务
- **WHEN** 调用 `ui_touch_start()`
- **THEN** SHALL 创建触摸任务、配置 INT 中断，返回 `ESP_OK`

#### Scenario: 无触摸时低功耗
- **WHEN** 无用户触摸
- **THEN** 触摸任务 SHALL 阻塞在信号量上，不消耗 CPU 时间

### Requirement: 触摸任务停止
系统 SHALL 提供 `ui_touch_stop()` 函数，禁用 INT 中断并销毁触摸任务。

#### Scenario: 停止已运行的任务
- **WHEN** 触摸任务正在运行时调用 `ui_touch_stop()`
- **THEN** INT 中断 SHALL 被禁用，任务 SHALL 被安全销毁

### Requirement: 坐标旋转映射
触摸任务 SHALL 将 GT911 的物理坐标转换为竖屏逻辑坐标（x∈[0,539], y∈[0,959]），所有发送到事件队列的触摸事件 SHALL 使用逻辑坐标。

#### Scenario: 物理坐标到逻辑坐标映射
- **WHEN** GT911 报告物理触摸点
- **THEN** 事件中的坐标 SHALL 已映射为竖屏逻辑坐标

### Requirement: 点击手势识别
触摸任务 SHALL 识别点击手势：按下到释放的时间 < 500ms 且移动距离 < 20px。识别后 SHALL 向事件队列发送 `UI_EVENT_TOUCH_TAP` 事件，坐标为按下位置。

#### Scenario: 快速点击
- **WHEN** 用户在一个位置按下并在 200ms 内释放，移动不超过 10px
- **THEN** SHALL 产生一个 `UI_EVENT_TOUCH_TAP` 事件

#### Scenario: 移动过大不算点击
- **WHEN** 用户按下后移动 30px 再释放
- **THEN** SHALL 不产生 `UI_EVENT_TOUCH_TAP` 事件

### Requirement: 滑动手势识别
触摸任务 SHALL 识别滑动手势：按下到释放的移动距离 > 40px。根据主方向（水平/垂直位移较大者），SHALL 发送对应方向事件：
- 水平方向：`UI_EVENT_TOUCH_SWIPE_LEFT` 或 `UI_EVENT_TOUCH_SWIPE_RIGHT`
- 垂直方向：`UI_EVENT_TOUCH_SWIPE_UP` 或 `UI_EVENT_TOUCH_SWIPE_DOWN`

坐标 SHALL 为按下位置。

#### Scenario: 向左滑动
- **WHEN** 用户从 (400,500) 按下，移动到 (200,510) 后释放
- **THEN** SHALL 产生 `UI_EVENT_TOUCH_SWIPE_LEFT` 事件，坐标为 (400,500)

#### Scenario: 向下滑动
- **WHEN** 用户从 (270,100) 按下，移动到 (280,300) 后释放
- **THEN** SHALL 产生 `UI_EVENT_TOUCH_SWIPE_DOWN` 事件

### Requirement: 长按手势识别
触摸任务 SHALL 识别长按手势：持续按下超过 800ms 且移动距离 < 20px。识别后 SHALL 立即发送 `UI_EVENT_TOUCH_LONG_PRESS` 事件（不等待释放），坐标为按下位置。

#### Scenario: 长按 1 秒
- **WHEN** 用户在一个位置按下并持续 1000ms 不释放
- **THEN** SHALL 在 800ms 时产生 `UI_EVENT_TOUCH_LONG_PRESS` 事件

#### Scenario: 长按后释放不产生点击
- **WHEN** 长按事件已触发后用户释放手指
- **THEN** SHALL 不再产生 `UI_EVENT_TOUCH_TAP` 事件
