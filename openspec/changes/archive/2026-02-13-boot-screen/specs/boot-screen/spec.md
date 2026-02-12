## ADDED Requirements

### Requirement: 启动画面页面结构
系统 SHALL 提供 `page_boot` 页面（`ui_page_t` 类型），位于 `main/pages/page_boot.c`，并在 `page_boot.h` 中导出。

#### Scenario: 页面声明可用
- **WHEN** `#include "page_boot.h"` 并引用 `page_boot`
- **THEN** 编译 SHALL 成功，`page_boot.name` SHALL 为 `"boot"`

### Requirement: 启动画面布局
系统 SHALL 在启动画面上渲染以下居中元素（540×960 逻辑坐标）：
- Logo 占位区域：屏幕垂直居中偏上，120×120px 区域
- 标题文字 "Parchment"：Logo 下方，36px 字号，BLACK 颜色，水平居中
- 副标题 "E-Ink Reader"：标题下方，18px 字号，DARK 颜色，水平居中
- 状态文字 "Initializing..."：副标题下方留间距，14px 字号，MEDIUM 颜色，水平居中
- 页面背景 SHALL 为 WHITE

#### Scenario: 首次渲染
- **WHEN** `page_boot` 被 push 到页面栈
- **THEN** 屏幕 SHALL 显示白色背景上的 Logo 区域、"Parchment" 标题、"E-Ink Reader" 副标题和 "Initializing..." 状态文字，所有元素水平居中

#### Scenario: Logo 占位
- **WHEN** 无 Logo 位图资源可用
- **THEN** Logo 区域 SHALL 使用文字占位符或矩形边框表示

### Requirement: 定时自动跳转
系统 SHALL 在启动画面进入后 2 秒自动跳转到下一页面。

跳转 SHALL 通过以下机制实现：
1. `on_enter` 回调中创建一个 2000ms 的 one-shot FreeRTOS timer
2. Timer 到期后通过 `ui_event_send()` 发送 `UI_EVENT_TIMER` 事件
3. `on_event` 回调接收到 `UI_EVENT_TIMER` 后调用 `ui_page_replace()` 导航到下一页面

#### Scenario: 2 秒后跳转
- **WHEN** `page_boot` 被 push 且 2 秒已过
- **THEN** 系统 SHALL 通过 `ui_page_replace()` 导航离开启动画面

#### Scenario: timer 在页面退出时清理
- **WHEN** `page_boot` 在 timer 到期前被手动 pop 或 replace
- **THEN** `on_exit` SHALL 删除未到期的 timer，避免悬挂回调

### Requirement: 清理临时测试代码
`main.c` SHALL 移除全部临时验证代码：
- `s_lyrics` 字符串常量
- `s_demo_mode`、`s_page_offset`、`s_next_offset` 静态变量
- `text_demo_on_enter`、`text_demo_on_event`、`render_font_showcase`、`render_lyrics_page`、`text_demo_on_render` 函数
- `text_demo_page` 页面结构体
- 不再需要的 `#include "ui_font.h"` 和 `#include "ui_text.h"` 头文件

`app_main()` SHALL 使用 `ui_page_push(&page_boot, NULL)` 启动生产页面。

#### Scenario: main.c 不含临时代码
- **WHEN** 编译项目
- **THEN** `main.c` SHALL 不包含 `text_demo_page`、`s_lyrics` 等临时代码的任何引用

#### Scenario: 启动入口使用 page_boot
- **WHEN** `app_main()` 执行到 UI 初始化部分
- **THEN** SHALL 调用 `ui_page_push(&page_boot, NULL)` 作为首个页面
