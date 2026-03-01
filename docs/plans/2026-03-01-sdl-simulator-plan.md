# SDL Simulator Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Build a full-function SDL2 simulator for the Parchment e-reader, enabling desktop UI development and testing without hardware.

**Architecture:** HAL abstraction layer — define pure virtual interfaces in `ink_ui/hal/`, InkUI framework depends only on these interfaces. ESP32 hardware and SDL simulator each provide concrete implementations. Application receives HAL dependencies via constructor injection.

**Tech Stack:** C++17, SDL2, POSIX threads, CMake (standalone, not ESP-IDF)

---

## Phase 1: HAL Interface Definitions

### Task 1: Create HAL interface headers

Create 5 pure virtual interface headers. These define the contract between InkUI and platform-specific code.

**Files:**
- Create: `components/ink_ui/include/ink_ui/hal/DisplayDriver.h`
- Create: `components/ink_ui/include/ink_ui/hal/TouchDriver.h`
- Create: `components/ink_ui/include/ink_ui/hal/Platform.h`
- Create: `components/ink_ui/include/ink_ui/hal/FontProvider.h`
- Create: `components/ink_ui/include/ink_ui/hal/SystemInfo.h`

**Step 1: Create DisplayDriver.h**

```cpp
/**
 * @file DisplayDriver.h
 * @brief 显示驱动抽象接口 — EPD 或模拟器统一的显示操作。
 */
#pragma once

#include <cstdint>

namespace ink {

/// 刷新模式
enum class RefreshMode {
    Full,   ///< GL16 — 灰度准确，不闪黑
    Fast,   ///< DU  — 单色直接更新，最快
    Clear   ///< GC16 — 全屏清除残影，会闪黑
};

/// 显示驱动抽象接口
class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;

    /// 初始化显示设备，分配 framebuffer
    virtual void init() = 0;

    /// 获取 4bpp 灰度 framebuffer 指针 (物理布局 960x540)
    virtual uint8_t* framebuffer() = 0;

    /// 刷新指定区域到屏幕 (物理坐标)
    virtual void updateArea(int x, int y, int w, int h, RefreshMode mode) = 0;

    /// 全屏刷新 (GL16)
    virtual void updateScreen() = 0;

    /// 全屏清除（闪黑消残影后全白）
    virtual void fullClear() = 0;

    /// 设置 framebuffer 内容全白（不刷新屏幕）
    virtual void setAllWhite() = 0;

    /// 屏幕物理宽度 (960)
    virtual int width() const = 0;

    /// 屏幕物理高度 (540)
    virtual int height() const = 0;
};

} // namespace ink
```

**Step 2: Create TouchDriver.h**

```cpp
/**
 * @file TouchDriver.h
 * @brief 触摸输入抽象接口 — GT911 或鼠标统一的触摸点读取。
 */
#pragma once

#include <cstdint>

namespace ink {

/// 触摸点数据
struct TouchPoint {
    int x;       ///< 逻辑坐标 [0, 539]
    int y;       ///< 逻辑坐标 [0, 959]
    bool valid;  ///< 是否有有效触摸
};

/// 触摸输入抽象接口
class TouchDriver {
public:
    virtual ~TouchDriver() = default;

    /// 初始化触摸设备
    virtual void init() = 0;

    /// 等待触摸事件（阻塞），返回 true 表示有事件
    virtual bool waitForTouch(uint32_t timeout_ms) = 0;

    /// 读取当前触摸点
    virtual TouchPoint read() = 0;
};

} // namespace ink
```

**Step 3: Create Platform.h**

```cpp
/**
 * @file Platform.h
 * @brief OS 平台抽象接口 — FreeRTOS 或 std::thread 统一的任务/队列/定时器。
 */
#pragma once

#include <cstdint>
#include <cstdarg>

namespace ink {

/// 队列句柄 (类型擦除)
using QueueHandle = void*;

/// 任务句柄
using TaskHandle = void*;

/// 定时器回调函数签名
using TimerCallback = void(*)(void* arg);

/// OS 平台抽象接口
class Platform {
public:
    virtual ~Platform() = default;

    // ── 事件队列 ──

    /// 创建固定大小的队列
    virtual QueueHandle createQueue(int maxItems, int itemSize) = 0;

    /// 向队列发送数据，timeout_ms=0 表示不等待
    virtual bool queueSend(QueueHandle q, const void* item, uint32_t timeout_ms) = 0;

    /// 从队列接收数据（阻塞），timeout_ms=UINT32_MAX 表示永久等待
    virtual bool queueReceive(QueueHandle q, void* item, uint32_t timeout_ms) = 0;

    // ── 任务 ──

    /// 创建后台任务
    virtual TaskHandle createTask(void(*func)(void*), const char* name,
                                   int stackSize, void* arg, int priority) = 0;

    // ── 定时器 ──

    /// 启动一次性定时器，delay_ms 后调用 cb(arg)
    virtual void startOneShotTimer(uint32_t delay_ms, TimerCallback cb, void* arg) = 0;

    // ── 时间 ──

    /// 获取微秒级单调时钟
    virtual int64_t getTimeUs() = 0;

    /// 延时指定毫秒
    virtual void delayMs(uint32_t ms) = 0;
};

} // namespace ink
```

**Step 4: Create FontProvider.h**

```cpp
/**
 * @file FontProvider.h
 * @brief 字体加载抽象接口 — LittleFS 或本地文件系统统一的字体获取。
 */
#pragma once

struct EpdFont;  // epdiy 字体结构前向声明

namespace ink {

/// 字体加载抽象接口
class FontProvider {
public:
    virtual ~FontProvider() = default;

    /// 初始化字体系统（挂载文件系统、加载常驻字体）
    virtual void init() = 0;

    /// 按像素大小获取字体，返回 nullptr 表示不可用
    virtual const EpdFont* getFont(int sizePx) = 0;
};

} // namespace ink
```

**Step 5: Create SystemInfo.h**

```cpp
/**
 * @file SystemInfo.h
 * @brief 系统信息抽象接口 — 电池和时间。
 */
#pragma once

namespace ink {

/// 系统信息抽象接口
class SystemInfo {
public:
    virtual ~SystemInfo() = default;

    /// 获取电池电量百分比 [0, 100]
    virtual int batteryPercent() = 0;

    /// 获取当前时间字符串 (HH:MM)
    virtual void getTimeString(char* buf, int bufSize) = 0;
};

} // namespace ink
```

**Step 6: Commit**

```bash
git add components/ink_ui/include/ink_ui/hal/
git commit -m "feat: add HAL interface definitions for display, touch, platform, font, system"
```

---

## Phase 2: Refactor InkUI to use HAL

### Task 2: Refactor RenderEngine to use DisplayDriver

Replace `EpdDriver&` with `DisplayDriver&` in RenderEngine. This is a minimal change.

**Files:**
- Modify: `components/ink_ui/include/ink_ui/core/RenderEngine.h`
- Modify: `components/ink_ui/src/core/RenderEngine.cpp`

**Step 1: Update RenderEngine.h**

