# InkUI Performance Profiling Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add conditional performance profiling to InkUI's full rendering pipeline, from touch event to EPD screen refresh, to identify bottlenecks.

**Architecture:** Macro-based instrumentation with `CONFIG_INKUI_PROFILE` Kconfig switch. Method A (pipeline stage timers) instruments RenderEngine, EpdDriver, Canvas, and Application. Method B (event tracing) adds timestamps to Event struct for end-to-end latency tracking. All profiling code compiles to nothing when disabled.

**Tech Stack:** ESP-IDF v5.5.1, C++17, `esp_timer_get_time()` for microsecond timestamps, `ESP_LOGI` for output, Kconfig for build switch.

---

### Task 1: Create Profiler.h and Kconfig foundation

**Files:**
- Create: `components/ink_ui/include/ink_ui/core/Profiler.h`
- Create: `components/ink_ui/Kconfig`

**Step 1: Create Profiler.h**

```cpp
/**
 * @file Profiler.h
 * @brief InkUI 性能 Profiling 宏，通过 CONFIG_INKUI_PROFILE 条件编译控制。
 */

#pragma once

#include "sdkconfig.h"

#ifdef CONFIG_INKUI_PROFILE

#include "esp_timer.h"
#include "esp_log.h"

/// 开始计时（声明局部变量）
#define INKUI_PROFILE_BEGIN(name) \
    int64_t _prof_##name##_start = esp_timer_get_time()

/// 结束计时（声明结束时刻变量）
#define INKUI_PROFILE_END(name) \
    int64_t _prof_##name##_end = esp_timer_get_time()

/// 获取耗时（微秒）
#define INKUI_PROFILE_US(name) \
    ((int)(_prof_##name##_end - _prof_##name##_start))

/// 获取耗时（毫秒）
#define INKUI_PROFILE_MS(name) \
    ((int)((_prof_##name##_end - _prof_##name##_start) / 1000))

/// 获取当前时间戳（微秒）
#define INKUI_PROFILE_NOW() esp_timer_get_time()

/// 计算两个时间戳差值（毫秒）
#define INKUI_PROFILE_DIFF_MS(start, end) ((int)((end) - (start)) / 1000)

/// Profiling 日志输出
#define INKUI_PROFILE_LOG(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)

#else  // !CONFIG_INKUI_PROFILE

#define INKUI_PROFILE_BEGIN(name)
#define INKUI_PROFILE_END(name)
#define INKUI_PROFILE_US(name) 0
#define INKUI_PROFILE_MS(name) 0
#define INKUI_PROFILE_NOW() 0
#define INKUI_PROFILE_DIFF_MS(start, end) 0
#define INKUI_PROFILE_LOG(tag, fmt, ...)

#endif  // CONFIG_INKUI_PROFILE
```

**Step 2: Create Kconfig**

```
menu "InkUI Configuration"
    config INKUI_PROFILE
        bool "Enable InkUI performance profiling"
        default n
        help
            Enable profiling instrumentation in InkUI rendering pipeline.
            Outputs per-stage timing to serial log via ESP_LOGI.
            When disabled, all profiling macros compile to nothing.
endmenu
```

**Step 3: Build verification**

Run: `idf.py build`
Expected: Build succeeds. No new code references the macros yet, so no behavior change.

**Step 4: Commit**

```bash
git add components/ink_ui/include/ink_ui/core/Profiler.h components/ink_ui/Kconfig
git commit -m "feat(ink_ui): add Profiler.h macros and Kconfig for performance profiling"
```

---

### Task 2: Add timestamp to Event struct and GestureRecognizer

**Files:**
- Modify: `components/ink_ui/include/ink_ui/core/Event.h` (lines 59-99)
- Modify: `components/ink_ui/src/core/GestureRecognizer.cpp` (line 194-196)

**Step 1: Add timestampUs to Event struct**

