## ADDED Requirements

### Requirement: 自动换行渲染
系统 SHALL 提供 `ui_canvas_draw_text_wrapped()` 函数，将文本在指定行宽内自动换行并渲染到 framebuffer。函数 SHALL 返回 `ui_text_result_t` 结构，包含已消耗的字节数、渲染行数和是否到达文本末尾。

#### Scenario: 长文本自动换行
- **WHEN** 在 max_width=492 的区域内渲染一段超过一行宽度的中文文本
- **THEN** 文本 SHALL 在接近行宽边界处自动断行，每行不超过 max_width 像素

#### Scenario: 短文本单行
- **WHEN** 文本宽度不超过 max_width
- **THEN** SHALL 渲染为单行，`lines_rendered` = 1

#### Scenario: 换行符处理
- **WHEN** 文本包含 `\n` 字符
- **THEN** SHALL 在换行符处强制换行，开始新的一行

### Requirement: CJK 逐字断行
对于 CJK 字符（U+3040-U+309F 平假名、U+30A0-U+30FF 片假名、U+4E00-U+9FFF CJK 统一汉字），换行引擎 SHALL 允许在每个字符后断行。

#### Scenario: 中文逐字断行
- **WHEN** 渲染一串中文 "奇迹肯定会发生"，且第 5 个字符超出行宽
- **THEN** SHALL 在第 4 个字符后断行，第 5 个字符移到下一行

#### Scenario: 中日混排
- **WHEN** 渲染中日混排文本 "キセキは起こる"
- **THEN** 每个假名和汉字 SHALL 都是合法断点

### Requirement: 英文单词断行
对于连续的 ASCII 字母序列，换行引擎 SHALL 将其作为不可拆分的单词单元处理，仅在单词边界（空格）处断行。

#### Scenario: 英文单词不拆分
- **WHEN** 渲染 "hello sunshine" 且 "sunshine" 超出行宽
- **THEN** SHALL 将整个 "sunshine" 移到下一行，不在字母之间断开

#### Scenario: 单词过长
- **WHEN** 单个英文单词宽度超过 max_width
- **THEN** SHALL 强制在行宽处截断该单词

### Requirement: 标点禁则
换行引擎 SHALL 实现 CJK 标点禁则（Kinsoku Shori）规则。

行首禁止字符（不得出现在行首）：`，。、；：？！）》」』】〉…——`
行尾禁止字符（不得出现在行尾）：`（《「『【〈`

#### Scenario: 行首禁止标点
- **WHEN** 断行点后的第一个字符是 `，`（行首禁止）
- **THEN** SHALL 将该标点连同前一个字符保留在上一行末尾

#### Scenario: 行尾禁止标点
- **WHEN** 断行点前的最后一个字符是 `（`（行尾禁止）
- **THEN** SHALL 将该字符推到下一行开头

### Requirement: 分页渲染
系统 SHALL 提供 `ui_canvas_draw_text_page()` 函数，从指定字节偏移开始渲染文本，直到填满指定的页面高度或到达文本末尾。函数 SHALL 返回 `ui_text_result_t`，调用方可用 `bytes_consumed` 计算下一页的起始偏移。

#### Scenario: 首页渲染
- **WHEN** 调用 `ui_canvas_draw_text_page(fb, 24, 40, 492, 870, line_height, font, text, 0, 0x00)`
- **THEN** SHALL 从文本开头渲染，填满高度 870px 的区域后停止，返回已消耗的字节数

#### Scenario: 后续页渲染
- **WHEN** 使用前一页返回的 `bytes_consumed` 作为 `start_offset` 调用
- **THEN** SHALL 从上一页结束处继续渲染，文本无缝衔接

#### Scenario: 最后一页
- **WHEN** 剩余文本不足以填满一页
- **THEN** SHALL 渲染全部剩余文本，`reached_end` = true

#### Scenario: 起始偏移超出文本长度
- **WHEN** start_offset >= strlen(text)
- **THEN** SHALL 不渲染任何内容，`reached_end` = true，`bytes_consumed` = 0

### Requirement: 返回值结构
`ui_text_result_t` 结构 SHALL 包含以下字段：
- `bytes_consumed` (int): 本次渲染消耗的 UTF-8 字节数
- `lines_rendered` (int): 实际渲染的行数
- `last_y` (int): 最后一行的 y 坐标
- `reached_end` (bool): 是否已到达文本末尾

#### Scenario: 返回值正确
- **WHEN** 渲染一页文本后
- **THEN** `bytes_consumed` SHALL 等于从 start_offset 到最后一行末尾字符的字节距离，`lines_rendered` SHALL 等于实际渲染的行数
