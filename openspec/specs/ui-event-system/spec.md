## ADDED Requirements

### Requirement: 事件类型定义
系统 SHALL 定义统一的事件类型枚举 `ui_event_type_t`，至少包含以下类型：
- `UI_EVENT_TOUCH_TAP` — 点击
- `UI_EVENT_TOUCH_SWIPE_LEFT` — 左滑
- `UI_EVENT_TOUCH_SWIPE_RIGHT` — 右滑
- `UI_EVENT_TOUCH_SWIPE_UP` — 上滑
- `UI_EVENT_TOUCH_SWIPE_DOWN` — 下滑
- `UI_EVENT_TOUCH_LONG_PRESS` — 长按
- `UI_EVENT_TIMER` — 定时器
- `UI_EVENT_CUSTOM` — 自定义

#### Scenario: 事件类型枚举完整性
- **WHEN** 代码中引用任一上述事件类型
- **THEN** 编译 SHALL 成功且枚举值互不相同

### Requirement: 事件数据结构
系统 SHALL 定义事件结构体 `ui_event_t`，包含以下字段：
- `type` — 事件类型（`ui_event_type_t`）
- `x`, `y` — 触摸坐标（`int16_t`，竖屏逻辑坐标系）
- `data` — 附加数据（`union`，包含 `int32_t param` 和 `void *ptr`）

#### Scenario: 事件结构体可携带触摸坐标
- **WHEN** 创建一个 `ui_event_t` 并设置 x=100, y=200
- **THEN** 该事件的 x 字段 SHALL 为 100，y 字段 SHALL 为 200

### Requirement: 事件队列初始化
系统 SHALL 提供 `ui_event_init()` 函数，创建容量为 16 的 FreeRTOS 队列用于事件传递。

#### Scenario: 初始化成功
- **WHEN** 调用 `ui_event_init()`
- **THEN** SHALL 返回 `ESP_OK` 且事件队列已就绪

#### Scenario: 重复初始化
- **WHEN** 连续调用两次 `ui_event_init()`
- **THEN** 第二次调用 SHALL 返回 `ESP_OK` 且不创建额外队列

### Requirement: 事件发送
系统 SHALL 提供 `ui_event_send(const ui_event_t *event)` 函数，将事件推入队列。

#### Scenario: 发送事件到队列
- **WHEN** 调用 `ui_event_send()` 传入一个有效事件
- **THEN** 事件 SHALL 被加入队列

#### Scenario: 队列满时发送
- **WHEN** 队列已满时调用 `ui_event_send()`
- **THEN** SHALL 返回 `ESP_ERR_TIMEOUT` 且不阻塞调用者

### Requirement: 事件接收
系统 SHALL 提供 `ui_event_receive(ui_event_t *event, uint32_t timeout_ms)` 函数，从队列中取出事件。

#### Scenario: 有事件时接收
- **WHEN** 队列中有事件且调用 `ui_event_receive()`
- **THEN** SHALL 立即返回 `ESP_OK` 并填充事件数据

#### Scenario: 超时无事件
- **WHEN** 队列为空且调用 `ui_event_receive()` 传入 timeout_ms=1000
- **THEN** SHALL 阻塞最多 1000ms 后返回 `ESP_ERR_TIMEOUT`
