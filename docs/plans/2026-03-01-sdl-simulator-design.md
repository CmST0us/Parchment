# SDL Simulator Design

基于 SDL2 的 Parchment 电纸书全功能模拟器设计方案。

## 目标

- 在 Linux 桌面上全功能仿真 Parchment 电纸书 UI
- 包括书籍加载、阅读翻页、设置持久化等完整功能链
- 简单灰度渲染（不模拟墨水屏残影/刷新效果）
- 通过 HAL 抽象层实现代码解耦

## 架构方案：全面 HAL 重构

### 核心原则

在 `components/ink_ui/hal/` 下定义纯虚接口，InkUI 框架只依赖这些接口。硬件实现和模拟器实现分别提供具体实现。`Application` 通过构造函数依赖注入 HAL 实例。

### 目录结构

```
components/ink_ui/
  hal/                          ← 新增：纯虚接口定义
    DisplayDriver.h
    TouchDriver.h
    Platform.h
    FontProvider.h
    SystemInfo.h

components/epd_driver/          ← 改为实现 DisplayDriver 接口
components/gt911/               ← 改为实现 TouchDriver 接口

simulator/
  CMakeLists.txt                ← 独立 CMake 构建（非 ESP-IDF）
  src/
    main.cpp                    ← SDL 入口 + HAL 实例创建
    SdlDisplayDriver.cpp/h      ← DisplayDriver 的 SDL 实现
    SdlTouchDriver.cpp/h        ← TouchDriver 的 SDL 鼠标实现
    DesktopPlatform.cpp/h       ← Platform 的 std::thread 实现
    DesktopFontProvider.cpp/h   ← FontProvider 的本地文件系统实现
    DesktopSystemInfo.cpp/h     ← SystemInfo stub
  stubs/
    sim_nvs.cpp                 ← NVS API → JSON 文件
    sim_esp_log.cpp             ← ESP_LOGx → fprintf
    sim_heap.cpp                ← heap_caps_malloc → malloc
    sim_md5.cpp                 ← mbedtls MD5 替代实现
  data/
    book/                       ← 测试用 .txt 文件
    settings.json
    progress.json
  fonts/                        ← .pfnt 字体文件（从设备拷贝）
```

## HAL 接口定义

### 1. DisplayDriver — 显示驱动

```cpp
namespace ink {

enum class RefreshMode {
    Full,   // GL16 — 灰度准确，不闪黑
    Fast,   // DU  — 单色直接更新，最快
    Clear   // GC16 — 全屏清除残影，会闪黑
};

class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;
    virtual void init() = 0;
    virtual uint8_t* framebuffer() = 0;
    virtual void updateArea(int x, int y, int w, int h, RefreshMode mode) = 0;
    virtual void updateScreen() = 0;
    virtual void fullClear() = 0;
    virtual void setAllWhite() = 0;
    virtual int width() const = 0;   // 960 (物理宽)
    virtual int height() const = 0;  // 540 (物理高)
};

} // namespace ink
```

### 2. TouchDriver — 触摸输入

```cpp
namespace ink {

struct TouchPoint {
    int x;       // 逻辑坐标 [0, 539]
    int y;       // 逻辑坐标 [0, 959]
    bool valid;
};

class TouchDriver {
public:
    virtual ~TouchDriver() = default;
    virtual void init() = 0;
    virtual bool waitForTouch(uint32_t timeout_ms) = 0;
    virtual TouchPoint read() = 0;
};

} // namespace ink
```

### 3. Platform — OS 抽象层

```cpp
namespace ink {

using QueueHandle = void*;
using TaskHandle = void*;
using TimerCallback = void(*)(void* arg);

class Platform {
public:
    virtual ~Platform() = default;

    // 事件队列
    virtual QueueHandle createQueue(int maxItems, int itemSize) = 0;
    virtual bool queueSend(QueueHandle q, const void* item, uint32_t timeout_ms) = 0;
    virtual bool queueReceive(QueueHandle q, void* item, uint32_t timeout_ms) = 0;

    // 任务
    virtual TaskHandle createTask(void(*func)(void*), const char* name,
                                   int stackSize, void* arg, int priority) = 0;

    // 定时器
    virtual void startOneShotTimer(uint32_t delay_ms, TimerCallback cb, void* arg) = 0;

    // 时间
    virtual int64_t getTimeUs() = 0;

    // 延时
    virtual void delayMs(uint32_t ms) = 0;

    // 日志
    virtual void log(const char* tag, const char* fmt, ...) = 0;
};

} // namespace ink
```

