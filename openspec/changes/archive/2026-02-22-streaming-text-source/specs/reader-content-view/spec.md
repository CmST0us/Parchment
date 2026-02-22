## MODIFIED Requirements

### Requirement: ReaderContentView 文本配置
ReaderContentView SHALL 提供以下配置方法：
- `setTextSource(TextSource* source)` — 设置文本数据源（非拥有指针，调用方负责生命周期）。替代原 `setTextBuffer()`。
- `setFont(const EpdFont* font)` — 设置渲染字体
- `setLineSpacing(uint8_t spacing10x)` — 设置行距倍数（×10），如 16 表示 1.6 倍。范围 10~25。
- `setParagraphSpacing(uint8_t px)` — 设置段间距像素数。范围 0~24。
- `setTextColor(uint8_t color)` — 设置文字颜色

任何配置变更 SHALL 使已有 PageIndex 失效（清空），下次 `onDraw` 时重新触发后台构建。

#### Scenario: 设置文本源和字体
- **WHEN** 调用 `setTextSource(source)` 和 `setFont(font)`
- **THEN** View 持有 TextSource 指针和字体引用，准备好进行分页和渲染

#### Scenario: 修改行距触发重新分页
- **WHEN** 已完成分页后调用 `setLineSpacing(20)`（从 1.6x 改为 2.0x）
- **THEN** PageIndex 被清空，后台分页 task 重新启动

### Requirement: ReaderContentView 懒分页
ReaderContentView SHALL 在首次 `onDraw()` 调用时：
1. 尝试从 SD 卡加载缓存的 PageIndex（`pages.idx`）
2. 若缓存命中（file_size + paramsHash 校验通过）：直接使用，立即可显示总页数
3. 若缓存未命中：启动后台 FreeRTOS task 增量构建 PageIndex

后台分页 task SHALL：
- 优先级 `tskIDLE_PRIORITY + 2`，栈空间 8KB
- 循环调用 `layoutPage()` 获取每页边界，通过 `PageIndex::addPage()` 添加
- 从 `TextSource::read()` 获取文本数据
- 完成后调用 `PageIndex::markComplete()` 和 `PageIndex::save()`
- task 自动退出

分页完成后 SHALL 恢复 `currentPage_`：若之前设置过 `initialByteOffset_` 且在页表范围内，使用 `PageIndex::findPage()` 定位；否则保持为 0。

#### Scenario: 缓存命中秒开
- **WHEN** 设置了 TextSource，首次 `onDraw` 被调用，SD 卡存在有效 `pages.idx`
- **THEN** PageIndex 从缓存加载，总页数立刻可用，不启动后台 task

#### Scenario: 缓存未命中启动后台分页
- **WHEN** 设置了 TextSource，首次 `onDraw` 被调用，无有效 `pages.idx`
- **THEN** 后台 FreeRTOS task 启动，当前页正常显示，PageIndex 逐步构建

#### Scenario: 分页后再次绘制不重复
- **WHEN** 已完成分页后，`onDraw` 再次被调用
- **THEN** 不重新执行分页，直接绘制当前页

### Requirement: ReaderContentView 统一布局引擎 layoutPage
ReaderContentView SHALL 提供内部方法 `layoutPage(uint32_t startOffset)` 作为折行和页面填充的单一来源。该方法 SHALL：

1. 通过 `textSource_->read(startOffset)` 获取 UTF-8 文本（替代原来的直接指针访问）
2. 使用 `bounds().w` 作为可用行宽
3. 使用 `bounds().h` 作为可用页面高度
4. 计算行高 = `font->advance_y * lineSpacing10x / 10`，最小为 `font->advance_y`
5. 从 `startOffset` 开始，按像素高度填充页面：
   - 遇到 `\n`（或 `\r\n`）：记录为空行（段落结束），消耗 `lineHeight + paragraphSpacing` 像素
   - 否则：逐字符测量宽度（`epd_get_glyph` 获取 `advance_x`），超出行宽时折行，消耗 `lineHeight` 像素；若该行末尾紧跟 `\n`，额外消耗 `paragraphSpacing` 像素
   - 剩余高度不足一个 `lineHeight` 时停止
6. 返回该页所有行的信息（起始偏移、结束偏移、是否段落结尾）和下一页起始偏移

`layoutPage()` 在 `read()` 返回 `{nullptr, 0}` 时 SHALL 返回空 PageLayout（无行，endOffset = startOffset）。

后台 task 和 `onDraw()` SHALL 都使用 `layoutPage()` ，保证分页计算和渲染使用完全相同的折行逻辑。

#### Scenario: 纯文本无换行符
- **WHEN** 文本为 600 字节连续中文（无 `\n`），行宽可容纳 20 个字符
- **THEN** layoutPage 自动折行，每行约 20 字符，直到页面高度用完

