## MODIFIED Requirements

### Requirement: GestureRecognizer 初始化
`ink::GestureRecognizer` SHALL 接受 FreeRTOS 事件队列句柄构造。不再配置 GPIO 中断，改为使用 M5.Touch 轮询模式。

#### Scenario: 构造
- **WHEN** 创建 GestureRecognizer 并传入事件队列
- **THEN** GestureRecognizer 就绪，无 GPIO 中断配置

### Requirement: 独立 FreeRTOS 任务
`start()` SHALL 创建一个 FreeRTOS 任务（优先级高于 Application 主循环），该任务：
- 以 20ms 间隔轮询 `M5.Touch.getDetail()` 获取触摸状态
- 根据触摸状态（pressed/released）驱动手势状态机
- 手指未按下时仍以 20ms 轮询（M5.Touch 内部已有中断优化，无触摸时 I2C 读取开销极低）

`stop()` SHALL 销毁任务。

#### Scenario: 启动手势任务
- **WHEN** 调用 start()
- **THEN** 触摸任务开始运行，以 20ms 间隔轮询 M5.Touch

#### Scenario: 无触摸时低开销轮询
- **WHEN** 无用户触摸
- **THEN** 每 20ms 读取一次 `M5.Touch.getDetail()`，`isPressed()` 返回 false，无进一步处理

### Requirement: 坐标映射
GestureRecognizer SHALL 直接使用 `M5.Touch.getDetail()` 返回的 x, y 坐标作为逻辑坐标（x∈[0,539], y∈[0,959]）。M5Unified 已自动将 GT911 物理坐标映射到显示逻辑坐标。

#### Scenario: 坐标直通
- **WHEN** `M5.Touch.getDetail()` 返回坐标 (270, 480)
- **THEN** 事件中的逻辑坐标为 (270, 480)

### Requirement: 触摸数据读取
GestureRecognizer SHALL 通过以下方式读取触摸数据：
```
M5.update();  // 或在任务中直接调用 M5.Touch.update()
auto detail = M5.Touch.getDetail();
bool pressed = detail.isPressed();
int x = detail.x;
int y = detail.y;
```

#### Scenario: 读取触摸点
- **WHEN** 用户触摸屏幕
- **THEN** `detail.isPressed()` 返回 true，`detail.x` 和 `detail.y` 为触摸坐标

#### Scenario: 无触摸
- **WHEN** 用户未触摸屏幕
- **THEN** `detail.isPressed()` 返回 false
