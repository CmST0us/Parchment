## Phase 1: HAL 接口定义与 InkUI 核心重构

- [x] 1.1 创建 5 个 HAL 头文件（DisplayDriver, TouchDriver, Platform, FontProvider, SystemInfo）
- [x] 1.2 重构 RenderEngine：接受 `DisplayDriver&` 替代 `EpdDriver&`
- [x] 1.3 重构 GestureRecognizer：接受 `TouchDriver&` 和 `Platform&`，移除 FreeRTOS 直接调用
- [x] 1.4 重构 Application：`init()` 接受 HAL 注入，使用 Platform 接口操作队列/定时器
- [x] 1.5 更新 EpdDriver：实现 `DisplayDriver` 接口

## Phase 2: ESP32 HAL 实现

- [x] 2.1 实现 Esp32TouchDriver（GT911 封装）
- [x] 2.2 实现 Esp32Platform（FreeRTOS 队列/任务/定时器封装）
- [x] 2.3 实现 Esp32SystemInfo（ADC 电池读取 + localtime）
- [x] 2.4 更新 main/main.cpp 创建 HAL 实例并注入 Application
- [x] 2.5 更新 main/CMakeLists.txt

## Phase 3: 模拟器基础设施

- [x] 3.1 创建 simulator/ 目录结构和 CMakeLists.txt
- [x] 3.2 实现 epdiy 兼容层（字体结构 + glyph 查找 + miniz 解压）
- [x] 3.3 实现 ESP-IDF API stub 头文件和简化实现

## Phase 4: 模拟器 HAL 实现

- [x] 4.1 实现 SdlDisplayDriver（4bpp framebuffer → SDL 纹理 + 坐标变换）
- [x] 4.2 实现 SdlTouchDriver（鼠标事件 → TouchPoint + 线程安全队列）
- [x] 4.3 实现 DesktopPlatform（std::thread 队列/任务/定时器）
- [x] 4.4 实现 DesktopFontProvider（委托 ui_font 从桌面路径加载 .pfnt）
- [x] 4.5 实现 DesktopSystemInfo（模拟电池 100% + localtime）

## Phase 5: 集成与修复

- [x] 5.1 创建 simulator/src/main.cpp 入口（SDL 初始化 + HAL 注入 + 事件循环）
- [x] 5.2 路径重定向：FONTS_MOUNT_POINT / SD_MOUNT_POINT 可覆盖
- [x] 5.3 构建修复（CMake 排除 EpdDriver、tagged struct、缺失 stub 等）
- [x] 5.4 修复白屏问题：SDL 渲染线程安全（主线程 presentIfNeeded + 原子标志）
