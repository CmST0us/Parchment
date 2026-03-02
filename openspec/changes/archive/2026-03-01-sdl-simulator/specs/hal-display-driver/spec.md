## ADDED Requirements

### Requirement: DisplayDriver 接口定义
`ink::DisplayDriver` SHALL 是一个纯虚类，定义以下接口方法：
- `bool init()` — 初始化显示硬件/后端
- `uint8_t* framebuffer()` — 获取 4bpp 物理帧缓冲区指针
- `bool updateArea(int x, int y, int w, int h, RefreshMode mode)` — 局部区域刷新（物理坐标）
- `bool updateScreen()` — 全屏刷新
- `void fullClear()` — 全屏闪黑清除（消残影）
- `void setAllWhite()` — 将帧缓冲区设为全白（不触发刷新）
- `int width() const` — 物理 framebuffer 宽度
- `int height() const` — 物理 framebuffer 高度

命名空间 `ink::`，头文件 `ink_ui/hal/DisplayDriver.h`。

#### Scenario: ESP32 EpdDriver 实现
- **WHEN** 在 ESP32 上运行
- **THEN** `EpdDriver` 继承 `DisplayDriver`，通过 epdiy 库操作墨水屏硬件

#### Scenario: SDL 模拟器实现
- **WHEN** 在桌面模拟器运行
- **THEN** `SdlDisplayDriver` 继承 `DisplayDriver`，将 4bpp framebuffer 转换为 SDL 纹理显示

### Requirement: RefreshMode 枚举
`ink::RefreshMode` SHALL 定义以下三种刷新模式：
- `Full` — 灰度准确刷新，不闪黑（对应 EPD MODE_GL16）
- `Fast` — 快速单色刷新（对应 EPD MODE_DU）
- `Clear` — 闪黑全清，消残影（对应 EPD MODE_GC16）

#### Scenario: 模拟器忽略刷新模式
- **WHEN** 模拟器调用 `updateArea` 传入任意 RefreshMode
- **THEN** 模拟器均以相同方式渲染（无 E-Ink 特效差异）

### Requirement: 屏幕尺寸常量
`DisplayDriver.h` SHALL 定义以下全局常量（命名空间 `ink::`）：
- `kScreenWidth = 540` — 逻辑宽度（portrait）
- `kScreenHeight = 960` — 逻辑高度（portrait）
- `kFbPhysWidth = 960` — 物理 framebuffer 宽度（landscape）
- `kFbPhysHeight = 540` — 物理 framebuffer 高度（landscape）

#### Scenario: Canvas 和 View 使用屏幕常量
- **WHEN** Canvas 检查屏幕边界
- **THEN** 使用 `kScreenWidth`/`kScreenHeight` 而非硬编码数值

### Requirement: Framebuffer 格式
`framebuffer()` 返回的缓冲区 SHALL 为 4bpp 灰度格式（每字节 2 像素），物理布局 960×540 landscape。总大小 `960/2 * 540 = 259200` 字节。像素编码：
- 偶数像素 px：低 nibble `(gray >> 4)`
- 奇数像素 px：高 nibble `(gray & 0xF0)`

#### Scenario: Canvas 写入像素
- **WHEN** Canvas::setPixel 写入逻辑坐标 (absX, absY)
- **THEN** 通过坐标变换 `px = absY, py = 539 - absX` 写入物理 framebuffer
