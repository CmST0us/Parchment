## 1. 页面文件创建

- [x] 1.1 创建 `main/pages/` 目录，新建 `page_boot.h` 头文件，声明 `extern ui_page_t page_boot`
- [x] 1.2 创建 `page_boot.c`，实现 `page_boot` 结构体（name="boot"）和四个回调函数骨架（on_enter, on_exit, on_event, on_render）

## 2. 启动画面渲染

- [x] 2.1 实现 `on_render`：白色背景填充 + Logo 占位矩形（居中 120×120，DARK 边框）
- [x] 2.2 实现 `on_render`：渲染 "Parchment"（36px, BLACK, 水平居中）、"E-Ink Reader"（18px, DARK, 居中）、"Initializing..."（14px, MEDIUM, 居中）

## 3. 定时跳转

- [x] 3.1 在 `on_enter` 中创建 2000ms one-shot FreeRTOS timer，timer callback 通过 `ui_event_send()` 发送 `UI_EVENT_TIMER`
- [x] 3.2 在 `on_event` 中处理 `UI_EVENT_TIMER`，调用 `ui_page_replace(&page_boot, NULL)`（Library 页面实现前暂跳回自身）
- [x] 3.3 在 `on_exit` 中删除未到期的 timer（若存在），避免悬挂回调

## 4. 清理 main.c

- [x] 4.1 移除 `main.c` 中全部临时代码：`s_lyrics`、`s_demo_mode`、`s_page_offset`、`s_next_offset`、`text_demo_on_enter`、`text_demo_on_event`、`render_font_showcase`、`render_lyrics_page`、`text_demo_on_render`、`text_demo_page`
- [x] 4.2 移除不再需要的 `#include "ui_font.h"` 和 `#include "ui_text.h"`
- [x] 4.3 添加 `#include "page_boot.h"`，将 `ui_page_push(&text_demo_page, NULL)` 改为 `ui_page_push(&page_boot, NULL)`

## 5. 构建验证

- [x] 5.1 更新 `main/CMakeLists.txt`，将 `pages/page_boot.c` 加入 SRCS
- [x] 5.2 执行 `idf.py build` 确认编译通过，无错误无警告