In `Event.h`, add a conditional `timestampUs` field and a `stamp()` helper **after the union, before the factory methods**:

```cpp
// After line 66 (closing brace of union), before factory methods:
#ifdef CONFIG_INKUI_PROFILE
    int64_t timestampUs = 0;  ///< 事件创建时刻 (profiling only)
#endif
```

Also add `#include "sdkconfig.h"` at the top of the file (after `#include <cstdint>`).

**Step 2: Stamp events in GestureRecognizer::sendEvent()**

In `GestureRecognizer.cpp`, modify `sendEvent()` at line 194:

```cpp
void GestureRecognizer::sendEvent(const Event& event) {
#ifdef CONFIG_INKUI_PROFILE
    Event stamped = event;
    stamped.timestampUs = platform_.getTimeUs();
    platform_.queueSend(eventQueue_, &stamped, 10);
#else
    platform_.queueSend(eventQueue_, &event, 10);
#endif
}
```

Add `#include "ink_ui/core/Profiler.h"` at the top (after existing includes).

**Step 3: Build verification**

Run: `idf.py build`
Expected: Build succeeds. Event struct grows by 8 bytes in profile mode but queue uses `sizeof(Event)` so auto-adapts.

**Step 4: Commit**

```bash
git add components/ink_ui/include/ink_ui/core/Event.h components/ink_ui/src/core/GestureRecognizer.cpp
git commit -m "feat(ink_ui): add profiling timestamp to Event struct"
```

---

### Task 3: Instrument RenderEngine with per-phase timing

**Files:**
- Modify: `components/ink_ui/include/ink_ui/core/RenderEngine.h` (add profiling member vars)
- Modify: `components/ink_ui/src/core/RenderEngine.cpp` (instrument renderCycle, drawView)

**Step 1: Add profiling stats to RenderEngine.h**

Add after line 41 (`int dirtyCount_ = 0;`), inside the `private:` section:

```cpp
#ifdef CONFIG_INKUI_PROFILE
    // 绘制阶段累计统计（每次 renderCycle 重置）
    int64_t profClearUs_ = 0;    ///< canvas.clear() 累计耗时
    int64_t profOnDrawUs_ = 0;   ///< view->onDraw() 累计耗时
    int profViewCount_ = 0;      ///< 绘制的 View 数量
#endif
```

Add `#include "ink_ui/core/Profiler.h"` at top.

**Step 2: Instrument renderCycle() in RenderEngine.cpp**

Replace the `renderCycle()` method (lines 30-49) with:

```cpp
void RenderEngine::renderCycle(View* rootView) {
    if (!rootView) return;

    // Phase 1: Layout Pass
    INKUI_PROFILE_BEGIN(layout);
    layoutPass(rootView);
    INKUI_PROFILE_END(layout);

    // Phase 2: Collect Dirty
    INKUI_PROFILE_BEGIN(collect);
    dirtyCount_ = 0;
    collectDirty(rootView);
    INKUI_PROFILE_END(collect);

    if (dirtyCount_ == 0) {
        return;  // 无脏区域，跳过 Phase 3-5
    }

    // Phase 3: Draw Dirty
#ifdef CONFIG_INKUI_PROFILE
    profClearUs_ = 0;
    profOnDrawUs_ = 0;
    profViewCount_ = 0;
#endif

    INKUI_PROFILE_BEGIN(draw);
    drawDirty(rootView);
    INKUI_PROFILE_END(draw);

    // Phase 4: Flush
    INKUI_PROFILE_BEGIN(flush);
    flush();
    INKUI_PROFILE_END(flush);

#ifdef CONFIG_INKUI_PROFILE
    // 计算最大脏区域尺寸用于日志
    int maxW = 0, maxH = 0;
    for (int i = 0; i < dirtyCount_; i++) {
        if (dirtyRegions_[i].rect.w > maxW) maxW = dirtyRegions_[i].rect.w;
        if (dirtyRegions_[i].rect.h > maxH) maxH = dirtyRegions_[i].rect.h;
    }
    INKUI_PROFILE_LOG("PERF", "cycle: layout=%dms collect=%dms draw=%dms flush=%dms total=%dms dirty=%d (%dx%d)",
        INKUI_PROFILE_MS(layout), INKUI_PROFILE_MS(collect),
        INKUI_PROFILE_MS(draw), INKUI_PROFILE_MS(flush),
        INKUI_PROFILE_MS(layout) + INKUI_PROFILE_MS(collect) +
        INKUI_PROFILE_MS(draw) + INKUI_PROFILE_MS(flush),
        dirtyCount_, maxW, maxH);
    INKUI_PROFILE_LOG("PERF", "  draw: clear=%dms onDraw=%dms views=%d",
        (int)(profClearUs_ / 1000), (int)(profOnDrawUs_ / 1000), profViewCount_);
#endif
}
```

