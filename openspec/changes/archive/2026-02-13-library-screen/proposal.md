## Why

启动画面完成后需要跳转到第一个可交互页面。书库界面（Library Screen）是用户的主入口页面，负责展示 SD 卡中的书籍列表并允许用户选择书籍进入阅读。这是 Phase 2 页面实现的核心里程碑，连接了已完成的所有基础设施（图标、数据层、字体、Widget 组件）。

## What Changes

- 新增 `main/pages/page_library.c/h`，实现完整的书库列表页面
- 页面布局遵循 `docs/ui-wireframe-spec.md` 第 2 节线框规格：
  - Header（48px）：左侧菜单图标、标题 "Parchment"、右侧设置图标
  - Subheader（36px）：书籍数量统计、排序方式切换按钮
  - 书目列表：每项 96px 高，含格式标签、书名、作者、阅读进度条
  - 支持上下滑动滚动（局部刷新）
- 集成 `book_store` 组件扫描 SD 卡 TXT 文件
- 集成 `settings_store` 读取/保存排序偏好和阅读进度
- 修改 `page_boot` 的跳转目标，从无操作改为跳转到 Library 页面
- 排序功能支持按名称和按大小排序（通过 modal dialog 切换）

## Capabilities

### New Capabilities
- `library-page`: 书库列表页面的完整 UI 和交互逻辑（布局、渲染、触摸事件、滚动、排序、页面导航）

### Modified Capabilities
- `boot-screen`: 修改跳转目标，启动完成后 `ui_page_replace` 到 Library 页面

## Impact

- **新文件**: `main/pages/page_library.c`, `main/pages/page_library.h`
- **修改文件**: `main/pages/page_boot.c`（添加跳转逻辑）, `main/main.c`（如需注册页面）
- **依赖组件**: `book_store`, `settings_store`, `ui_core`（ui_widget, ui_canvas, ui_page, ui_render, ui_event, ui_font）
- **运行时依赖**: SD 卡已挂载（由 `main.c` 保证）、字体已加载（由 `ui_font_init` 保证）
