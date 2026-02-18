## Why

PageIndicatorView 当前使用中文文字「上一页」「下一页」作为翻页按钮，在 48×48 的固定尺寸内文字显示拥挤且不够直观。改为箭头图标可以提升视觉一致性（与 HeaderView 的图标按钮风格统一），同时消除对字体渲染的依赖，降低按钮区域的绘制开销。

## What Changes

- 新增 `arrow-right` 图标资源（32×32 4bpp），通过现有 `convert_icons.py` 工具从 Tabler Icons 生成
- 在 `ui_icon_data.h` 中添加 `icon_arrow_right_data[]` 位图数据
- 在 `ui_icon.h` / `ui_icon.c` 中注册 `UI_ICON_ARROW_RIGHT` 常量
- 修改 `PageIndicatorView` 的上一页/下一页按钮：从 `ButtonStyle::Secondary` + 文字标签改为 `ButtonStyle::Icon` + 箭头图标（← →）
- 移除 `PageIndicatorView::setFont()` 中对 prev/next 按钮的字体设置（Icon 模式不需要字体）

## Capabilities

### New Capabilities

无新增 capability。

### Modified Capabilities

- `icon-pipeline`: 新增 `arrow-right` 图标到图标资源集
- `ink-page-indicator`: 按钮样式从文字改为图标

## Impact

- `components/ui_core/ui_icon_data.h` — 新增图标位图数据
- `components/ui_core/include/ui_icon.h` — 新增常量声明
- `components/ui_core/ui_icon.c` — 新增常量定义
- `components/ink_ui/src/views/PageIndicatorView.cpp` — 按钮创建逻辑变更
- `components/ink_ui/include/ink_ui/views/PageIndicatorView.h` — 可能移除或简化 `setFont()` 签名
- `tools/icons/convert_icons.py` 的输入配置可能需要添加 `arrow-right`
