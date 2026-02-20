## Context

当前 `ReaderViewController` 直接使用 `ink::TextLabel` 显示阅读文本。TextLabel 是通用组件，不支持可配置行距（固定 `font->advance_y`）、段间距、自动折行。为绕过折行限制，ViewController 内部在 `paginate()` 和 `renderPage()` 中各维护一套 UTF-8 字符遍历 + glyph 宽度测量 + 折行逻辑，造成约 150 行代码重复。行距不一致导致分页计算与实际渲染不匹配。

## Goals / Non-Goals

**Goals:**
- 创建 `ReaderContentView`，统一文本折行/分页/渲染逻辑到一处
- 正确渲染可配置行距（`line_spacing`）和段间距（`paragraph_spacing`）
- 按像素高度填充页面，段间距影响每页可容纳行数
- 懒分页：首次 `onDraw` 时自动执行，此时 View frame 已由 FlexLayout 确定
- 重构 ReaderViewController，删除重复的分页/渲染代码

**Non-Goals:**
- 不实现阅读工具栏（Change 7: reader-toolbar）
- 不实现目录跳转（Change 8: table-of-contents）
- 不实现阅读设置界面（Change 9: reading-settings）
- 不改动 TextLabel 通用组件
- 不处理 EPUB 格式

## Decisions

### Decision 1: 新建 ReaderContentView 而非增强 TextLabel

**选择**: 在 `main/views/` 新建 `ReaderContentView` 继承 `ink::View`

**替代方案**: 给 TextLabel 加 `setLineSpacing()`、`setAutoWrap()` 等

**理由**: 阅读场景需要折行 + 分页 + 段间距 + 顶部对齐，与 TextLabel 的通用单行/多行标签定位差异太大。硬塞进去会使 TextLabel 臃肿，且分页逻辑不属于 View 组件。ReaderContentView 专注阅读场景，保持 TextLabel 简洁。

### Decision 2: 统一 `layoutPage()` 布局引擎

**选择**: 一个 `layoutPage(startOffset)` 方法，返回当前页的行列表和下一页偏移。`paginate()` 循环调用它构建页表，`onDraw()` 调用它获取当前页布局后绘制。

**理由**: 消除 paginate/renderPage 的代码重复。单一来源保证分页计算和渲染使用完全相同的折行逻辑。

### Decision 3: 按像素高度填充而非固定行数

**选择**: `layoutPage()` 用 `remainingHeight` 像素计数器，每行消耗 `lineHeight` 像素，段落结束额外消耗 `paragraphSpacing` 像素。

**替代方案**: `linesPerPage = height / lineHeight` 固定行数

**理由**: 段间距使每页行数不固定。短段落密集的页面（多次 `\n`）比连续长段落的页面能容纳更少文本行。按像素填充能精确利用页面空间。

### Decision 4: 懒分页

**选择**: `paginate()` 在首次 `onDraw()` 时自动触发（当 `pages_` 为空且有文本数据时）。

**理由**: 分页需要 View 的 `bounds()` 尺寸，FlexLayout 在渲染循环中才设置 frame。懒分页避免在 `viewDidLoad` 中手动计算内容区域尺寸。

### Decision 5: 文件路径

**选择**: `main/views/ReaderContentView.h` + `main/views/ReaderContentView.cpp`

**理由**: 该组件强耦合 `EpdFont`/`epd_get_glyph`（epdiy 库）和阅读场景，不属于 `ink_ui` 通用框架。放在应用层 `main/views/` 与 `BookCoverView`、`BootLogoView` 同级。

### Decision 6: 直接 Canvas 绘制，不使用子 View

**选择**: `onDraw()` 中直接调用 `canvas.drawTextN()` 逐行绘制文本。

**替代方案**: 创建多个 TextLabel 子 View，每行一个

**理由**: 阅读页面的文本行是动态的（每翻一页行内容全变），创建/销毁子 View 开销大且无意义。直接绘制最简洁高效。

## Risks / Trade-offs

**[大文件首次分页耗时]** → 与现有方案一致，8MB 文件的全量分页在 ESP32-S3 上可能需要数百毫秒。首次 `onDraw` 会阻塞渲染。可接受，因为只发生一次。未来可优化为流式分页（先算前 N 页，后台继续）。

**[压缩字体逐字 malloc/free]** → `Canvas::drawTextN` 对压缩字体每个字符触发 miniz 解压 + malloc/free。大段文字绘制性能可能不理想。这是 Canvas 层面的问题，本次不改，后续可通过 glyph cache 优化。

**[ReaderContentView 与 ReaderTouchView 嵌套]** → ReaderContentView 作为 ReaderTouchView 的子 View，触摸事件由 ReaderTouchView 处理，渲染由 ReaderContentView 负责。职责清晰，但需注意 parent 重绘时子 View 会被 force-redrawn。
