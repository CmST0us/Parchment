## Why

当前 `ReaderViewController` 的文本分页和渲染逻辑存在两个核心问题：
1. **行距不生效** — `paginate()` 按配置的 1.6x 行距计算每页行数，但 `TextLabel` 固定以 `font->advance_y`（1.0x）渲染，导致文字挤在页面上半部分
2. **折行逻辑重复** — `paginate()` 和 `renderPage()` 各维护一套约 60 行的 UTF-8 遍历 + 宽度测量 + 折行代码，任何修改需同步两处

根本原因是 `ink::TextLabel` 作为通用组件不支持可配置行距、段间距和自动折行，不适合阅读场景。需要一个专用的阅读内容 View。

## What Changes

- **新增 `ReaderContentView`** (`main/views/`) — 专用阅读文本渲染 View，内置统一的折行/分页/绘制引擎
  - 统一 `layoutPage()` 方法同时服务分页和渲染，消除代码重复
  - 支持可配置行距（`line_spacing`）、段间距（`paragraph_spacing`）
  - 按像素高度填充页面（支持段间距导致的变长页面）
  - 懒分页：首次 `onDraw` 时自动执行分页
  - 直接使用 `Canvas::drawTextN` 逐行绘制，绕过 TextLabel
- **重构 `ReaderViewController`** — 删除内部的 `paginate()`/`renderPage()` 及重复折行代码，改用 `ReaderContentView` 的分页和翻页 API

## Capabilities

### New Capabilities
- `reader-content-view`: 专用阅读文本渲染 View，提供文本折行、分页计算、逐行绘制能力，支持可配置行距和段间距

### Modified Capabilities
- `reader-viewcontroller`: 文本分页和渲染逻辑从 ViewController 内部迁移到 ReaderContentView，ViewController 改为调用 ReaderContentView 的分页/翻页 API

## Impact

- **新增文件**: `main/views/ReaderContentView.h`, `main/views/ReaderContentView.cpp`
- **修改文件**: `main/controllers/ReaderViewController.h`, `main/controllers/ReaderViewController.cpp`
- **删除代码**: ReaderViewController 中约 150 行的 paginate/renderPage/重复折行逻辑
- **依赖**: `ink::View`, `ink::Canvas`, `EpdFont`/`epd_get_glyph` (epdiy)
- **无 API 变更**: 对外接口（ViewController 生命周期、触摸交互）不变
