## MODIFIED Requirements

### Requirement: GestureRecognizer 初始化
`ink::GestureRecognizer` SHALL 接受 `TouchDriver&` 引用、`Platform&` 引用和 `QueueHandle` 事件队列句柄构造。不直接依赖 GT911 驱动或 FreeRTOS API。

#### Scenario: 构造并绑定 HAL
- **WHEN** 创建 `GestureRecognizer(touch, platform, eventQueue)`
- **THEN** GestureRecognizer 持有 HAL 引用和事件队列句柄，就绪

### Requirement: 独立任务
`start()` SHALL 通过 `Platform::createTask()` 创建一个高优先级任务，该任务：
- 空闲时通过 `TouchDriver::waitForTouch()` 阻塞等待触摸事件
- 被唤醒后通过 `TouchDriver::read()` 读取触摸数据
- 手指按下期间通过 `Platform::delayMs(20)` 间隔轮询持续追踪手指位置
- 手指释放后回到等待模式

`stop()` SHALL 设置停止标志并等待任务退出。

#### Scenario: 启动手势任务
- **WHEN** 调用 start()
- **THEN** 触摸任务开始运行，阻塞等待触摸输入

#### Scenario: 无触摸时低功耗
- **WHEN** 无用户触摸
- **THEN** 任务阻塞在 `waitForTouch()` 上，不消耗 CPU

### Requirement: 坐标映射
GestureRecognizer SHALL 使用 `TouchDriver::read()` 返回的逻辑坐标（x∈[0,539], y∈[0,959]）。坐标映射由 TouchDriver 实现负责。

#### Scenario: 坐标直通
- **WHEN** TouchDriver 报告触摸点 (270, 480)
- **THEN** 事件中的逻辑坐标为 (270, 480)

### Requirement: Tap 点击识别
GestureRecognizer SHALL 识别 Tap 手势：按下到释放时间 < 500ms 且移动距离 < 20px。识别后 SHALL 通过 `Platform::queueSend()` 向事件队列发送 TouchEvent（type=Tap），坐标为按下位置。

#### Scenario: 快速点击
- **WHEN** 用户在 (200,300) 按下并在 150ms 内释放，移动 < 10px
- **THEN** 产生 TouchEvent{type=Tap, x=200, y=300}

#### Scenario: 移动过大不算 Tap
- **WHEN** 用户按下后移动 30px 再释放
- **THEN** 不产生 Tap 事件

### Requirement: LongPress 长按识别
GestureRecognizer SHALL 识别 LongPress 手势：持续按下超过 800ms 且移动距离 < 20px。识别后 SHALL 立即发送 TouchEvent（type=LongPress），不等待释放。释放后 SHALL 不再产生 Tap 事件。时间通过 `Platform::getTimeUs()` 测量。

#### Scenario: 长按 1 秒
- **WHEN** 用户在 (270,500) 按下并保持 1000ms 不动
- **THEN** 在 800ms 时产生 TouchEvent{type=LongPress, x=270, y=500}

#### Scenario: 长按后释放不产生 Tap
- **WHEN** LongPress 已触发后用户释放
- **THEN** 不产生 Tap 事件

### Requirement: Swipe 滑动识别
GestureRecognizer SHALL 识别 Swipe 手势：按下到释放的移动距离 > 40px。根据主方向（水平/垂直位移较大者）确定方向。识别后 SHALL 向事件队列发送 SwipeEvent，坐标为按下位置。

#### Scenario: 向左滑动
- **WHEN** 用户从 (400,500) 滑动到 (200,510)
- **THEN** 产生 SwipeEvent{direction=Left}

#### Scenario: 向下滑动
- **WHEN** 用户从 (270,100) 滑动到 (280,300)
- **THEN** 产生 SwipeEvent{direction=Down}

### Requirement: Down/Move/Up 原始事件
除了 Tap/LongPress/Swipe 高级手势外，GestureRecognizer SHALL 在按下时发送 TouchEvent{type=Down}，移动时发送 TouchEvent{type=Move}，释放时发送 TouchEvent{type=Up}。这些原始事件 SHALL 在手势判定之前/之中发送。

#### Scenario: 完整触摸序列
- **WHEN** 用户按下、移动、释放
- **THEN** 依次产生 Down、Move（可能多个）、Up 事件，最后可能产生 Tap 或 Swipe