Add `#include "ink_ui/core/Profiler.h"` at top of the .cpp file.

**Step 3: Instrument drawView() for clear/onDraw breakdown**

Replace the `drawView()` method (lines 80-114) with:

```cpp
void RenderEngine::drawView(View* view, bool forced) {
    if (view->isHidden()) return;

    bool shouldDraw = forced || view->needsDisplay();

    if (shouldDraw) {
        Rect sf = view->screenFrame();
        Canvas canvas(fb_, sf);

#ifdef CONFIG_INKUI_PROFILE
        int64_t clearStart = INKUI_PROFILE_NOW();
#endif
        if (view->backgroundColor() != Color::Clear) {
            canvas.clear(view->backgroundColor());
        } else if (!forced && view->isOpaque()) {
            uint8_t inheritedBg = Color::White;
            for (View* p = view->parent(); p; p = p->parent()) {
                if (p->backgroundColor() != Color::Clear) {
                    inheritedBg = p->backgroundColor();
                    break;
                }
            }
            canvas.clear(inheritedBg);
        }
#ifdef CONFIG_INKUI_PROFILE
        int64_t clearEnd = INKUI_PROFILE_NOW();
        profClearUs_ += (clearEnd - clearStart);
#endif

#ifdef CONFIG_INKUI_PROFILE
        int64_t drawStart = INKUI_PROFILE_NOW();
#endif
        view->onDraw(canvas);
#ifdef CONFIG_INKUI_PROFILE
        int64_t drawEnd = INKUI_PROFILE_NOW();
        profOnDrawUs_ += (drawEnd - drawStart);
        profViewCount_++;
#endif

        for (auto& child : view->subviews()) {
            drawView(child.get(), true);
        }
    } else if (view->subtreeNeedsDisplay()) {
        for (auto& child : view->subviews()) {
            drawView(child.get(), false);
        }
    }
}
```

**Step 4: Build verification**

Run: `idf.py build`
Expected: Build succeeds.

**Step 5: Commit**

```bash
git add components/ink_ui/include/ink_ui/core/RenderEngine.h components/ink_ui/src/core/RenderEngine.cpp
git commit -m "feat(ink_ui): instrument RenderEngine with per-phase profiling"
```

---

### Task 4: Instrument EpdDriver with refresh timing

**Files:**
- Modify: `components/ink_ui/src/core/EpdDriver.cpp` (lines 45-51, updateArea with RefreshMode)

**Step 1: Add profiling to EpdDriver::updateArea()**

Modify the `updateArea(int x, int y, int w, int h, RefreshMode mode)` method at line 45:

