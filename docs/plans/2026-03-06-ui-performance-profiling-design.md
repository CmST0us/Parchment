# InkUI 性能 Profiling 设计

## 目标

为 InkUI 框架添加全链路性能 profiling，量化从触摸事件到屏幕刷新完成的每个阶段耗时，定位性能瓶颈。

## 方案

采用 **管线阶段计时器（方案 A）+ 事件追踪日志（方案 B）** 组合。

- 方案 A：在渲染管线各阶段插入 `esp_timer_get_time()` 计时点，输出每阶段耗时
- 方案 B：Event 结构携带时间戳，追踪单个事件的完整生命周期

通过 `CONFIG_INKUI_PROFILE` 条件编译开关控制，默认关闭，零运行时开销。

## 基础设施

### Profiler.h 宏

```cpp
// components/ink_ui/include/ink_ui/core/Profiler.h
#pragma once
#include "sdkconfig.h"

#ifdef CONFIG_INKUI_PROFILE
  #include "esp_timer.h"
  #include "esp_log.h"

  #define INKUI_PROFILE_BEGIN(name)  int64_t _prof_##name = esp_timer_get_time()
  #define INKUI_PROFILE_END(name)    int64_t _prof_##name##_end = esp_timer_get_time()
  #define INKUI_PROFILE_US(name)     (int)(_prof_##name##_end - _prof_##name)
  #define INKUI_PROFILE_MS(name)     (int)((_prof_##name##_end - _prof_##name) / 1000)
  #define INKUI_PROFILE_LOG(tag, fmt, ...) ESP_LOGI(tag, fmt, ##__VA_ARGS__)
#else
  #define INKUI_PROFILE_BEGIN(name)
  #define INKUI_PROFILE_END(name)
  #define INKUI_PROFILE_US(name)  0
  #define INKUI_PROFILE_MS(name)  0
  #define INKUI_PROFILE_LOG(tag, fmt, ...)
#endif
```

### Kconfig 开关

```
# components/ink_ui/Kconfig
menu "InkUI Configuration"
    config INKUI_PROFILE
        bool "Enable InkUI performance profiling"
        default n
        help
            Enable profiling instrumentation in InkUI rendering pipeline.
            Outputs per-stage timing to serial log.
endmenu
```

## 插桩点

### 1. RenderEngine::renderCycle() — 各阶段耗时

```
[PERF] cycle: layout=1ms collect=0ms draw=15ms flush=1523ms total=1539ms dirty=1 (540x920)
```

仅在有脏区域时输出（避免空循环刷屏）。

### 2. RenderEngine::drawView() — 绘制细节

统计 canvas.clear() 和 onDraw() 的累计耗时、绘制 View 数量。

```
[PERF]   draw: clear=11ms onDraw=4ms views=8
```

### 3. EpdDriver::updateArea() — EPD 硬件刷新

```
[PERF]   epd: area=(0,0,960,540) mode=GL16 time=1520ms
```

### 4. Application::run() — 事件循环层

```
[PERF] event: type=Tap dispatch=0ms → render=1539ms total=1539ms
```

### 5. Event 时间戳追踪（方案 B）

Event 结构条件编译增加 `timestampUs` 字段：

```cpp
struct Event {
    EventType type;
    union { ... };
#ifdef CONFIG_INKUI_PROFILE
    int64_t timestampUs = 0;
#endif
};
```

GestureRecognizer::sendEvent() 中打上时间戳。Application 中计算完整延迟。

```
[TRACE] Tap@(270,480): touch→dispatch=0.4ms render=1539ms total=1539.4ms
```

### 6. Canvas::clear() / Canvas::drawText() — 可选细粒度

仅在需要深入分析时启用，可作为后续增强。

## 修改文件清单

| 文件 | 修改内容 |
|------|----------|
| **新建** `components/ink_ui/include/ink_ui/core/Profiler.h` | Profiler 宏 |
| **新建** `components/ink_ui/Kconfig` | CONFIG_INKUI_PROFILE 开关 |
| `components/ink_ui/include/ink_ui/core/Event.h` | 条件编译添加 timestampUs |
| `components/ink_ui/src/core/GestureRecognizer.cpp` | sendEvent() 打时间戳 |
| `components/ink_ui/src/core/Application.cpp` | 事件循环 + 分发计时 |
| `components/ink_ui/src/core/RenderEngine.cpp` | renderCycle 各阶段计时 |
| `components/ink_ui/src/core/EpdDriver.cpp` | updateArea 刷新计时 |
| `components/ink_ui/src/core/Canvas.cpp` | clear/drawText 细粒度计时 |

## 设计约束

- 条件编译默认关闭，生产环境零开销
- 不修改 View/ViewController 业务代码，仅在引擎层插桩
- Event 大小增加 8 字节（仅 profiling 模式），FreeRTOS 队列自动适配
- 日志使用 ESP_LOGI，通过 ESP-IDF 日志级别可进一步控制