#### Scenario: 多段落文本
- **WHEN** 文本包含多个 `\n` 分隔的短段落，paragraphSpacing = 8px
- **THEN** 每个段落后额外消耗 8px，同一页可容纳的文本行数少于纯连续文本

#### Scenario: TextSource 数据不可用
- **WHEN** 调用 `layoutPage(offset)` 但 `textSource_->read(offset)` 返回 `{nullptr, 0}`
- **THEN** 返回空 PageLayout，endOffset = startOffset

### Requirement: ReaderContentView 分页 API
ReaderContentView SHALL 提供以下分页查询和控制 API：
- `int totalPages() const` — PageIndex 完成时返回总页数；未完成时返回 -1
- `int currentPage() const` — 返回当前页码（0-indexed）
- `void setCurrentPage(int page)` — 设置当前页并调用 `setNeedsDisplay()`
- `uint32_t currentPageOffset() const` — 返回当前页的起始字节偏移
- `int pageForByteOffset(uint32_t offset) const` — PageIndex 有数据时二分搜索页码，否则返回 -1
- `bool isPageIndexComplete() const` — 页索引是否构建完成
- `float pageIndexProgress() const` — 页索引构建进度 [0.0, 1.0]

#### Scenario: 翻到下一页
- **WHEN** 当前 page=5, 调用 `setCurrentPage(6)`
- **THEN** `currentPage_` 设为 6，`setNeedsDisplay()` 被调用

#### Scenario: 页码越界保护
- **WHEN** 调用 `setCurrentPage(-1)` 或 `setCurrentPage(totalPages + 10)`
- **THEN** 页码被 clamp 到有效范围内

#### Scenario: 索引未完成时查询总页数
- **WHEN** 后台分页进行中，调用 `totalPages()`
- **THEN** 返回 -1

#### Scenario: 顺序翻页不依赖完整索引
- **WHEN** PageIndex 只有 10 页但用户翻到第 10 页后继续翻页
- **THEN** 通过 layoutPage() 的 nextOffset 计算下一页，将新 offset 追加到 PageIndex

### Requirement: ReaderContentView 渲染
ReaderContentView 的 `onDraw()` SHALL：
1. 若 TextSource 为空或状态为 Error，不绘制
2. 若 PageIndex 为空且 TextSource 可用，尝试加载缓存或启动后台分页
3. 通过 `textSource_->read(currentPageOffset)` 获取文本
4. 调用 `layoutPage(currentPageOffset)` 获取当前页行布局
5. 逐行调用 `canvas.drawTextN()` 绘制：
   - 每行 baseline = `currentY + font->ascender`
   - 每行后 `currentY += lineHeight`
   - 段落结束行后额外 `currentY += paragraphSpacing`
6. 文本从 View 左上角开始渲染（顶部对齐，左对齐）

若 `read()` 返回 `{nullptr, 0}`（文本尚不可用），SHALL 在页面中央显示 "正在加载..." 提示文本。

#### Scenario: 正常渲染一页
- **WHEN** 当前页有 15 行文本，行距 1.6x，字号 20px
- **THEN** 15 行文本从顶部开始以 1.6 倍行距渲染

#### Scenario: 文本不可用时显示加载提示
- **WHEN** TextSource 处于 Converting 状态且当前 offset 超出 availableSize
- **THEN** 页面中央显示 "正在加载..."

#### Scenario: 无 TextSource 不渲染
- **WHEN** 未设置 TextSource
- **THEN** `onDraw` 不绘制任何内容，不崩溃

### Requirement: ReaderContentView 加载状态回调
ReaderContentView SHALL 提供状态回调机制，通知外部（ReaderViewController）当前加载状态变化，以便更新页脚显示：
- `setStatusCallback(std::function<void()> callback)` — 设置状态变化回调
- 后台分页 task 每完成 10% 进度或状态变化时调用此回调
- 回调在后台 task 线程中触发，ReaderViewController 负责安全地更新 UI

#### Scenario: 分页进度通知
- **WHEN** 后台分页从 0% 推进到 10%
- **THEN** statusCallback 被调用，ReaderViewController 可查询 `pageIndexProgress()` 更新页脚

#### Scenario: 分页完成通知
- **WHEN** 后台分页完成
- **THEN** statusCallback 被调用，ReaderViewController 可查询 `totalPages()` 显示完整页码

## REMOVED Requirements

### Requirement: ReaderContentView 文本配置
**Reason**: `setTextBuffer(const char* buf, uint32_t size)` 被 `setTextSource(TextSource* source)` 替代，不再使用裸文本指针。
**Migration**: 所有调用方改用 `setTextSource()`。