Replace the include and member type:
- Change `#include "ink_ui/core/EpdDriver.h"` → `#include "ink_ui/hal/DisplayDriver.h"`
- Change constructor: `explicit RenderEngine(EpdDriver& driver)` → `explicit RenderEngine(DisplayDriver& driver)`
- Change member: `EpdDriver& driver_` → `DisplayDriver& driver_`
- Keep all `EpdMode` references in `flush()` — need to map `RefreshHint` to `RefreshMode`

In `flush()` method, the `EpdMode::GL16` calls become `RefreshMode::Full`. Update the `flush()` method to use `RefreshMode` instead of `EpdMode`.

**Step 2: Update RenderEngine.cpp**

- Remove `extern "C" { #include "esp_log.h" }` — replace ESP_LOGI with a comment or conditional
- In `flush()`: change `driver_.updateArea(phys, EpdMode::GL16)` → `driver_.updateArea(phys.x, phys.y, phys.w, phys.h, RefreshMode::Full)`
- In `repairDamage()`: same change for the updateArea call

Note: The `updateArea` signature changes from `updateArea(Rect, EpdMode)` to `updateArea(int,int,int,int, RefreshMode)`.

**Step 3: Commit**

```bash
git add components/ink_ui/include/ink_ui/core/RenderEngine.h components/ink_ui/src/core/RenderEngine.cpp
git commit -m "refactor: RenderEngine uses DisplayDriver HAL instead of EpdDriver"
```

### Task 3: Refactor GestureRecognizer to use HAL

Replace FreeRTOS primitives and GT911 driver with TouchDriver + Platform HAL.

**Files:**
- Modify: `components/ink_ui/include/ink_ui/core/GestureRecognizer.h`
- Modify: `components/ink_ui/src/core/GestureRecognizer.cpp`

**Step 1: Update GestureRecognizer.h**

- Remove FreeRTOS includes (`freertos/FreeRTOS.h`, `freertos/queue.h`, `freertos/semphr.h`, `freertos/task.h`)
- Add includes: `"ink_ui/hal/TouchDriver.h"`, `"ink_ui/hal/Platform.h"`
- Change constructor: `explicit GestureRecognizer(QueueHandle_t eventQueue)` → `GestureRecognizer(TouchDriver& touch, Platform& platform, QueueHandle eventQueue)`
- Replace FreeRTOS member types:
  - `QueueHandle_t eventQueue_` → `QueueHandle eventQueue_` (ink::QueueHandle)
  - `SemaphoreHandle_t touchSem_` → remove (TouchDriver::waitForTouch replaces semaphore)
  - `TaskHandle_t taskHandle_` → `TaskHandle taskHandle_`
- Add member references: `TouchDriver& touch_`, `Platform& platform_`

**Step 2: Update GestureRecognizer.cpp**

- Remove: `extern "C" { #include "gt911.h"; #include "driver/gpio.h"; #include "esp_log.h"; #include "esp_timer.h" }`
- `start()`:
  - Remove GPIO ISR setup (the ISR pattern is now internal to TouchDriver)
  - Remove `xSemaphoreCreateBinary` — use `touch_.waitForTouch()` instead
  - Replace `xTaskCreate(...)` → `platform_.createTask(taskEntry, "ink_touch", kTaskStackSize, this, kTaskPriority)`
- `taskLoop()`:
  - Idle state: Replace `xSemaphoreTake(touchSem_, timeout)` → `touch_.waitForTouch(timeout_ms)`
  - Tracking state: Replace `vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs))` → `platform_.delayMs(kPollIntervalMs)`
  - Replace `gt911_read(&point)` with `auto tp = touch_.read()` and adapt fields
  - Replace `esp_timer_get_time()` → `platform_.getTimeUs()`
  - Replace `xQueueSend(eventQueue_, &event, 0)` → `platform_.queueSend(eventQueue_, &event, 0)`
- `mapCoords()`: keep the clamping logic, adapt from `gt911_touch_point_t` fields to `TouchPoint` fields
- `sendEvent()`: replace `xQueueSend` → `platform_.queueSend`

**Step 3: Commit**

```bash
git add components/ink_ui/include/ink_ui/core/GestureRecognizer.h components/ink_ui/src/core/GestureRecognizer.cpp
git commit -m "refactor: GestureRecognizer uses TouchDriver + Platform HAL"
```

### Task 4: Refactor Application to accept HAL dependencies

This is the core change — Application receives all HAL interfaces via init().

**Files:**
- Modify: `components/ink_ui/include/ink_ui/core/Application.h`
- Modify: `components/ink_ui/src/core/Application.cpp`

**Step 1: Update Application.h**

- Remove direct includes: `"ink_ui/core/EpdDriver.h"` (keep RenderEngine.h, GestureRecognizer.h, NavigationController.h)
- Add includes: `"ink_ui/hal/DisplayDriver.h"`, `"ink_ui/hal/TouchDriver.h"`, `"ink_ui/hal/Platform.h"`, `"ink_ui/hal/SystemInfo.h"`
- Change `init()` signature:
  ```cpp
  bool init(DisplayDriver& display, TouchDriver& touch,
            Platform& platform, SystemInfo& system);
  ```
- Add `postDelayed(const Event& event, int delayMs)` — needs Platform ref
- Replace `QueueHandle_t eventQueue_` → `QueueHandle eventQueue_`
- Add member references:
  ```cpp
  DisplayDriver* display_ = nullptr;
  Platform* platform_ = nullptr;
  SystemInfo* system_ = nullptr;
  ```

**Step 2: Update Application.cpp**

- Remove FreeRTOS includes: `freertos/FreeRTOS.h`, `freertos/queue.h`
- Remove ESP-IDF includes: `esp_timer.h`, `battery.h`, `esp_log.h`
- Add: `<cstdio>` (for fprintf in place of ESP_LOGI where needed)
- `init()`:
  - Store HAL references: `display_ = &display; platform_ = &platform; system_ = &system;`
  - `display.init()` instead of `EpdDriver::instance().init()`
  - `platform.createQueue(...)` instead of `xQueueCreate(...)`
  - `renderEngine_ = std::make_unique<RenderEngine>(display)`
  - `gesture_ = std::make_unique<GestureRecognizer>(touch, platform, eventQueue_)`
- `run()`:
  - Replace `xQueueReceive(eventQueue_, &event, pdMS_TO_TICKS(timeout))` → `platform_->queueReceive(eventQueue_, &event, kQueueTimeoutMs)`
  - Replace `battery_get_percent()` → `system_->batteryPercent()`
  - Time updates: use `system_->getTimeString()` instead of raw `<ctime>` calls
  - Replace `esp_timer_get_time()` → `platform_->getTimeUs()`
- `postEvent()`:
  - Replace `xQueueSend` → `platform_->queueSend`
- `postDelayed()`:
  - Replace `esp_timer_create` + `esp_timer_start_once` with `platform_->startOneShotTimer(delayMs, callback, ctx)`
  - Keep the `DelayedEventCtx` heap allocation pattern, adjust callback to use Platform's timer

**Step 3: Commit**

```bash
git add components/ink_ui/include/ink_ui/core/Application.h components/ink_ui/src/core/Application.cpp
git commit -m "refactor: Application accepts HAL dependencies via init()"
```

### Task 5: Make EpdDriver implement DisplayDriver

Wrap the existing EpdDriver as a concrete implementation of the DisplayDriver interface.

