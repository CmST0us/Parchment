### Requirement: ReaderContentView 类定义
`ReaderContentView` SHALL 继承 `ink::View`，位于 `main/views/ReaderContentView.h`。该 View 负责阅读文本的折行、分页和渲染，不处理触摸事件、文件 I/O 或进度存储。

#### Scenario: 类继承
- **WHEN** 创建 `ReaderContentView` 实例
- **THEN** 它是 `ink::View` 的子类，可作为子 View 添加到任何父 View

### Requirement: ReaderContentView 文本配置
ReaderContentView SHALL 提供以下配置方法：
- `setTextBuffer(const char* buf, uint32_t size)` — 设置 UTF-8 文本缓冲区（非拥有指针，调用方负责生命周期）
- `setFont(const EpdFont* font)` — 设置渲染字体
- `setLineSpacing(uint8_t spacing10x)` — 设置行距倍数（×10），如 16 表示 1.6 倍。范围 10~25。
- `setParagraphSpacing(uint8_t px)` — 设置段间距像素数。范围 0~24。
- `setTextColor(uint8_t color)` — 设置文字颜色

任何配置变更 SHALL 使已有分页表失效（清空 `pages_`），下次 `onDraw` 时重新分页。

#### Scenario: 设置文本和字体
- **WHEN** 调用 `setTextBuffer(buf, 1024)` 和 `setFont(font)`
- **THEN** View 持有文本指针和字体引用，准备好进行分页和渲染

#### Scenario: 修改行距触发重新分页
- **WHEN** 已完成分页后调用 `setLineSpacing(20)`（从 1.6x 改为 2.0x）
- **THEN** `pages_` 被清空，下次 `onDraw` 时按新行距重新分页

### Requirement: ReaderContentView 懒分页
ReaderContentView SHALL 在首次 `onDraw()` 调用时自动执行分页（当 `pages_` 为空且文本缓冲区非空时）。分页使用 View 的 `bounds()` 获取可用尺寸。

分页完成后 SHALL 恢复 `currentPage_`：若之前设置过页码且在新页表范围内，保持不变；否则重置为 0。

#### Scenario: 首次绘制触发分页
- **WHEN** 设置了文本和字体后，首次 `onDraw` 被调用
- **THEN** `paginate()` 自动执行，`pages_` 填充完毕，随后绘制当前页

#### Scenario: 分页后再次绘制不重复
- **WHEN** 已完成分页后，`onDraw` 再次被调用（翻页导致）
- **THEN** 不重新执行分页，直接绘制当前页

### Requirement: ReaderContentView 统一布局引擎 layoutPage
ReaderContentView SHALL 提供内部方法 `layoutPage(uint32_t startOffset)` 作为折行和页面填充的单一来源。该方法 SHALL：

1. 使用 `bounds().w` 作为可用行宽
2. 使用 `bounds().h` 作为可用页面高度
3. 计算行高 = `font->advance_y * lineSpacing10x / 10`，最小为 `font->advance_y`
4. 从 `startOffset` 开始，按像素高度填充页面：
   - 遇到 `\n`（或 `\r\n`）：记录为空行（段落结束），消耗 `lineHeight + paragraphSpacing` 像素
   - 否则：逐字符测量宽度（`epd_get_glyph` 获取 `advance_x`），超出行宽时折行，消耗 `lineHeight` 像素；若该行末尾紧跟 `\n`，额外消耗 `paragraphSpacing` 像素
   - 剩余高度不足一个 `lineHeight` 时停止
5. 返回该页所有行的信息（起始偏移、结束偏移、是否段落结尾）和下一页起始偏移

`paginate()` 和 `onDraw()` SHALL 都使用 `layoutPage()` ，保证分页计算和渲染使用完全相同的折行逻辑。

#### Scenario: 纯文本无换行符
- **WHEN** 文本为 600 字节连续中文（无 `\n`），行宽可容纳 20 个字符
- **THEN** layoutPage 自动折行，每行约 20 字符，直到页面高度用完

#### Scenario: 多段落文本
- **WHEN** 文本包含多个 `\n` 分隔的短段落，paragraphSpacing = 8px
- **THEN** 每个段落后额外消耗 8px，同一页可容纳的文本行数少于纯连续文本

#### Scenario: CR+LF 处理
- **WHEN** 文本使用 `\r\n` 作为换行符
- **THEN** `\r\n` 被视为单个段落结束，等同于 `\n`

#### Scenario: 极端字符不卡死
- **WHEN** 一个字符的宽度超过整行宽度
- **THEN** 至少前进一个字符，不会死循环

### Requirement: ReaderContentView 分页 API
ReaderContentView SHALL 提供以下分页查询和控制 API：
- `int totalPages() const` — 返回总页数
- `int currentPage() const` — 返回当前页码（0-indexed）
- `void setCurrentPage(int page)` — 设置当前页并调用 `setNeedsDisplay()`
- `uint32_t currentPageOffset() const` — 返回当前页的起始字节偏移
- `int pageForByteOffset(uint32_t offset) const` — 根据字节偏移查找对应页码（用于恢复进度）

#### Scenario: 翻到下一页
- **WHEN** 当前 page=5, 调用 `setCurrentPage(6)`
- **THEN** `currentPage_` 设为 6，`setNeedsDisplay()` 被调用，下次渲染显示第 6 页内容

#### Scenario: 页码越界保护
- **WHEN** 调用 `setCurrentPage(-1)` 或 `setCurrentPage(totalPages + 10)`
- **THEN** 页码被 clamp 到 `[0, totalPages - 1]` 范围内

#### Scenario: 字节偏移恢复进度
- **WHEN** 调用 `pageForByteOffset(12345)`，该偏移落在第 42 页范围内
- **THEN** 返回 42

### Requirement: ReaderContentView 渲染
ReaderContentView 的 `onDraw()` SHALL：
1. 若 `pages_` 为空且有文本数据，先执行 `paginate()`
2. 调用 `layoutPage(pages_[currentPage_])` 获取当前页行布局
3. 从 View 顶部（y=0）开始，逐行调用 `canvas.drawTextN()` 绘制：
   - 每行 baseline = `currentY + font->ascender`
   - 每行后 `currentY += lineHeight`
   - 段落结束行后额外 `currentY += paragraphSpacing`
4. 文本从 View 左上角开始渲染（顶部对齐，左对齐），不垂直居中

#### Scenario: 正常渲染一页
- **WHEN** 当前页有 15 行文本，行距 1.6x，字号 20px
- **THEN** 15 行文本从顶部开始以 1.6 倍行距渲染，行间距视觉均匀

#### Scenario: 无文本不渲染
- **WHEN** 未设置文本缓冲区或字体
- **THEN** `onDraw` 不绘制任何内容，不崩溃
