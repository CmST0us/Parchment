## Context

当前 BootViewController 和 LibraryViewController 已有基本功能框架，但视觉效果与 `design/index.html` 设计稿有明显差距。InkUI 框架（View、FlexLayout、Canvas、各 widget 组件）已就绪，图标管线（Tabler Icons → 4bpp C 数组）已建立，字体系统（LittleFS .pfnt → PSRAM）已运行。需要重写两个 ViewController 的视图树以像素级还原设计稿。

**设计稿关键尺寸参考** (从 `design/index.html` CSS 提取):

Boot 画面:
- 全屏 540×960 居中布局
- Logo: 100×100px 书本图标
- 标题 "Parchment": 36px 衬线体 bold, letter-spacing 2px
- 副标题 "墨水屏阅读器": 16px 300 weight
- Logo 与标题间距: margin-bottom 24px
- 副标题与进度条间距: margin-bottom 48px
- 状态文字: 13px muted, margin-bottom 16px
- 进度条: 200px × 3px, 圆角 2px

Library 页面:
- Header: 48px 黑底白字, 左侧汉堡菜单图标 + 标题 "Parchment" + 右侧设置图标
- Subheader: 36px 浅灰底, 左侧书本图标 + "共 N 本" + 右侧 "按名称排序 ▾"
- 书籍条目: 96px 高
  - 左侧封面: 52×72px, 深色边框 1.5px, 圆角 3px, 内含格式标签 "TXT", 右上角折角
  - 封面与信息间距: margin-right 16px
  - 书名: 17px 衬线体 bold
  - 作者: 13px secondary color
  - 进度条: 160px × 6px + 百分比文字
  - 条目 padding: 0 16px
- 底部翻页: 48px, "← 上一页 · 1/1 · 下一页 →"

## Goals / Non-Goals

**Goals:**
- 像素级还原 `design/index.html` 的 Boot 画面和 Library 页面视觉效果
- 利用现有 InkUI widget 组件，最小化新增代码
- 在 4bpp 灰度 E-Ink 屏上忠实呈现设计意图

**Non-Goals:**
- 不实现阅读页面、工具栏、设置页面等其他页面
- 不修改 InkUI 框架核心（View, FlexLayout, RenderEngine 等）
- 不修改字体系统或图标转换管线
- 不实现排序按钮的实际排序功能（仅视觉展示）
- 不实现汉堡菜单按钮的实际功能

## Decisions

### 1. Boot Logo: 使用 Canvas 自绘而非 Bitmap 图标

**决策**: 在 Boot 画面中创建一个自定义 `BootLogoView` 继承 `ink::View`，在 `onDraw()` 中使用 Canvas API 绘制简化版书本图标。

**理由**:
- 设计稿中的 Logo 是 SVG 线条画（书本轮廓 + 书页线条 + 装饰羽毛笔），包含曲线和细线
- Canvas 只有 `drawLine`, `drawRect`, `fillRect`, `drawHLine`, `drawVLine` 等基础图元
- 用直线段近似绘制简化的书本图标即可，E-Ink 低分辨率下差异不明显
- 备选方案: 将 Logo SVG 转为 100×100 的 4bpp bitmap 通过 `drawBitmap` 渲染，但这需要 5000 bytes 的 const 数据，且 100px 超出了图标管线的 32px 标准尺寸

**替代方案**: 修改 `convert_icons.py` 支持自定义尺寸然后生成 100×100 bitmap。但这增加了管线复杂度且只用一次，不值得。

### 2. 书籍封面: 创建 BookCoverView 自定义 View

**决策**: 新增 `BookCoverView` 继承 `ink::View`，固定 52×72px，在 `onDraw()` 中绘制边框、背景、格式标签文字和右上角折角效果。

**理由**:
- 封面是复合视觉元素（矩形边框 + 文字 + 装饰三角形），不适合用现有 widget 拼装
- 自绘 View 最灵活、最能精确控制像素
- 复用现有 Canvas API: `drawRect` 画边框, `fillRect` 画背景, `drawText` 画格式标签

### 3. 作者信息: book_info_t 暂不含 author 字段

**决策**: `book_info_t` 目前只有 `name`, `path`, `size_bytes`。Library 列表中作者行改为显示文件大小信息（如 "245.3 KB"），保持与当前数据层一致。

**理由**:
- 设计稿中的作者信息来自 prototype 的硬编码示例数据
- 实际系统只支持 TXT 文件，无元数据提取
- 如果未来支持 EPUB 等格式，再扩展 `book_info_t` 添加 author 字段

### 4. 字体映射

**决策**:
- 标题 "Parchment" (设计稿 36px serif bold): 使用 `ui_font_get(28)` (现有最大 UI 字体)
- 副标题 "墨水屏阅读器" (设计稿 16px light): 使用 `ui_font_get(20)`, Color::Dark
- 状态文字 (设计稿 13px): 使用 `ui_font_get(16)` (16px UI 字体), Color::Medium
- 书名 (设计稿 17px serif bold): 使用 `ui_font_get(20)`, Color::Black
- 作者/文件大小 (设计稿 13px): 使用 `ui_font_get(16)`, Color::Dark
- 进度百分比 (设计稿 12px): 使用 `ui_font_get(16)`, Color::Medium
- Subheader 文字 (设计稿 13px): 使用 `ui_font_get(16)`, Color::Dark
- 翻页指示器: 使用 `ui_font_get(16)`

**理由**: 设备只有 16px 和 24px UI 字体可用（pfnt 文件为 `ui_font_16.pfnt` 和 `ui_font_24.pfnt`），以及 28px (api 上是 24 映射)。选择最接近设计稿的可用字号。E-Ink 低 DPI 下微小字号差异不明显。

### 5. 进度条尺寸

**决策**: Boot 进度条使用 ProgressBarView，宽度 200px，track 高度 3px。Library 书籍进度条宽度由 flex 决定（约 160px），track 高度 6px。

### 6. 新增图标需求

**决策**: 需要额外下载 `book-2.svg`（书本图标用于 subheader）到 `tools/icons/src/`，重新运行 `convert_icons.py` 生成更新的 `ui_icon_data.h`，并在 `ui_icon.h/c` 中注册新图标常量。

现有可用图标: arrow-left, bookmark, chevron-right, list, menu-2, settings, sort-descending, typography。
新增需要: book-2 (Tabler Icons)。

## Risks / Trade-offs

- **[风险] 字号不完全匹配设计稿** → 设备可用字号有限（16/24px），与设计稿（13/16/17/36px）有差异。在 E-Ink 低 DPI 下可接受，视觉效果通过间距和布局补偿。
- **[风险] 封面折角在 4bpp 灰度下可能不清晰** → 使用 `fillRect` 绘制简化的三角形近似（用两个三角形矩形块），E-Ink 上效果足够。
- **[风险] Boot Logo 简化后与设计稿差异** → 用直线段画简化书本轮廓，省略羽毛笔装饰。在墨水屏上辨识度足够。
- **[Trade-off] 作者信息用文件大小替代** → 保持数据层简洁，TXT 文件无作者元数据。未来可扩展。