**Files:**
- Modify: `components/ink_ui/include/ink_ui/core/EpdDriver.h`
- Modify: `components/ink_ui/src/core/EpdDriver.cpp`

**Step 1: Update EpdDriver.h**

- Add include: `"ink_ui/hal/DisplayDriver.h"`
- Change class: `class EpdDriver : public DisplayDriver`
- Keep existing `EpdMode` enum (used internally by the EPD driver, not exposed via HAL)
- Implement all virtual methods from DisplayDriver
- Keep `instance()` singleton for ESP32 code convenience
- Map `RefreshMode` to internal `EpdMode` in `updateArea()`:
  - `RefreshMode::Full` → `EpdMode::GL16`
  - `RefreshMode::Fast` → `EpdMode::DU`
  - `RefreshMode::Clear` → `EpdMode::GC16`

**Step 2: Update EpdDriver.cpp**

- Implement `width()` → return `kFbPhysWidth` (960)
- Implement `height()` → return `kFbPhysHeight` (540)
- Adapt `updateArea(int x, int y, int w, int h, RefreshMode mode)` to call existing `epd_driver_update_area_mode`

**Step 3: Commit**

```bash
git add components/ink_ui/include/ink_ui/core/EpdDriver.h components/ink_ui/src/core/EpdDriver.cpp
git commit -m "refactor: EpdDriver implements DisplayDriver interface"
```

### Task 6: Create ESP32 HAL implementations

Create thin wrappers for the ESP32-specific HAL implementations for TouchDriver, Platform, SystemInfo.

**Files:**
- Create: `components/ink_ui/include/ink_ui/hal/Esp32TouchDriver.h`
- Create: `components/ink_ui/src/hal/Esp32TouchDriver.cpp`
- Create: `components/ink_ui/include/ink_ui/hal/Esp32Platform.h`
- Create: `components/ink_ui/src/hal/Esp32Platform.cpp`
- Create: `components/ink_ui/include/ink_ui/hal/Esp32SystemInfo.h`
- Create: `components/ink_ui/src/hal/Esp32SystemInfo.cpp`

**Step 1: Esp32TouchDriver**

Wraps GT911 driver:
```cpp
class Esp32TouchDriver : public TouchDriver {
public:
    void init() override; // calls gt911_init() with configured pins
    bool waitForTouch(uint32_t timeout_ms) override; // GPIO ISR + semaphore wait
    TouchPoint read() override; // calls gt911_read()
private:
    SemaphoreHandle_t touchSem_;
    // GPIO ISR handler that gives semaphore
};
```

The GPIO ISR setup (currently in GestureRecognizer::start()) moves here.

**Step 2: Esp32Platform**

Wraps FreeRTOS:
```cpp
class Esp32Platform : public Platform {
public:
    QueueHandle createQueue(int maxItems, int itemSize) override;  // xQueueCreate
    bool queueSend(QueueHandle q, const void* item, uint32_t timeout_ms) override;
    bool queueReceive(QueueHandle q, void* item, uint32_t timeout_ms) override;
    TaskHandle createTask(...) override;  // xTaskCreate
    void startOneShotTimer(...) override; // esp_timer
    int64_t getTimeUs() override;         // esp_timer_get_time
    void delayMs(uint32_t ms) override;   // vTaskDelay
};
```

**Step 3: Esp32SystemInfo**

```cpp
class Esp32SystemInfo : public SystemInfo {
public:
    int batteryPercent() override;  // battery_get_percent()
    void getTimeString(char* buf, int bufSize) override; // strftime
};
```

**Step 4: Update ink_ui CMakeLists.txt**

Add the new HAL source files to the SRCS list. These files will only compile in ESP-IDF context (they include FreeRTOS headers).

Guard the ESP32 HAL implementations: only add them to SRCS when building with ESP-IDF (check for `IDF_TARGET` or `ESP_PLATFORM` variable). Since the simulator builds with standalone CMake, these files won't be included.

**Step 5: Commit**

```bash
git add components/ink_ui/include/ink_ui/hal/Esp32*.h components/ink_ui/src/hal/Esp32*.cpp components/ink_ui/CMakeLists.txt
git commit -m "feat: add ESP32 HAL implementations (TouchDriver, Platform, SystemInfo)"
```

### Task 7: Update main/main.cpp for HAL injection

**Files:**
- Modify: `main/main.cpp`

**Step 1: Update main.cpp**

- Add includes for ESP32 HAL implementations
- Create HAL instances before `app.init()`:
  ```cpp
  auto& epd = ink::EpdDriver::instance();  // DisplayDriver
  ink::Esp32TouchDriver touch;
  ink::Esp32Platform platform;
  ink::Esp32SystemInfo system;
  ```
- Change `app.init()` call:
  ```cpp
  app.init(epd, touch, platform, system);
  ```
- Remove direct `gt911_init()` call (now done by Esp32TouchDriver::init(), called from GestureRecognizer::start() → touch.init())
- Remove direct `battery_init()` / `battery_get_percent()` calls (now in Esp32SystemInfo)

**Step 2: Commit**

```bash
git add main/main.cpp
git commit -m "refactor: main.cpp uses HAL injection for Application::init()"
```

---

## Phase 3: Simulator Implementation

### Task 8: Create simulator directory structure and CMake

**Files:**
- Create: `simulator/CMakeLists.txt`
- Create: `simulator/src/` (directory)
- Create: `simulator/stubs/` (directory)
- Create: `simulator/stubs/include/` (directory)
- Create: `simulator/compat/` (directory)
- Create: `simulator/data/book/` (directory)
- Create: `simulator/fonts/` (directory)
- Delete: `simulator/build/` (old build artifacts from previous simulator)

**Step 1: Clean up old simulator**

```bash
rm -rf simulator/build
```

**Step 2: Create directory structure**

```bash
mkdir -p simulator/{src,stubs/include,compat,data/book,fonts}
```

**Step 3: Create CMakeLists.txt**

