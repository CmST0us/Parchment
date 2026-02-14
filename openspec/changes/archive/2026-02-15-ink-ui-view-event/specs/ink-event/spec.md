## ADDED Requirements

### Requirement: TouchEvent 类型
`ink::TouchEvent` SHALL 包含以下字段：
- `type`: TouchType 枚举（Down, Move, Up, Tap, LongPress）
- `x`, `y`: 当前触摸坐标（逻辑坐标系）
- `startX`, `startY`: 触摸起始坐标（用于计算移动距离）

#### Scenario: Tap 事件
- **WHEN** GestureRecognizer 识别出 tap 手势
- **THEN** 生成 TouchEvent，type 为 Tap，x/y 为 tap 位置，startX/startY 与 x/y 相同

#### Scenario: Move 事件携带起始坐标
- **WHEN** 手指按下后移动
- **THEN** 生成 TouchEvent，type 为 Move，x/y 为当前位置，startX/startY 为按下时的位置

### Requirement: SwipeEvent 类型
`ink::SwipeEvent` SHALL 包含方向字段 `direction`，类型为 `SwipeDirection` 枚举（Left, Right, Up, Down）。

#### Scenario: 向左滑动
- **WHEN** GestureRecognizer 识别出向左滑动
- **THEN** 生成 SwipeEvent，direction 为 Left

### Requirement: TimerEvent 类型
`ink::TimerEvent` SHALL 包含 `timerId` 字段（int），用于标识是哪个定时器触发。

#### Scenario: 定时器超时
- **WHEN** 定时器 ID=3 超时
- **THEN** 生成 TimerEvent，timerId 为 3

### Requirement: 统一 Event 类型
`ink::Event` SHALL 使用 tagged union 模式：
- `type`: EventType 枚举（Touch, Swipe, Timer, Custom）
- union 包含 TouchEvent、SwipeEvent、TimerEvent 和 int32_t customParam

Event 的大小 SHALL 为固定值，可通过 FreeRTOS 队列按值传递。

#### Scenario: 创建触摸事件
- **WHEN** 构造 Event，type 设为 Touch，union 的 touch 字段填入 TouchEvent 数据
- **THEN** 可通过 `event.type == EventType::Touch` 判断类型，通过 `event.touch` 访问数据

#### Scenario: 队列传递
- **WHEN** Event 通过 `xQueueSend` 发送
- **THEN** 接收端通过 `xQueueReceive` 获得完整的 Event 值拷贝
