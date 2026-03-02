## ADDED Requirements

### Requirement: 独立 CMake 构建
SDL 模拟器 SHALL 是一个独立的 CMake 项目（`simulator/CMakeLists.txt`），不依赖 ESP-IDF 构建系统。使用 PkgConfig 查找 SDL2。编译产物为 `parchment_sim` 可执行文件。

构建命令：`cd simulator && mkdir build && cd build && cmake .. && make`

#### Scenario: 全新构建
- **WHEN** 执行 `cmake .. && make`，系统已安装 SDL2
- **THEN** 成功编译 `parchment_sim` 可执行文件

### Requirement: SdlDisplayDriver
`SdlDisplayDriver` SHALL 实现 `DisplayDriver` 接口：
- `init()`: 分配 4bpp framebuffer（259200 字节）和 ARGB 像素缓冲区（540×960），创建 SDL 窗口、渲染器和流式纹理
- `framebuffer()`: 返回 4bpp framebuffer 指针（与 Canvas 共享）
- `updateArea()`: 将指定物理区域的 4bpp 数据转换为 ARGB 写入像素缓冲区，设置 `needsPresent_` 标志
- `presentIfNeeded()`: 由**主线程**调用，检查标志并将像素缓冲区提交到 SDL 纹理进行渲染

坐标变换（Canvas::setPixel 的逆变换）：物理 (px, py) → 逻辑 (lx, ly) = (539 - py, px)

支持 `--scale N` 参数指定窗口缩放倍数（1-4）。

#### Scenario: 4bpp 到 ARGB 转换
- **WHEN** framebuffer 中物理像素 (px, py) 的灰度值为 0x0 (黑)
- **THEN** 像素缓冲区逻辑位置 (539-py, px) 的 ARGB 值为 0xFF000000（黑色）

#### Scenario: 线程安全渲染
- **WHEN** 后台线程调用 `updateArea()`
- **THEN** 只更新 pixelBuf_ 并设置原子标志，不调用任何 SDL API
- **AND** 主线程下一次调用 `presentIfNeeded()` 时执行 SDL 渲染

### Requirement: SdlTouchDriver
`SdlTouchDriver` SHALL 实现 `TouchDriver` 接口：
- `pushMouseEvent(x, y, pressed)`: 从 SDL 主线程调用，将鼠标事件推入线程安全队列
- `waitForTouch()`: 在手势线程阻塞等待，条件变量唤醒
- `read()`: 返回最新触摸状态

鼠标坐标 SHALL 除以缩放倍数转换为逻辑坐标。

#### Scenario: 鼠标点击转触摸
- **WHEN** 用户在窗口坐标 (100, 200) 按下左键，scale=1
- **THEN** `read()` 返回 `TouchPoint{x=100, y=200, valid=true}`

#### Scenario: 鼠标释放
- **WHEN** 用户释放左键
- **THEN** `read()` 返回 `TouchPoint{valid=false}`

### Requirement: DesktopPlatform
`DesktopPlatform` SHALL 实现 `Platform` 接口：
- 队列: `SimQueue` 结构（`std::deque<std::vector<uint8_t>>` + mutex + condition_variable）
- 任务: `std::thread` detached 线程
- 定时器: detached 线程 + `std::this_thread::sleep_for` + 回调
- 时间: `std::chrono::steady_clock` 微秒

#### Scenario: 队列超时
- **WHEN** `queueReceive` 等待 30000ms，队列为空
- **THEN** 30 秒后返回 false

### Requirement: DesktopFontProvider
`DesktopFontProvider` SHALL 实现 `FontProvider` 接口，委托 `ui_font_init()` 和 `ui_font_get()` 从 `FONTS_MOUNT_POINT` 路径加载 .pfnt 字体文件。

#### Scenario: 加载桌面字体
- **WHEN** `FONTS_MOUNT_POINT` 指向 `simulator/fonts/`，目录包含 .pfnt 文件
- **THEN** `init()` 成功加载字体，`getFont(16)` 返回有效字体指针

### Requirement: DesktopSystemInfo
`DesktopSystemInfo` SHALL 实现 `SystemInfo` 接口：
- `batteryPercent()` 固定返回 100
- `getTimeString()` 使用 `localtime_r` + `strftime` 格式化当前时间

#### Scenario: 模拟电池
- **WHEN** 调用 `batteryPercent()`
- **THEN** 返回 100

### Requirement: 兼容层
模拟器 SHALL 提供以下兼容层使应用层代码无修改编译：

1. **epdiy 兼容** (`simulator/compat/epdiy.h`): 定义 `EpdFont`/`EpdGlyph`/`EpdUnicodeInterval` tagged 结构体和 `epd_get_glyph()` 二分查找函数
2. **miniz 兼容** (`simulator/compat/miniz.h`): tinfl 解压器用于 glyph bitmap 解压
3. **ESP-IDF stub** (`simulator/stubs/`): NVS（内存 KV 存储）、LittleFS（POSIX readdir）、FreeRTOS（pthread）、esp_timer、esp_heap_caps 等简化实现

#### Scenario: NVS 存储持久化
- **WHEN** 应用调用 `nvs_set_i32` 存储设置
- **THEN** 值保存在内存 HashMap 中，进程退出后丢失

### Requirement: 路径重定向
`FONTS_MOUNT_POINT` 和 `SD_MOUNT_POINT` SHALL 通过 `#ifndef` 宏保护，模拟器通过 CMake `-D` 覆盖为桌面路径：
- `FONTS_MOUNT_POINT` → `simulator/fonts/`
- `SD_MOUNT_POINT` → `simulator/data/`

#### Scenario: 书籍扫描桌面目录
- **WHEN** book_store 扫描 `SD_MOUNT_POINT "/book/"`
- **THEN** 实际扫描 `simulator/data/book/` 目录

### Requirement: 主入口线程模型
`simulator/src/main.cpp` SHALL：
1. 主线程初始化 SDL、创建 HAL 实例、调用 `app.init()` 和 `navigator().push(BootVC)`
2. 在 detached 后台线程启动 `app.run()`（`[[noreturn]]`）
3. 主线程运行 SDL 事件循环：轮询 SDL_Event、转发鼠标事件到 SdlTouchDriver、调用 `display.presentIfNeeded()`
4. 窗口关闭时调用 `_exit(0)` 直接退出

#### Scenario: 正常启动
- **WHEN** 运行 `./parchment_sim`
- **THEN** 出现 540×960 SDL 窗口，显示 Boot 页面，2 秒后切换到 Library 页面