```cmake
cmake_minimum_required(VERSION 3.16)
project(parchment_sim CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# 模拟器标志
add_compile_definitions(SIMULATOR=1)
add_compile_options(-fno-exceptions -fno-rtti -Wall -Wextra -Wno-unused-parameter)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# ── InkUI 框架源码 ──
set(INK_UI_DIR ${CMAKE_SOURCE_DIR}/../components/ink_ui)
file(GLOB INK_UI_CORE_SRCS ${INK_UI_DIR}/src/core/*.cpp)
file(GLOB INK_UI_VIEW_SRCS ${INK_UI_DIR}/src/views/*.cpp)

# ── 应用代码 ──
set(APP_DIR ${CMAKE_SOURCE_DIR}/../main)
file(GLOB APP_CONTROLLER_SRCS ${APP_DIR}/controllers/*.cpp)
file(GLOB APP_VIEW_SRCS ${APP_DIR}/views/*.cpp)

# ── Components (纯 C, 无硬件依赖的部分) ──
set(COMP_DIR ${CMAKE_SOURCE_DIR}/../components)
set(COMPONENT_SRCS
    ${COMP_DIR}/ui_core/ui_font.c
    ${COMP_DIR}/ui_core/ui_font_pfnt.c
    ${COMP_DIR}/book_store/book_store.c
    ${COMP_DIR}/settings_store/settings_store.c
    ${COMP_DIR}/text_encoding/text_encoding.c
    ${COMP_DIR}/text_encoding/text_encoding_gbk_table.c
    ${COMP_DIR}/text_source/src/TextSource.cpp
    ${COMP_DIR}/text_source/src/TextWindow.cpp
    ${COMP_DIR}/text_source/src/EncodingConverter.cpp
)

# ── 模拟器源码 ──
file(GLOB SIM_SRCS src/*.cpp)
file(GLOB STUB_SRCS stubs/*.cpp stubs/*.c)
file(GLOB COMPAT_SRCS compat/*.c compat/*.cpp)

add_executable(parchment_sim
    ${SIM_SRCS}
    ${INK_UI_CORE_SRCS}
    ${INK_UI_VIEW_SRCS}
    ${APP_CONTROLLER_SRCS}
    ${APP_VIEW_SRCS}
    ${COMPONENT_SRCS}
    ${STUB_SRCS}
    ${COMPAT_SRCS}
)

target_include_directories(parchment_sim PRIVATE
    # InkUI 框架
    ${INK_UI_DIR}/include

    # 应用头文件
    ${APP_DIR}
    ${APP_DIR}/pages

    # Components
    ${COMP_DIR}/ui_core/include
    ${COMP_DIR}/book_store/include
    ${COMP_DIR}/settings_store/include
    ${COMP_DIR}/text_encoding/include
    ${COMP_DIR}/text_source/include
    ${COMP_DIR}/sd_storage/include
    ${COMP_DIR}/epd_driver/include
    ${COMP_DIR}/gt911/include

    # 模拟器 stub/compat 头文件（优先于系统头）
    ${CMAKE_SOURCE_DIR}/stubs/include
    ${CMAKE_SOURCE_DIR}/compat

    # SDL2
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(parchment_sim ${SDL2_LIBRARIES} pthread)
```

**Step 4: Commit**

```bash
git add simulator/CMakeLists.txt
git commit -m "feat: add simulator CMake build system"
```

### Task 9: Create epdiy compatibility layer

The simulator needs EpdFont/EpdGlyph/EpdUnicodeInterval struct definitions and `epd_get_glyph()` function without linking the full epdiy hardware library.

**Files:**
- Create: `simulator/compat/epdiy.h`
- Create: `simulator/compat/epdiy_font.c`

**Step 1: Create epdiy.h compat header**

Copy the essential type definitions from epdiy's headers:
```c
/**
 * @file epdiy.h
 * @brief Minimal epdiy compatibility header for simulator.
 *
 * Contains only font-related types and functions needed by Canvas and ui_font.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t first;
    uint32_t last;
    uint32_t offset;
} EpdUnicodeInterval;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t advance_x;
    int16_t left;
    int16_t top;
    uint32_t compressed_size;
    uint32_t data_offset;
} EpdGlyph;

typedef struct {
    const uint8_t* bitmap;
    const EpdGlyph* glyph;
    const EpdUnicodeInterval* intervals;
    uint32_t interval_count;
    bool compressed;
    uint16_t advance_y;
    int ascender;
    int descender;
} EpdFont;

/// 通过 codepoint 查找 glyph (二分查找)
const EpdGlyph* epd_get_glyph(const EpdFont* font, uint32_t code_point);

#ifdef __cplusplus
}
#endif
```

**Step 2: Create epdiy_font.c**

```c
#include "epdiy.h"

const EpdGlyph* epd_get_glyph(const EpdFont* font, uint32_t code_point) {
    if (!font || !font->intervals || !font->glyph) return NULL;
    int lo = 0, hi = (int)font->interval_count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        const EpdUnicodeInterval* interval = &font->intervals[mid];
        if (code_point < interval->first) {
            hi = mid - 1;
        } else if (code_point > interval->last) {
            lo = mid + 1;
        } else {
            return &font->glyph[interval->offset + (code_point - interval->first)];
        }
    }
    return NULL;
}
```

**Step 3: Commit**

```bash
git add simulator/compat/
git commit -m "feat: add epdiy compat layer (font types + epd_get_glyph)"
```

### Task 10: Create miniz compatibility

Canvas.cpp uses `tinfl_decompress` from ESP-IDF's built-in miniz. The simulator needs a standalone version.

**Files:**
- Create: `simulator/compat/miniz.h`
- Create: `simulator/compat/miniz.c`

**Step 1: Download or create minimal tinfl implementation**

Option A (recommended): Use the public domain miniz library's tinfl portion. Download `miniz.h` and `miniz.c` from https://github.com/richgel999/miniz and place in `simulator/compat/`.

Option B: Create a minimal header with just the tinfl types and functions needed by Canvas.cpp:
- `tinfl_decompressor` struct
- `tinfl_init()` macro
- `tinfl_decompress()` function
- `TINFL_FLAG_PARSE_ZLIB_HEADER`, `TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF` constants
- `TINFL_STATUS_DONE` etc. status codes

The miniz library is public domain and self-contained. Use the single-file version.

**Step 2: Commit**

```bash
git add simulator/compat/miniz.*
git commit -m "feat: add miniz compat for glyph decompression in simulator"
```

### Task 11: Create ESP-IDF API stub headers

These provide the ESP-IDF API signatures that components expect.

**Files:**
- Create: `simulator/stubs/include/esp_err.h`
- Create: `simulator/stubs/include/esp_log.h`
- Create: `simulator/stubs/include/esp_heap_caps.h`
- Create: `simulator/stubs/include/nvs_flash.h`
- Create: `simulator/stubs/include/nvs.h`
- Create: `simulator/stubs/include/esp_littlefs.h`
- Create: `simulator/stubs/include/mbedtls/md5.h`
- Create: `simulator/stubs/include/sdmmc_card.h`
- Create: `simulator/stubs/include/freertos/FreeRTOS.h` (minimal, for text_source)
- Create: `simulator/stubs/include/freertos/task.h`
- Create: `simulator/stubs/include/freertos/semphr.h`

**Step 1: esp_err.h**

```c
#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK                0
#define ESP_FAIL             -1
#define ESP_ERR_NOT_FOUND    0x105
#define ESP_ERR_NVS_NO_FREE_PAGES  0x1100
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1110
#define ESP_ERR_NVS_NOT_FOUND 0x1102
```

**Step 2: esp_log.h**

```c
#pragma once
#include <stdio.h>
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "E (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "W (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) fprintf(stderr, "I (%s) " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)
```

**Step 3: esp_heap_caps.h**

```c
#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 0
#define MALLOC_CAP_8BIT   0
static inline void* heap_caps_malloc(size_t size, uint32_t caps) {
    (void)caps;
    return malloc(size);
}
static inline void* heap_caps_calloc(size_t n, size_t size, uint32_t caps) {
    (void)caps;
    return calloc(n, size);
}
static inline void heap_caps_free(void* ptr) { free(ptr); }
```

**Step 4: nvs_flash.h and nvs.h**

nvs_flash.h:
```c
#pragma once
#include "esp_err.h"
esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);
```

