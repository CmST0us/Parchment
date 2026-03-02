## ADDED Requirements

### Requirement: Platform 接口定义
`ink::Platform` SHALL 是一个纯虚类，定义以下接口方法：
- `QueueHandle createQueue(int maxItems, int itemSize)` — 创建消息队列
- `bool queueSend(QueueHandle, const void* item, uint32_t timeout_ms)` — 发送数据到队列
- `bool queueReceive(QueueHandle, void* item, uint32_t timeout_ms)` — 从队列接收数据
- `TaskHandle createTask(entry, name, stackSize, arg, priority)` — 创建任务
- `bool startOneShotTimer(uint32_t delayMs, TimerCallback, void* arg)` — 启动一次性定时器
- `int64_t getTimeUs()` — 获取系统时间（微秒）
- `void delayMs(uint32_t ms)` — 延迟指定毫秒

命名空间 `ink::`，头文件 `ink_ui/hal/Platform.h`。

#### Scenario: ESP32 实现
- **WHEN** 在 ESP32 上运行
- **THEN** `Esp32Platform` 通过 FreeRTOS xQueueCreate/xQueueSend/xQueueReceive/xTaskCreate/esp_timer 实现

#### Scenario: 桌面实现
- **WHEN** 在桌面模拟器运行
- **THEN** `DesktopPlatform` 通过 std::deque + mutex（队列）、std::thread（任务）、detached thread + sleep（定时器）实现

### Requirement: 类型擦除句柄
`QueueHandle` 和 `TaskHandle` SHALL 定义为 `void*`，允许各平台使用不同内部数据结构。

#### Scenario: ESP32 队列句柄
- **WHEN** `Esp32Platform::createQueue()` 返回 QueueHandle
- **THEN** 内部为 FreeRTOS `QueueHandle_t` 强转

#### Scenario: 桌面队列句柄
- **WHEN** `DesktopPlatform::createQueue()` 返回 QueueHandle
- **THEN** 内部为 `SimQueue*`（std::deque + mutex + condition_variable）

### Requirement: TimerCallback 函数签名
`ink::TimerCallback` SHALL 定义为 `void(*)(void*)`，接受一个用户数据指针参数。

#### Scenario: 延迟事件发送
- **WHEN** `Application::postDelayed()` 使用 `startOneShotTimer` 延迟 2000ms
- **THEN** 定时器在 2000ms 后调用回调函数，回调将事件发送到队列