```cpp
bool EpdDriver::updateArea(int x, int y, int w, int h, RefreshMode mode) {
    if (!initialized_) {
        return false;
    }
    int epdiyMode = static_cast<int>(toEpdMode(mode));

#ifdef CONFIG_INKUI_PROFILE
    const char* modeName = "?";
    switch (mode) {
        case RefreshMode::Full: modeName = "GL16"; break;
        case RefreshMode::Fast: modeName = "DU"; break;
        case RefreshMode::Clear: modeName = "GC16"; break;
    }
    INKUI_PROFILE_BEGIN(epd);
#endif

    bool ok = epd_driver_update_area_mode(x, y, w, h, epdiyMode) == ESP_OK;

#ifdef CONFIG_INKUI_PROFILE
    INKUI_PROFILE_END(epd);
    INKUI_PROFILE_LOG("PERF", "  epd: area=(%d,%d,%d,%d) mode=%s time=%dms",
        x, y, w, h, modeName, INKUI_PROFILE_MS(epd));
#endif

    return ok;
}
```

Add `#include "ink_ui/core/Profiler.h"` at top of the .cpp file.

**Step 2: Build verification**

Run: `idf.py build`
Expected: Build succeeds.

**Step 3: Commit**

```bash
git add components/ink_ui/src/core/EpdDriver.cpp
git commit -m "feat(ink_ui): instrument EpdDriver::updateArea with profiling"
```

---

### Task 5: Instrument Application event loop with end-to-end tracing

**Files:**
- Modify: `components/ink_ui/src/core/Application.cpp` (lines 174-215 run(), lines 230-273 dispatchEvent())

**Step 1: Add profiling to run() and dispatchEvent()**

Add `#include "ink_ui/core/Profiler.h"` at top of Application.cpp.

Modify `run()` at lines 174-215. The key change: track event timestamp + dispatch time, then log after renderCycle if an event was processed:

```cpp
void Application::run() {
    fprintf(stderr, "ink::App: Entering main event loop\n");

    // 初始渲染
    if (screenRoot_) {
        renderEngine_->renderCycle(screenRoot_.get());
    }

    while (true) {
        Event event;
        bool received = platform_->queueReceive(
            eventQueue_, &event, kQueueTimeoutMs);

#ifdef CONFIG_INKUI_PROFILE
        int64_t dispatchStartUs = 0;
        int64_t dispatchEndUs = 0;
        int64_t eventTimestampUs = 0;
        const char* eventTypeName = nullptr;
#endif

        if (received) {
#ifdef CONFIG_INKUI_PROFILE
            dispatchStartUs = INKUI_PROFILE_NOW();
            eventTimestampUs = event.timestampUs;
            switch (event.type) {
                case EventType::Touch:
                    switch (event.touch.type) {
                        case TouchType::Tap:       eventTypeName = "Tap"; break;
                        case TouchType::LongPress:  eventTypeName = "LongPress"; break;
                        case TouchType::Down:       eventTypeName = "Down"; break;
                        case TouchType::Move:       eventTypeName = "Move"; break;
                        case TouchType::Up:         eventTypeName = "Up"; break;
                    }
                    break;
                case EventType::Swipe:  eventTypeName = "Swipe"; break;
                case EventType::Timer:  eventTypeName = "Timer"; break;
                case EventType::Custom: eventTypeName = "Custom"; break;
            }
#endif
            dispatchEvent(event);

#ifdef CONFIG_INKUI_PROFILE
            dispatchEndUs = INKUI_PROFILE_NOW();
#endif
        }

        // 时间和电池更新
        if (statusBar_ && !statusBar_->isHidden()) {
            time_t now;
            time(&now);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            if (timeinfo.tm_min != lastMinute_) {
                lastMinute_ = timeinfo.tm_min;
                statusBar_->updateTime();
            }

            int64_t nowUs = platform_->getTimeUs();
            if (nowUs - lastBatteryUpdateUs_ >= kBatteryUpdateIntervalUs) {
                lastBatteryUpdateUs_ = nowUs;
                statusBar_->updateBattery(system_->batteryPercent());
            }
        }

        // 渲染
        if (screenRoot_) {
            renderEngine_->renderCycle(screenRoot_.get());
        }

#ifdef CONFIG_INKUI_PROFILE
        if (received && eventTypeName) {
            int64_t nowUs = INKUI_PROFILE_NOW();
            int dispatchMs = (int)((dispatchEndUs - dispatchStartUs) / 1000);
            int totalMs = (int)((nowUs - dispatchStartUs) / 1000);
            if (eventTimestampUs > 0) {
                int latencyMs = (int)((nowUs - eventTimestampUs) / 1000);
                INKUI_PROFILE_LOG("PERF", "event: type=%s touch->done=%dms dispatch=%dms render=%dms",
                    eventTypeName, latencyMs, dispatchMs, totalMs - dispatchMs);
            } else {
                INKUI_PROFILE_LOG("PERF", "event: type=%s dispatch=%dms render=%dms total=%dms",
                    eventTypeName, dispatchMs, totalMs - dispatchMs, totalMs);
            }
        }
#endif
    }
}
```