nvs.h:
```c
#pragma once
#include "esp_err.h"
#include <stdint.h>
#include <stddef.h>

typedef uint32_t nvs_handle_t;
typedef enum { NVS_READWRITE, NVS_READONLY } nvs_open_mode_t;

esp_err_t nvs_open(const char* namespace_name, nvs_open_mode_t open_mode, nvs_handle_t* out_handle);
void nvs_close(nvs_handle_t handle);
esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, uint8_t* out_value);
esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* out_value, size_t* length);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t length);
esp_err_t nvs_commit(nvs_handle_t handle);
```

**Step 5: esp_littlefs.h**

```c
#pragma once
#include "esp_err.h"
typedef struct {
    const char* base_path;
    const char* partition_label;
    size_t max_files;
    bool format_if_mount_failed;
} esp_vfs_littlefs_conf_t;

static inline esp_err_t esp_vfs_littlefs_register(const esp_vfs_littlefs_conf_t* conf) {
    (void)conf;
    return ESP_OK;
}
static inline esp_err_t esp_littlefs_info(const char* label, size_t* total, size_t* used) {
    (void)label;
    if (total) *total = 16 * 1024 * 1024;
    if (used) *used = 0;
    return ESP_OK;
}
```

**Step 6: mbedtls/md5.h**

Provide a minimal MD5 implementation (settings_store uses it for path → NVS key hashing).

```c
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t buffer[64];
} mbedtls_md5_context;

void mbedtls_md5_init(mbedtls_md5_context *ctx);
void mbedtls_md5_free(mbedtls_md5_context *ctx);
int mbedtls_md5_starts(mbedtls_md5_context *ctx);
int mbedtls_md5_update(mbedtls_md5_context *ctx, const unsigned char *input, size_t ilen);
int mbedtls_md5_finish(mbedtls_md5_context *ctx, unsigned char output[16]);

// Deprecated but used by settings_store
#define mbedtls_md5_starts_ret mbedtls_md5_starts
#define mbedtls_md5_update_ret mbedtls_md5_update
#define mbedtls_md5_finish_ret mbedtls_md5_finish
```

**Step 7: FreeRTOS stubs** (for text_source/EncodingConverter which use xTaskCreate directly)

`freertos/FreeRTOS.h`:
```c
#pragma once
#include <stdint.h>
#include <stddef.h>
typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;
typedef void* QueueHandle_t;
typedef int32_t BaseType_t;
typedef uint32_t TickType_t;
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS pdTRUE
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (ms)
#define configSTACK_DEPTH_TYPE uint32_t
```

`freertos/task.h`:
```c
#pragma once
#include "FreeRTOS.h"
BaseType_t xTaskCreate(void(*fn)(void*), const char* name,
                        configSTACK_DEPTH_TYPE stack, void* arg,
                        int priority, TaskHandle_t* handle);
void vTaskDelete(TaskHandle_t handle);
void vTaskDelay(TickType_t ticks);
```

`freertos/semphr.h`:
```c
#pragma once
#include "FreeRTOS.h"
SemaphoreHandle_t xSemaphoreCreateBinary(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t* woken);
void vSemaphoreDelete(SemaphoreHandle_t sem);
```

**Step 8: Commit**

```bash
git add simulator/stubs/include/
git commit -m "feat: add ESP-IDF API stub headers for simulator"
```

### Task 12: Create ESP-IDF API stub implementations

**Files:**
- Create: `simulator/stubs/sim_nvs.c`
- Create: `simulator/stubs/sim_freertos.c`
- Create: `simulator/stubs/sim_md5.c`
- Create: `simulator/stubs/sim_sd_storage.c`
- Create: `simulator/stubs/sim_board.c`
- Create: `simulator/stubs/sim_battery.c`

**Step 1: sim_nvs.c — NVS → in-memory key-value store**

Implement a simple in-memory hash map with JSON file persistence:
- `nvs_flash_init()` → load JSON from `simulator/data/settings.json` and `progress.json`
- `nvs_open(namespace, ...)` → select the right internal map by namespace name
- `nvs_get_u8/set_u8` → read/write from map
- `nvs_get_blob/set_blob` → read/write binary data (hex-encoded in JSON)
- `nvs_commit()` → write map back to JSON file

For simplicity, can use a flat array of `{namespace, key, value_bytes}` entries with linear scan. The data volume is tiny (a few settings + a few progress entries).

**Step 2: sim_freertos.c — FreeRTOS primitives via pthreads**

```c
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

BaseType_t xTaskCreate(void(*fn)(void*), const char* name,
                        configSTACK_DEPTH_TYPE stack, void* arg,
                        int priority, TaskHandle_t* handle) {
    pthread_t* thread = malloc(sizeof(pthread_t));
    pthread_create(thread, NULL, (void*(*)(void*))fn, arg);
    if (handle) *handle = thread;
    return pdPASS;
}

void vTaskDelete(TaskHandle_t handle) {
    if (handle) {
        pthread_cancel(*(pthread_t*)handle);
        free(handle);
    }
}

void vTaskDelay(TickType_t ticks) {
    usleep(ticks * 1000);  // ticks = ms in simulator
}

// Semaphore: use pthread mutex + condition variable
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
} sim_sem_t;

SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    sim_sem_t* sem = calloc(1, sizeof(sim_sem_t));
    pthread_mutex_init(&sem->mutex, NULL);
    pthread_cond_init(&sem->cond, NULL);
    sem->count = 0;
    return sem;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t handle, TickType_t timeout) {
    sim_sem_t* sem = (sim_sem_t*)handle;
    pthread_mutex_lock(&sem->mutex);
    while (sem->count == 0) {
        if (timeout == portMAX_DELAY) {
            pthread_cond_wait(&sem->cond, &sem->mutex);
        } else {
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += timeout / 1000;
            ts.tv_nsec += (timeout % 1000) * 1000000;
            if (ts.tv_nsec >= 1000000000) { ts.tv_sec++; ts.tv_nsec -= 1000000000; }
            int ret = pthread_cond_timedwait(&sem->cond, &sem->mutex, &ts);
            if (ret != 0) { pthread_mutex_unlock(&sem->mutex); return pdFALSE; }
        }
    }
    sem->count--;
    pthread_mutex_unlock(&sem->mutex);
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t handle) {
    sim_sem_t* sem = (sim_sem_t*)handle;
    pthread_mutex_lock(&sem->mutex);
    sem->count = 1;
    pthread_cond_signal(&sem->cond);
    pthread_mutex_unlock(&sem->mutex);
    return pdTRUE;
}

BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t handle, BaseType_t* woken) {
    if (woken) *woken = pdFALSE;
    return xSemaphoreGive(handle);
}

void vSemaphoreDelete(SemaphoreHandle_t handle) {
    sim_sem_t* sem = (sim_sem_t*)handle;
    pthread_mutex_destroy(&sem->mutex);
    pthread_cond_destroy(&sem->cond);
    free(sem);
}
```

**Step 3: sim_md5.c — Minimal MD5 implementation**

Implement the standard MD5 algorithm (RFC 1321). This is well-known public domain code (~150 lines). Many reference implementations exist. Or use a permissively-licensed one.

**Step 4: sim_sd_storage.c, sim_board.c, sim_battery.c**

sim_sd_storage.c:
```c
#include "sd_storage.h"
#include <stdbool.h>
static bool s_mounted = false;
esp_err_t sd_storage_mount(const sd_storage_config_t *config) {
    (void)config; s_mounted = true; return ESP_OK;
}
void sd_storage_unmount(void) { s_mounted = false; }
bool sd_storage_is_mounted(void) { return s_mounted; }
```

