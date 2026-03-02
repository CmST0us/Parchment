## ADDED Requirements

### Requirement: FontProvider 接口定义
`ink::FontProvider` SHALL 是一个纯虚类，定义以下接口方法：
- `bool init()` — 初始化字体子系统（挂载存储、加载 UI 字体）
- `const EpdFont* getFont(int sizePx)` — 获取指定像素大小的字体，未找到返回 nullptr

命名空间 `ink::`，头文件 `ink_ui/hal/FontProvider.h`。`EpdFont` 通过前向声明引用。

#### Scenario: ESP32 从 LittleFS 加载
- **WHEN** 在 ESP32 上运行
- **THEN** `init()` 调用 `ui_font_init()` 挂载 LittleFS fonts 分区并加载 UI 字体到 PSRAM

#### Scenario: 桌面从文件系统加载
- **WHEN** 在桌面模拟器运行
- **THEN** `DesktopFontProvider` 委托 `ui_font_init()` 从 `FONTS_MOUNT_POINT` 目录加载 .pfnt 字体文件

### Requirement: 字体生命周期
`init()` 加载的 UI 字体（16px、24px）SHALL 常驻内存，`getFont()` 返回的指针在 FontProvider 生命周期内有效。阅读字体（24px、32px）按需加载，同时只保留一个。

#### Scenario: 获取 UI 字体
- **WHEN** 调用 `getFont(16)` 或 `getFont(24)`
- **THEN** 返回已加载的 UI 字体指针（非 nullptr）

#### Scenario: 获取不存在的字体
- **WHEN** 调用 `getFont(48)`（无此尺寸字体）
- **THEN** 返回 nullptr