**Step 2: Build verification**

Run: `idf.py build`
Expected: Build succeeds.

**Step 3: Commit**

```bash
git add components/ink_ui/src/core/Application.cpp
git commit -m "feat(ink_ui): instrument Application event loop with end-to-end tracing"
```

---

### Task 6: Instrument Canvas clear() and drawText() (fine-grained)

**Files:**
- Modify: `components/ink_ui/src/core/Canvas.cpp` (lines 84-93 clear(), lines 433-446 drawText())

**Step 1: Add profiling to Canvas::clear()**

Add `#include "ink_ui/core/Profiler.h"` at top of Canvas.cpp.

Modify `clear()` at lines 84-93:

```cpp
void Canvas::clear(uint8_t gray) {
    if (clip_.isEmpty() || !fb_) {
        return;
    }
#ifdef CONFIG_INKUI_PROFILE
    int pixels = clip_.w * clip_.h;
    INKUI_PROFILE_BEGIN(clear);
#endif
    for (int ay = clip_.y; ay < clip_.bottom(); ay++) {
        for (int ax = clip_.x; ax < clip_.right(); ax++) {
            setPixel(ax, ay, gray);
        }
    }
#ifdef CONFIG_INKUI_PROFILE
    INKUI_PROFILE_END(clear);
    if (INKUI_PROFILE_MS(clear) > 5) {  // 仅输出耗时 >5ms 的 clear
        INKUI_PROFILE_LOG("PERF", "    canvas.clear: %dx%d=%dpx time=%dms",
            clip_.w, clip_.h, pixels, INKUI_PROFILE_MS(clear));
    }
#endif
}
```

**Step 2: Add profiling to Canvas::drawText()**

Modify `drawText()` at lines 433-446:

```cpp
void Canvas::drawText(const EpdFont* font, const char* text,
                      int x, int y, uint8_t color) {
    if (!fb_ || !font || !text || *text == '\0' || clip_.isEmpty()) return;

    int cursorX = clip_.x + x;
    int cursorY = clip_.y + y;

#ifdef CONFIG_INKUI_PROFILE
    INKUI_PROFILE_BEGIN(text);
    int charCount = 0;
#endif

    uint32_t cp;
    while ((cp = nextCodepoint(&text)) != 0) {
        if (cp == '\n') break;
        drawChar(font, cp, &cursorX, cursorY, color);
#ifdef CONFIG_INKUI_PROFILE
        charCount++;
#endif
    }

#ifdef CONFIG_INKUI_PROFILE
    INKUI_PROFILE_END(text);
    if (INKUI_PROFILE_MS(text) > 2) {  // 仅输出耗时 >2ms 的文本渲染
        INKUI_PROFILE_LOG("PERF", "    canvas.drawText: %d chars time=%dms",
            charCount, INKUI_PROFILE_MS(text));
    }
#endif
}
```

**Step 3: Build verification**

Run: `idf.py build`
Expected: Build succeeds.

**Step 4: Commit**

```bash
git add components/ink_ui/src/core/Canvas.cpp
git commit -m "feat(ink_ui): instrument Canvas clear/drawText with fine-grained profiling"
```