sim_board.c:
```c
#include "board.h"
void board_init(void) {}
```

Note: `board.h` defines GPIO pin constants. The simulator needs a stub that provides these constants without doing anything.

sim_battery.c:
```c
// If battery.h is included directly by some components
#include "battery.h"
void battery_init(void) {}
int battery_get_percent(void) { return 100; }
```

**Step 5: Commit**

```bash
git add simulator/stubs/
git commit -m "feat: add ESP-IDF API stub implementations"
```

### Task 13: Implement SdlDisplayDriver

**Files:**
- Create: `simulator/src/SdlDisplayDriver.h`
- Create: `simulator/src/SdlDisplayDriver.cpp`

**Step 1: SdlDisplayDriver.h**

```cpp
#pragma once

#include "ink_ui/hal/DisplayDriver.h"
#include <SDL.h>

/// SDL2 实现的显示驱动
class SdlDisplayDriver : public ink::DisplayDriver {
public:
    explicit SdlDisplayDriver(int scale = 1);
    ~SdlDisplayDriver();

    void init() override;
    uint8_t* framebuffer() override;
    void updateArea(int x, int y, int w, int h, ink::RefreshMode mode) override;
    void updateScreen() override;
    void fullClear() override;
    void setAllWhite() override;
    int width() const override;
    int height() const override;

    /// 处理 SDL 窗口事件 (resize, close)
    bool handleWindowEvent(const SDL_Event& event);

private:
    static constexpr int kPhysWidth = 960;
    static constexpr int kPhysHeight = 540;
    static constexpr int kLogicalWidth = 540;   // portrait
    static constexpr int kLogicalHeight = 960;
    static constexpr int kFbSize = kPhysWidth / 2 * kPhysHeight;  // 259200 bytes

    int scale_;
    uint8_t* fb_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;

    /// 将 framebuffer 物理区域转换并更新到 SDL texture
    void blitToTexture(int physX, int physY, int physW, int physH);

    /// 将全部 framebuffer 更新到 SDL texture 并 present
    void presentFullScreen();
};
```

**Step 2: SdlDisplayDriver.cpp**

Key implementation:
- `init()`: allocate `fb_` (259200 bytes via malloc), create SDL window (kLogicalWidth*scale × kLogicalHeight*scale), create renderer + streaming texture
- `framebuffer()` → return `fb_`
- `updateArea(x,y,w,h,mode)` → `blitToTexture(x,y,w,h)` + `SDL_RenderPresent()`
- `updateScreen()` → `presentFullScreen()`
- `fullClear()` / `setAllWhite()` → memset fb to 0xFF + present
- `blitToTexture()`:
  - Iterate over the physical area pixels
  - For each physical pixel (px, py): extract 4bpp gray from fb
  - Map to logical coordinates: `lx = kPhysHeight - 1 - py`, `ly = px` (inverse of Canvas::setPixel transform)
  - Write 32-bit ARGB pixel to a temporary pixel buffer
  - SDL_UpdateTexture with the logical rect
  - SDL_RenderCopy + SDL_RenderPresent

**Step 3: Commit**

```bash
git add simulator/src/SdlDisplayDriver.*
git commit -m "feat: implement SdlDisplayDriver (4bpp framebuffer → SDL window)"
```

### Task 14: Implement DesktopPlatform

**Files:**
- Create: `simulator/src/DesktopPlatform.h`
- Create: `simulator/src/DesktopPlatform.cpp`

**Step 1: Create DesktopPlatform**

Implement `ink::Platform` using C++ standard library:

```cpp
#pragma once

#include "ink_ui/hal/Platform.h"
#include <deque>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <vector>
#include <cstring>

/// 线程安全的固定元素大小队列
struct SimQueue {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::vector<uint8_t>> items;
    int maxItems;
    int itemSize;
};

class DesktopPlatform : public ink::Platform {
public:
    ink::QueueHandle createQueue(int maxItems, int itemSize) override;
    bool queueSend(ink::QueueHandle q, const void* item, uint32_t timeout_ms) override;
    bool queueReceive(ink::QueueHandle q, void* item, uint32_t timeout_ms) override;
    ink::TaskHandle createTask(void(*func)(void*), const char* name,
                                int stackSize, void* arg, int priority) override;
    void startOneShotTimer(uint32_t delay_ms, ink::TimerCallback cb, void* arg) override;
    int64_t getTimeUs() override;
    void delayMs(uint32_t ms) override;
};
```

Implementation:
- `createQueue`: `new SimQueue{maxItems, itemSize}`
- `queueSend`: lock mutex, push copy of item bytes, signal cv
- `queueReceive`: lock mutex, wait on cv with timeout, pop front
- `createTask`: `new std::thread(func, arg)`, detach
- `startOneShotTimer`: spawn detached `std::thread` that sleeps then calls callback
- `getTimeUs`: `std::chrono::steady_clock::now()` since epoch in microseconds
- `delayMs`: `std::this_thread::sleep_for`

**Step 2: Commit**

```bash
git add simulator/src/DesktopPlatform.*
git commit -m "feat: implement DesktopPlatform (queues, tasks, timers via std::thread)"
```

### Task 15: Implement SdlTouchDriver

**Files:**
- Create: `simulator/src/SdlTouchDriver.h`
- Create: `simulator/src/SdlTouchDriver.cpp`

**Step 1: Create SdlTouchDriver**

```cpp
#pragma once
#include "ink_ui/hal/TouchDriver.h"
#include <SDL.h>
#include <atomic>

class SdlTouchDriver : public ink::TouchDriver {
public:
    explicit SdlTouchDriver(int windowScale = 1);
    void init() override;
    bool waitForTouch(uint32_t timeout_ms) override;
    ink::TouchPoint read() override;

private:
    int scale_;
    std::atomic<bool> pressed_{false};
    std::atomic<int> lastX_{0};
    std::atomic<int> lastY_{0};
    SDL_Event pendingEvent_;
    bool hasPendingEvent_ = false;
};
```

Implementation:
- `init()` → no-op
- `waitForTouch(timeout_ms)`:
  - `SDL_WaitEventTimeout(&event, timeout_ms)`
  - Filter for `SDL_MOUSEBUTTONDOWN`, `SDL_MOUSEMOTION` (while pressed), `SDL_MOUSEBUTTONUP`
  - Also handle `SDL_QUIT` → exit
  - Store event, update `pressed_`, `lastX_`, `lastY_`
  - Mouse coordinates in SDL window are logical (portrait), divide by scale
- `read()`:
  - Return `{lastX_, lastY_, pressed_}` as TouchPoint

Note: `waitForTouch` and `read` will be called from the touch task thread, while SDL events must be polled from the main thread. Solution: SDL event polling happens in the main loop and pushes touch data via a shared atomic/mutex. Or: use SDL's thread-safe event pushing.

Better approach: Use a separate thread-safe queue. The main thread polls SDL events and pushes mouse events to a sim touch queue. The touch driver's `waitForTouch()` pops from this queue.

**Step 2: Commit**

```bash
git add simulator/src/SdlTouchDriver.*
git commit -m "feat: implement SdlTouchDriver (mouse events → touch points)"
```

