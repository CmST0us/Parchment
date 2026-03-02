## ADDED Requirements

### Requirement: SystemInfo 接口定义
`ink::SystemInfo` SHALL 是一个纯虚类，定义以下接口方法：
- `int batteryPercent()` — 获取电池电量百分比 (0-100)
- `void getTimeString(char* buf, int bufSize)` — 获取当前时间字符串（如 "HH:MM"）

命名空间 `ink::`，头文件 `ink_ui/hal/SystemInfo.h`。

#### Scenario: ESP32 读取真实电池
- **WHEN** 在 ESP32 上运行
- **THEN** `Esp32SystemInfo::batteryPercent()` 通过 ADC 读取电池电压并转换为百分比

#### Scenario: 模拟器返回固定值
- **WHEN** 在桌面模拟器运行
- **THEN** `DesktopSystemInfo::batteryPercent()` 返回 100（始终满电）

### Requirement: 时间格式
`getTimeString()` SHALL 将当前本地时间格式化为 "HH:MM" 写入 buf。buf 应至少 6 字节。

#### Scenario: 获取时间字符串
- **WHEN** 当前时间为 14:30
- **THEN** `getTimeString(buf, 6)` 写入 "14:30"