### 4. FontProvider — 字体加载

```cpp
namespace ink {

struct EpdFont;  // 复用 epdiy 的字体结构

class FontProvider {
public:
    virtual ~FontProvider() = default;
    virtual void init() = 0;
    virtual const EpdFont* getFont(int sizePx) = 0;
};

} // namespace ink
```

### 5. SystemInfo — 系统信息

```cpp
namespace ink {

class SystemInfo {
public:
    virtual ~SystemInfo() = default;
    virtual int batteryPercent() = 0;
    virtual void getTimeString(char* buf, int bufSize) = 0;
};

} // namespace ink
```

## 模拟器实现细节

### SdlDisplayDriver

- 创建 540x960 portrait SDL 窗口
- 内部维护 960x540 的 4bpp framebuffer（与硬件格式完全一致）
- `updateArea()` / `updateScreen()` 时将 4bpp 转换为 32bpp ARGB，blit 到 SDL_Texture
- 坐标变换：framebuffer 物理坐标 → SDL 逻辑坐标（与 RenderEngine 的 logicalToPhysical 互逆）
- 可选 2x 缩放以适应高分屏

### SdlTouchDriver

- `waitForTouch()` 使用 `SDL_WaitEventTimeout` 等待鼠标事件
- 鼠标左键按下/移动/释放映射为 TouchPoint
- SDL 窗口坐标直接对应逻辑坐标（考虑缩放比）

### DesktopPlatform

| FreeRTOS | 桌面实现 |
|---|---|
| `xQueueCreate` | `std::mutex` + `std::condition_variable` + `std::deque` |
| `xQueueSend/Receive` | 线程安全队列 push/pop，支持超时 |
| `xTaskCreate` | `std::thread` |
| `esp_timer_start_once` | `std::thread` + `sleep_for` + callback |
| `esp_timer_get_time` | `std::chrono::steady_clock` |
| `vTaskDelay` | `std::this_thread::sleep_for` |
| `ESP_LOGx` | `fprintf(stderr, ...)` 带颜色 |

### DesktopFontProvider

- 从 `simulator/fonts/` 目录加载 `.pfnt` 文件
- 复用 `ui_font_pfnt.c` 解析逻辑（纯 C，无硬件依赖）
- `heap_caps_malloc` 替换为 `malloc`
- 跳过 LittleFS 挂载，直接用 POSIX 文件 I/O

### 数据层

| 功能 | 硬件 | 模拟器 |
|---|---|---|
| 书籍存储 | SD 卡 `/sdcard/book/` | `simulator/data/book/` |
| 字体文件 | LittleFS `/fonts/` | `simulator/fonts/` |
| 阅读设置 | NVS flash | `simulator/data/settings.json` |
| 阅读进度 | NVS flash | `simulator/data/progress.json` |

NVS stub：`nvs_open()` 加载 JSON → 内存 map，`nvs_get/set` 操作 map，`nvs_commit()` 写回 JSON。

## 对现有代码的改动

| 文件 | 改动 | 说明 |
|---|---|---|
| `ink_ui/core/Application.h/cpp` | `init()` 参数改为接收 HAL 引用 | 核心改动 |
| `ink_ui/core/EpdDriver.h/cpp` | 提取接口，现有代码改为继承 DisplayDriver | 拆分 |
| `ink_ui/core/GestureRecognizer.h/cpp` | 通过 TouchDriver + Platform 访问硬件 | 去除直接 FreeRTOS/GPIO |
| `ink_ui/core/RenderEngine.h/cpp` | 使用 DisplayDriver& 而非 EpdDriver& | 接口替换 |
| `ink_ui/core/Canvas.h/cpp` | 无改动 | 已经只操作 framebuffer |
| `main/main.cpp` | 创建硬件 HAL 实例传入 Application | 入口适配 |

## 构建系统

独立 CMakeLists.txt（非 ESP-IDF），定义 `SIMULATOR=1` 编译宏。直接编译 InkUI 源码 + 模拟器 HAL 实现 + 需要的 components 源码。依赖：SDL2、pthread。

```bash
# 安装依赖
sudo apt install libsdl2-dev

# 构建
cd simulator && mkdir -p build && cd build
cmake .. && make -j$(nproc)

# 运行
./parchment_sim
```

## 不在范围内

- macOS / Windows 支持
- EPD 残影/刷新动画模拟
- 多点触控
- Wi-Fi / 蓝牙功能模拟
