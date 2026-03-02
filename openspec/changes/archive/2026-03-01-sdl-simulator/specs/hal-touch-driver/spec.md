## ADDED Requirements

### Requirement: TouchDriver 接口定义
`ink::TouchDriver` SHALL 是一个纯虚类，定义以下接口方法：
- `bool init()` — 初始化触摸硬件/后端
- `bool waitForTouch(uint32_t timeout_ms)` — 阻塞等待触摸事件，超时返回 false
- `TouchPoint read()` — 读取当前触摸状态

命名空间 `ink::`，头文件 `ink_ui/hal/TouchDriver.h`。

#### Scenario: ESP32 GT911 实现
- **WHEN** 在 ESP32 上运行
- **THEN** `Esp32TouchDriver` 通过 GPIO 中断等待触摸，I2C 读取 GT911 数据

#### Scenario: SDL 模拟器实现
- **WHEN** 在桌面模拟器运行
- **THEN** `SdlTouchDriver` 将鼠标按下/移动/释放转换为 TouchPoint

### Requirement: TouchPoint 结构
`ink::TouchPoint` SHALL 包含：
- `int x` — 触摸 X 坐标（逻辑坐标系 0-539）
- `int y` — 触摸 Y 坐标（逻辑坐标系 0-959）
- `bool valid` — 是否有有效触摸（false 表示手指已抬起）

#### Scenario: 手指抬起
- **WHEN** 无触摸接触
- **THEN** `read()` 返回 `TouchPoint{valid=false}`

### Requirement: 线程安全
`waitForTouch()` 和 `read()` SHALL 可从 GestureRecognizer 任务线程安全调用。在模拟器中，`pushMouseEvent()` 从 SDL 主线程调用，`waitForTouch()`/`read()` 从手势任务线程调用，通过互斥锁和条件变量同步。

#### Scenario: 跨线程触摸传递
- **WHEN** 主线程推入鼠标事件，手势线程正在 `waitForTouch()` 阻塞
- **THEN** 手势线程被唤醒，`read()` 返回正确的触摸坐标
