## Context

Parchment 的 InkUI 框架直接依赖 ESP-IDF 和 FreeRTOS API（队列、任务、定时器），显示通过 epdiy 驱动 E-Ink 硬件，触摸通过 GT911 I2C 驱动。每次 UI 调试都需要编译烧录到 M5PaperS3 实机，然后观察墨水屏效果，迭代周期长。

**约束**：
- 模拟器仅支持 Linux 桌面
- 使用简单灰度显示，不模拟 E-Ink 刷新效果（残影、闪黑等）
- 编译选项 `-fno-exceptions -fno-rtti` 保持与 ESP32 一致
- 字体使用相同的 .pfnt 格式，从桌面文件系统加载

## Goals / Non-Goals

**Goals:**
- 在 Linux 桌面上完整运行 InkUI 应用（书架、阅读、设置等全部页面）
- 鼠标点击/拖拽模拟触摸手势（Tap、Swipe、LongPress）
- 4bpp 灰度帧缓冲区精确渲染（与实机像素对应）
- 零修改应用层代码即可在两个平台运行

**Non-Goals:**
- 模拟 E-Ink 刷新特效（残影、部分刷新延迟）
- 支持 macOS/Windows（仅 Linux）
- 模拟 SD 卡文件系统限制（直接使用桌面文件系统）
- 模拟 PSRAM 限制（桌面内存充裕）

## Decisions

### Decision 1: 全面 HAL 重构（方案 B）

**选择**: 定义 5 个纯虚 HAL 接口，InkUI 核心通过依赖注入使用

**替代方案**:
- A) 最小 HAL + Stub — 只替换最底层调用（`epd_hl_update_screen` 等），其他用 `#ifdef` 条件编译。优点是改动小，缺点是 `#ifdef` 散落各处、不可测试
- C) 链接时替换 — 提供同名 `.c` 替代文件。优点是零代码改动，缺点是签名耦合、脆弱

**理由**: HAL 接口清晰定义了硬件抽象边界，完全消除 `#ifdef`，支持未来单元测试 Mock，也为可能的其他平台移植奠基础。一次性重构成本可控（InkUI 核心文件有限），长期收益高。

### Decision 2: SDL2 作为显示和输入后端

**选择**: 使用 SDL2 库提供窗口、纹理渲染和事件输入

**替代方案**:
- A) 直接用 X11/Wayland API — 底层控制强但代码量大且不跨 Linux 发行版
- B) 使用 GTK/Qt — 太重量级，引入大量不必要依赖

**理由**: SDL2 轻量、跨平台（Linux 各发行版均有包）、API 简洁。只需要基本的窗口创建、纹理上传、事件轮询即可。

### Decision 3: 主线程 SDL 渲染 + 后台线程 Application

**选择**: `app.run()` 在 detached 后台线程执行，SDL 事件循环在主线程。后台线程更新 pixelBuf_（内存操作），主线程调用 `presentIfNeeded()` 提交 SDL 渲染。

**替代方案**:
- A) 单线程全部在主线程 — 需要重构 `app.run()` 为非阻塞的 `app.step()`，改动大
- B) 直接从后台线程调用 SDL 渲染 — SDL 要求渲染 API 在创建 renderer 的线程调用，跨线程调用静默失败

**理由**: 后台线程 `app.run()` 保持与 ESP32 相同的阻塞事件循环语义，无需修改 Application 核心逻辑。通过原子标志和互斥锁协调渲染，简单可靠。

### Decision 4: ESP-IDF API Stub 层

**选择**: 在 `simulator/stubs/` 提供简化的 ESP-IDF 头文件和实现（NVS 用内存 KV、FreeRTOS 用 pthread、LittleFS 用 POSIX readdir 等）

**替代方案**:
- A) 条件编译隔离所有 ESP-IDF 调用 — 需要在每个调用点加 `#ifdef`
- B) 抽象出存储/配置层 — 过度工程化，settings_store 等不值得

**理由**: 应用层代码（BootVC、LibraryVC、ReaderVC、book_store、settings_store 等）直接使用 ESP-IDF API（`nvs_get_i32`、`esp_littlefs_info` 等）。通过 stub 头文件和简化实现，这些代码无需修改即可编译。Stub 层只需提供被实际使用到的 API 子集。

### Decision 5: 字体和数据路径可覆盖

**选择**: `FONTS_MOUNT_POINT` 和 `SD_MOUNT_POINT` 通过 `#ifndef` 宏保护，模拟器通过 CMake `-D` 覆盖为桌面路径

**理由**: 最小侵入方式让现有路径代码（`ui_font.c`、`sd_storage.h`）同时适配两个平台。