---

### Task 7: Enable profiling and verify end-to-end

**Files:**
- Modify: `sdkconfig.defaults` (add CONFIG_INKUI_PROFILE=y)

**Step 1: Enable profiling in sdkconfig**

Append to `sdkconfig.defaults`:

```
# InkUI performance profiling (remove for production)
CONFIG_INKUI_PROFILE=y
```

**Step 2: Full rebuild and flash**

Run: `idf.py fullclean && idf.py build`
Expected: Build succeeds with profiling enabled.

Run: `idf.py flash monitor`
Expected: Serial output shows profiling logs on touch interaction:

```
I (xxx) PERF: cycle: layout=1ms collect=0ms draw=15ms flush=1523ms total=1539ms dirty=1 (540x920)
I (xxx) PERF:   draw: clear=11ms onDraw=4ms views=8
I (xxx) PERF:   epd: area=(0,0,960,540) mode=GL16 time=1520ms
I (xxx) PERF: event: type=Tap touch->done=1540ms dispatch=0ms render=1539ms
```

**Step 3: Verify no output when idle**

Expected: No profiling logs when no touch events occur (30s timeout loop produces no dirty regions → no output).

**Step 4: Remove profiling enable from defaults**

Remove the `CONFIG_INKUI_PROFILE=y` line from `sdkconfig.defaults` (keep it disabled by default for production).

**Step 5: Commit**

```bash
git add -A
git commit -m "feat(ink_ui): verify profiling end-to-end, keep disabled by default"
```

---

## Expected Output Examples

**翻页交互 (Reader swipe):**
```
I (12345) PERF: event: type=Swipe dispatch=0ms render=1542ms total=1542ms
I (12345) PERF: cycle: layout=2ms collect=0ms draw=18ms flush=1520ms total=1540ms dirty=1 (540x920)
I (12345) PERF:   draw: clear=12ms onDraw=5ms views=12
I (12345) PERF:     canvas.clear: 540x920=496800px time=20ms
I (12345) PERF:     canvas.drawText: 42 chars time=3ms
I (12345) PERF:   epd: area=(0,0,960,540) mode=GL16 time=1518ms
```

**按钮点击 (小区域刷新):**
```
I (23456) PERF: event: type=Tap touch->done=210ms dispatch=0ms render=208ms
I (23456) PERF: cycle: layout=0ms collect=0ms draw=1ms flush=205ms total=206ms dirty=1 (120x40)
I (23456) PERF:   draw: clear=0ms onDraw=0ms views=2
I (23456) PERF:   epd: area=(480,210,40,120) mode=GL16 time=204ms
```

## How to Use

1. 开启: `idf.py menuconfig` → InkUI Configuration → Enable profiling, 或直接在 sdkconfig 中设置 `CONFIG_INKUI_PROFILE=y`
2. 构建烧录: `idf.py build flash monitor`
3. 操作设备，观察串口输出
4. 关闭: 删除 `CONFIG_INKUI_PROFILE=y`，重新构建

## File Summary

| # | Action | File |
|---|--------|------|
| 1 | Create | `components/ink_ui/include/ink_ui/core/Profiler.h` |
| 2 | Create | `components/ink_ui/Kconfig` |
| 3 | Modify | `components/ink_ui/include/ink_ui/core/Event.h` |
| 4 | Modify | `components/ink_ui/include/ink_ui/core/RenderEngine.h` |
| 5 | Modify | `components/ink_ui/src/core/GestureRecognizer.cpp` |
| 6 | Modify | `components/ink_ui/src/core/Application.cpp` |
| 7 | Modify | `components/ink_ui/src/core/RenderEngine.cpp` |
| 8 | Modify | `components/ink_ui/src/core/EpdDriver.cpp` |
| 9 | Modify | `components/ink_ui/src/core/Canvas.cpp` |
