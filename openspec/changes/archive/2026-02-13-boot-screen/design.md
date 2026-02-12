## Context

项目的基础设施层已完成（icon_resources、ui_widgets、data-layer、text-rendering），但 `main.c` 仍然包含 ~150 行临时验证代码（text_demo_page + 歌词文本 + 渲染函数）。当前需要实现第一个生产页面——启动画面（Boot Screen），并清理掉所有临时代码。

启动画面的线框规格定义在 `docs/ui-wireframe-spec.md` 第 1 节：居中显示 Logo 区域、"Parchment" 标题、"E-Ink Reader" 副标题、"Initializing..." 状态文字，2-3 秒后跳转到 Library 页面。

## Goals / Non-Goals

**Goals:**
- 实现 `page_boot.c/h`，严格按照线框规格渲染启动画面
- 使用 FreeRTOS timer 在 2 秒后发送 `UI_EVENT_TIMER` 触发自动跳转
- 彻底移除 `main.c` 中全部临时测试代码
- 建立 `main/pages/` 目录结构，为后续页面实现提供范式

**Non-Goals:**
- 不实现 Library 页面（后续 change）；跳转目标暂时先用 `ui_page_replace` 回到自身或留空
- 不实现错误处理显示（如初始化失败时替换 "Initializing..."）——此功能延后
- 不实现 Logo 图形（当前无 Logo 位图资源），Logo 区域使用文字或空占位

## Decisions

### 1. 页面文件位置：`main/pages/`

**选择**: 页面源文件放在 `main/pages/page_boot.c` 和 `main/pages/page_boot.h`。

**理由**: 页面是应用层逻辑，不是可复用组件，放在 `main/pages/` 比放在 `components/` 更合适。roadmap 中已确定此结构。

### 2. 定时跳转机制：FreeRTOS `xTimerCreate` + `UI_EVENT_TIMER`

**选择**: 在 `on_enter` 中创建一个 2 秒的 one-shot FreeRTOS timer，timer callback 通过 `ui_event_send()` 发送 `UI_EVENT_TIMER`，`on_event` 收到后执行 `ui_page_replace()`。

**备选方案**:
- `vTaskDelay` 在单独任务中：增加不必要的任务开销
- `on_render` 中计数帧数：渲染频率不固定，不可靠

**理由**: FreeRTOS timer 精确、轻量、不阻塞 UI 循环，且使用了已定义的 `UI_EVENT_TIMER` 事件类型。

### 3. 跳转目标：暂时 replace 为自身（或 placeholder 页面）

**选择**: Library 页面尚未实现，boot-screen 的 timer 到期后执行 `ui_page_replace(&page_boot, NULL)` 重新进入自身（效果是刷新画面）。后续 library-screen change 实现时再改为跳转到 Library。

**理由**: 避免引入未实现的依赖；保持本 change 独立可验证。

### 4. Logo 区域：使用文字 placeholder

**选择**: 线框中 120×120 Logo 区域使用大字号 "P" 字母或空心矩形占位。

**理由**: 当前没有 Logo 位图资源。后续可替换为真正的 Logo 图片。

### 5. 清理范围

移除 `main.c` 中以下内容:
- `s_lyrics` 字符串常量
- `s_demo_mode`、`s_page_offset`、`s_next_offset` 静态变量
- `text_demo_on_enter()`、`text_demo_on_event()`、`render_font_showcase()`、`render_lyrics_page()`、`text_demo_on_render()` 函数
- `text_demo_page` 页面结构体
- 不再需要的 `#include "ui_font.h"` 和 `#include "ui_text.h"`

`app_main()` 改为: `#include "page_boot.h"` + `ui_page_push(&page_boot, NULL)`。

## Risks / Trade-offs

- **[临时跳转到自身]** → 用户体验上 boot 后停留在启动画面。这是预期行为，Library 页面实现后立即修复。
- **[无 Logo 图形]** → 视觉上不如设计稿完整。可在后续 change 中添加 Logo 资源。
- **[2 秒硬编码]** → 跳转延时目前硬编码为 2000ms。如需可配置，可后续通过 `settings_store` 支持。
