## Why

项目已完成基础设施层（图标、widget、数据层、文字渲染），但 `main.c` 仍然使用临时测试页面（text_demo_page + 歌词文本）。需要实现第一个生产页面——启动画面，替换掉临时代码，为后续页面实现（Library、Reader 等）建立入口。

## What Changes

- 新建 `main/pages/page_boot.c/h`：启动画面页面，居中显示 Logo 区域、"Parchment" 标题、"E-Ink Reader" 副标题和 "Initializing..." 状态文字
- 启动画面显示 2 秒后自动跳转（当前先跳回自身或保持，Library 页面待后续 change 实现）
- 清理 `main.c` 中全部临时代码：移除 `text_demo_page`、`s_lyrics`、所有 `render_*` / `text_demo_*` 函数和相关 static 变量
- `app_main()` 改为 `ui_page_push(&page_boot, NULL)` 启动生产页面

## Capabilities

### New Capabilities
- `boot-screen`: 启动画面页面，负责品牌展示和初始化状态反馈，2 秒后自动导航到下一页面

### Modified Capabilities
- `ui-page-stack`: 需要支持定时器事件（`UI_EVENT_TIMER`）驱动自动跳转；当前 spec 中定时器事件已定义但未在页面实践中使用过

## Impact

- **`main/main.c`**: 移除 ~150 行临时测试代码，替换为 `page_boot` push 调用
- **新文件**: `main/pages/page_boot.c`, `main/pages/page_boot.h`
- **依赖**: ui_core（页面栈 + 事件 + 渲染）、ui_canvas（绘图）、ui_widget（header 可选）、ui_icon（可选 Logo 图标）
- **无 API 变更**: 不修改任何现有组件接口
