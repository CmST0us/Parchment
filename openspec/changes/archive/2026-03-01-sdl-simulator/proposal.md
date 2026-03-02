## Why

在真实硬件上调试 UI 需要频繁烧录和观察墨水屏，迭代效率低。需要一个基于 SDL2 的桌面模拟器，在 Linux 上实时预览 InkUI 界面，支持鼠标模拟触摸，大幅提升开发效率。

## What Changes

- **新增 HAL 抽象层**：从 InkUI 核心中抽取 5 个纯虚接口（DisplayDriver, TouchDriver, Platform, FontProvider, SystemInfo），解耦硬件依赖
- **重构 InkUI 核心**：Application、RenderEngine、GestureRecognizer 改为通过 HAL 接口注入依赖，移除直接 ESP-IDF/FreeRTOS 调用
- **新增 ESP32 HAL 实现**：Esp32TouchDriver、Esp32Platform、Esp32SystemInfo，保持原硬件逻辑不变
- **新增 SDL 模拟器**：独立 CMake 项目 `simulator/`，包含 SdlDisplayDriver、SdlTouchDriver、DesktopPlatform、DesktopFontProvider、DesktopSystemInfo
- **新增兼容层**：epdiy 字体结构兼容、miniz 解压、ESP-IDF API stub（NVS、LittleFS、FreeRTOS、esp_timer 等）
- **路径可覆盖**：`FONTS_MOUNT_POINT` 和 `SD_MOUNT_POINT` 可通过编译宏重定向到桌面路径

## Capabilities

### New Capabilities
- `hal-display-driver`: 显示驱动 HAL 接口——framebuffer 访问、区域刷新、全屏操作
- `hal-touch-driver`: 触摸输入 HAL 接口——阻塞等待、读取触摸点
- `hal-platform`: 平台抽象 HAL 接口——队列、任务、定时器、时间
- `hal-font-provider`: 字体加载 HAL 接口——初始化、按尺寸获取字体
- `hal-system-info`: 系统信息 HAL 接口——电池电量、时间
- `sdl-simulator`: SDL2 桌面模拟器——完整 InkUI 仿真运行环境

### Modified Capabilities
- `ink-application`: `init()` 改为接受 HAL 接口注入（DisplayDriver&, TouchDriver&, Platform&, SystemInfo&），移除直接 FreeRTOS/ESP-IDF 调用
- `ink-render-engine`: 构造改为接受 `DisplayDriver&` 替代 `EpdDriver&`，刷新通过 `RefreshMode` 枚举
- `ink-gesture-recognizer`: 构造改为接受 `TouchDriver&` 和 `Platform&`，使用 HAL 接口读取触摸和创建任务

## Impact

- **新增文件**: `components/ink_ui/include/ink_ui/hal/` (5 个 HAL 头文件)、`components/ink_ui/include/ink_ui/hal/Esp32*.h/.cpp` (3 个 ESP32 实现)、`simulator/` (完整模拟器目录)
- **修改文件**: `Application.h/.cpp`、`RenderEngine.h/.cpp`、`GestureRecognizer.h/.cpp`、`EpdDriver.h`、`main/main.cpp`、`main/CMakeLists.txt`、`ui_font.c`、`sd_storage.h`
- **依赖**: 模拟器依赖 SDL2、pthread；ESP32 目标无新依赖
- **线程模型**: 模拟器中 `app.run()` 在后台线程运行，SDL 事件循环在主线程，通过 `presentIfNeeded()` 原子标志协调渲染
- **构建**: ESP32 目标构建不受影响；模拟器通过独立 CMake 构建