### Task 16: Implement DesktopFontProvider

**Files:**
- Create: `simulator/src/DesktopFontProvider.h`
- Create: `simulator/src/DesktopFontProvider.cpp`

**Step 1: Create DesktopFontProvider**

```cpp
#pragma once
#include "ink_ui/hal/FontProvider.h"
#include <string>

class DesktopFontProvider : public ink::FontProvider {
public:
    explicit DesktopFontProvider(const std::string& fontsDir);
    void init() override;
    const EpdFont* getFont(int sizePx) override;
private:
    std::string fontsDir_;
};
```

Implementation: delegates to `ui_font_init()` and `ui_font_get()` from `ui_core`. But `ui_font_init()` tries to mount LittleFS, which our stub makes a no-op. The issue is the path: `ui_font_init()` scans `/fonts/` directory.

Solution: Before calling `ui_font_init()`, create a symlink `/fonts` → `simulator/fonts/` or modify `FONTS_MOUNT_POINT` via compile-time define. Simplest: add a `#ifdef SIMULATOR` in `ui_font.c` to use a configurable path, or set up the directory mapping.

Actually, the simplest approach: `esp_vfs_littlefs_register` stub is already a no-op. The font code then calls `opendir("/fonts")`. On Linux, if we create a symlink or just set the font path appropriately, it works. Since the simulator runs from `simulator/build/`, we can use a relative path.

Better: Add a `FONTS_MOUNT_POINT` override. In `ui_font.c`, change:
```c
#ifndef FONTS_MOUNT_POINT
#define FONTS_MOUNT_POINT "/fonts"
#endif
```

Then in simulator CMake: `add_compile_definitions(FONTS_MOUNT_POINT="${CMAKE_SOURCE_DIR}/fonts")`

Similarly for `SD_MOUNT_POINT` in `sd_storage.h`.

**Step 2: Commit**

```bash
git add simulator/src/DesktopFontProvider.*
git commit -m "feat: implement DesktopFontProvider"
```

### Task 17: Implement DesktopSystemInfo

**Files:**
- Create: `simulator/src/DesktopSystemInfo.h`
- Create: `simulator/src/DesktopSystemInfo.cpp`

**Step 1: Create DesktopSystemInfo**

```cpp
#pragma once
#include "ink_ui/hal/SystemInfo.h"

class DesktopSystemInfo : public ink::SystemInfo {
public:
    int batteryPercent() override { return 100; }
    void getTimeString(char* buf, int bufSize) override;
};
```

Implementation:
```cpp
#include "DesktopSystemInfo.h"
#include <ctime>
#include <cstdio>

void DesktopSystemInfo::getTimeString(char* buf, int bufSize) {
    time_t now = time(nullptr);
    struct tm* tm = localtime(&now);
    snprintf(buf, bufSize, "%02d:%02d", tm->tm_hour, tm->tm_min);
}
```

**Step 2: Commit**

```bash
git add simulator/src/DesktopSystemInfo.*
git commit -m "feat: implement DesktopSystemInfo"
```

### Task 18: Path remapping for simulator

Make `FONTS_MOUNT_POINT` and `SD_MOUNT_POINT` configurable for the simulator.

**Files:**
- Modify: `components/ui_core/ui_font.c` — make `FONTS_MOUNT_POINT` overridable
- Modify: `components/sd_storage/include/sd_storage.h` — make `SD_MOUNT_POINT` overridable
- Modify: `components/book_store/book_store.c` — uses `SD_MOUNT_POINT`, should work via header

**Step 1: Update ui_font.c**

Change the hardcoded define to an overridable one:
```c
// At the top of ui_font.c, change:
#define FONTS_MOUNT_POINT "/fonts"
// To:
#ifndef FONTS_MOUNT_POINT
#define FONTS_MOUNT_POINT "/fonts"
#endif
```

**Step 2: Update sd_storage.h**

```c
// Change:
#define SD_MOUNT_POINT "/sdcard"
// To:
#ifndef SD_MOUNT_POINT
#define SD_MOUNT_POINT "/sdcard"
#endif
```

**Step 3: Update simulator CMakeLists.txt**

Add path overrides:
```cmake
# 路径重映射
add_compile_definitions(
    SIMULATOR=1
    FONTS_MOUNT_POINT="${CMAKE_SOURCE_DIR}/fonts"
    SD_MOUNT_POINT="${CMAKE_SOURCE_DIR}/data"
)
```

**Step 4: Commit**

```bash
git add components/ui_core/ui_font.c components/sd_storage/include/sd_storage.h simulator/CMakeLists.txt
git commit -m "feat: make mount point paths overridable for simulator"
```

---

## Phase 4: Integration

### Task 19: Create simulator main.cpp

**Files:**
- Create: `simulator/src/main.cpp`

**Step 1: Write simulator entry point**

```cpp
/**
 * @file main.cpp
 * @brief 模拟器入口 — 创建 HAL 实例, 初始化 SDL, 启动 InkUI Application。
 */

#include <cstdio>
#include <SDL.h>

#include "SdlDisplayDriver.h"
#include "SdlTouchDriver.h"
#include "DesktopPlatform.h"
#include "DesktopFontProvider.h"
#include "DesktopSystemInfo.h"

#include "ink_ui/InkUI.h"
#include "controllers/BootViewController.h"

extern "C" {
#include "settings_store.h"
#include "ui_font.h"
}

static ink::Application app;

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // SDL 初始化
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    // 创建 HAL 实例
    int scale = 1;  // TODO: 可从命令行参数获取
    SdlDisplayDriver display(scale);
    SdlTouchDriver touch(scale);
    DesktopPlatform platform;
    DesktopFontProvider fonts(FONTS_MOUNT_POINT);
    DesktopSystemInfo system;

    // 初始化存储
    settings_store_init();

    // 初始化字体
    fonts.init();

    // 初始化 InkUI
    if (!app.init(display, touch, platform, system)) {
        fprintf(stderr, "Application init failed\n");
        return 1;
    }

    // 设置 StatusBar 字体和电池
    app.statusBar()->setFont(ui_font_get(16));
    app.statusBar()->updateBattery(system.batteryPercent());

    // 推入启动页面
    app.navigator().push(std::make_unique<BootViewController>(app));

    // 运行主循环（不会返回）
    app.run();

    SDL_Quit();
    return 0;
}
```

Note: The SDL event loop integration needs special care. `app.run()` currently blocks on `xQueueReceive` (now `platform.queueReceive`). SDL events need to be polled from somewhere. Options:

**Option A**: Poll SDL events in the touch driver thread, inject touch events into the app's event queue.

**Option B**: Modify the main loop to also poll SDL events. But `app.run()` is `[[noreturn]]` and blocks.

**Recommended approach**: SDL event polling runs in the main thread. The Application's `run()` loop calls `platform.queueReceive()` which blocks the main thread. Meanwhile, a separate thread handles SDL event polling and injects events.

BUT: SDL2 requires event polling from the main thread on some platforms. For Linux this is less strict, but for safety, we should poll SDL events from the main thread.

**Best approach**: In the simulator, override the main loop. Instead of calling `app.run()` (which is a blocking FreeRTOS-style loop), create a simulator-specific main loop that:
1. Polls SDL events (mouse → touch queue)
2. Calls `platform.queueReceive()` with short timeout
3. Dispatches events and renders

This can be done by not calling `app.run()` directly but instead calling `app.processOneEvent()` or similar. However, `run()` is currently `[[noreturn]]` with an infinite loop.

**Simplest approach**: Add an `#ifdef SIMULATOR` path in Application::run() that calls SDL_PollEvent, or have the touch driver use a custom SDL event pump in a thread. On Linux, SDL event polling from non-main thread works.

Actually, for the simulator, let's have SDL event polling in a dedicated thread that pushes to the touch driver's internal queue. The main thread runs `app.run()` normally. The touch driver's `waitForTouch()` blocks on its internal queue. SDL thread polls events and feeds the touch driver.

This needs a coordinating mechanism in the simulator main.cpp.

**Step 2: Commit**

```bash
git add simulator/src/main.cpp
git commit -m "feat: create simulator entry point"
```

### Task 20: Create additional stub headers

During compilation, additional ESP-IDF headers may be needed. Create stubs as build errors reveal them.

**Files to potentially create:**
- `simulator/stubs/include/battery.h`
- `simulator/stubs/include/board.h`
- `simulator/stubs/include/driver/gpio.h`
- `simulator/stubs/include/driver/i2c.h`
- `simulator/stubs/include/esp_timer.h`

**Step 1: Create stub headers as needed**

battery.h:
```c
#pragma once
void battery_init(void);
int battery_get_percent(void);
```

board.h — copy the pin constant defines from the real `main/board.h`, with stub `board_init()`:
```c
#pragma once
#ifdef __cplusplus
extern "C" {
#endif
void board_init(void);
// GPIO pin constants (not used in simulator but needed for compilation)
#define BOARD_I2C_SDA   11
#define BOARD_I2C_SCL   12
#define BOARD_TOUCH_INT 48
#define BOARD_SPI_MISO  13
#define BOARD_SPI_MOSI  14
#define BOARD_SPI_CLK   15
#define BOARD_SPI_CS    10
#ifdef __cplusplus
}
#endif
```

esp_timer.h:
```c
#pragma once
#include <stdint.h>
static inline int64_t esp_timer_get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}
```

**Step 2: Commit iteratively as build errors are found**

### Task 21: Build, fix, and iterate

**Step 1: Attempt first build**

```bash
cd simulator && mkdir -p build && cd build
cmake .. 2>&1 | head -50
make -j$(nproc) 2>&1 | head -100
```

**Step 2: Fix compilation errors**

Common expected issues:
1. Missing stub headers — create them
2. Include path issues — fix in CMakeLists.txt
3. Type mismatches between HAL interfaces and existing code — fix signatures
4. `#ifdef SIMULATOR` guards needed in some component code
5. FreeRTOS types used directly in components (text_source, ReaderContentView) — ensure FreeRTOS stub headers are complete

**Step 3: Fix linker errors**

Common expected issues:
1. Missing function implementations in stubs
2. Duplicate symbol definitions (header-defined inline functions linked multiple times)
3. Missing SDL2 linking

**Step 4: Iterate until clean build**

**Step 5: Run and verify**

```bash
cd simulator/build
./parchment_sim
```

Expected: SDL window opens showing 540x960 portrait, boot screen appears (Parchment logo + progress bar). If fonts are missing, the text won't render but the layout should still be visible.

**Step 6: Final commit**

```bash
git add -A simulator/
git commit -m "feat: SDL simulator builds and runs"
```

---

## Phase 5: Verification and Polish

### Task 22: Test with real fonts and books

**Step 1: Copy font files**

```bash
# From device or fonts_data directory
cp fonts_data/*.pfnt simulator/fonts/
```

**Step 2: Add test book**

```bash
echo "这是一本测试书籍。\n第二段内容。" > simulator/data/book/test.txt
```

**Step 3: Run and verify full flow**

```bash
cd simulator/build && ./parchment_sim
```

Verify:
- [ ] Boot screen shows with text rendering
- [ ] Auto-transition to Library page
- [ ] Book list displays the test book
- [ ] Clicking on book opens Reader page
- [ ] Text renders correctly
- [ ] Mouse left/right tap navigation works
- [ ] Back navigation (swipe) works

### Task 23: Clean up old simulator artifacts

**Step 1: Ensure old build directory is removed**

```bash
# Already done in Task 8, but verify
ls simulator/build/  # Should only contain new CMake artifacts
```

**Step 2: Add .gitignore for simulator build**

```bash
echo "build/" > simulator/.gitignore
git add simulator/.gitignore
git commit -m "chore: add .gitignore for simulator build directory"
```

---

## Summary: File Change Inventory

### New Files (~25 files)

| Directory | Files |
|---|---|
| `components/ink_ui/include/ink_ui/hal/` | DisplayDriver.h, TouchDriver.h, Platform.h, FontProvider.h, SystemInfo.h |
| `components/ink_ui/include/ink_ui/hal/` | Esp32TouchDriver.h, Esp32Platform.h, Esp32SystemInfo.h |
| `components/ink_ui/src/hal/` | Esp32TouchDriver.cpp, Esp32Platform.cpp, Esp32SystemInfo.cpp |
| `simulator/src/` | main.cpp, SdlDisplayDriver.h/cpp, SdlTouchDriver.h/cpp, DesktopPlatform.h/cpp, DesktopFontProvider.h/cpp, DesktopSystemInfo.h/cpp |
| `simulator/stubs/` | sim_nvs.c, sim_freertos.c, sim_md5.c, sim_sd_storage.c, sim_board.c, sim_battery.c |
| `simulator/stubs/include/` | esp_err.h, esp_log.h, esp_heap_caps.h, nvs.h, nvs_flash.h, esp_littlefs.h, esp_timer.h, battery.h, board.h, mbedtls/md5.h, freertos/*.h |
| `simulator/compat/` | epdiy.h, epdiy_font.c, miniz.h/c |
| `simulator/` | CMakeLists.txt, .gitignore |

### Modified Files (~10 files)

| File | Change |
|---|---|
| `components/ink_ui/include/ink_ui/core/Application.h` | init() accepts HAL refs |
| `components/ink_ui/src/core/Application.cpp` | Use HAL instead of FreeRTOS/ESP |
| `components/ink_ui/include/ink_ui/core/RenderEngine.h` | DisplayDriver& instead of EpdDriver& |
| `components/ink_ui/src/core/RenderEngine.cpp` | Use DisplayDriver API |
| `components/ink_ui/include/ink_ui/core/GestureRecognizer.h` | TouchDriver + Platform HAL |
| `components/ink_ui/src/core/GestureRecognizer.cpp` | Use HAL instead of GT911/FreeRTOS |
| `components/ink_ui/include/ink_ui/core/EpdDriver.h` | Extends DisplayDriver |
| `components/ink_ui/src/core/EpdDriver.cpp` | Implement new virtual methods |
| `components/ink_ui/CMakeLists.txt` | Add HAL source files |
| `main/main.cpp` | Create HAL instances, inject into app |
| `components/ui_core/ui_font.c` | Overridable FONTS_MOUNT_POINT |
| `components/sd_storage/include/sd_storage.h` | Overridable SD_MOUNT_POINT |
