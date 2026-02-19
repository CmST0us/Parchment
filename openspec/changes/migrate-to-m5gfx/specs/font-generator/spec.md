## ADDED Requirements

### Requirement: TTF/OTF 输入
字体生成工具 SHALL 接受 TTF 或 OTF 格式的字体文件作为输入，使用 FreeType 库进行光栅化。

#### Scenario: 加载 TTF 字体
- **WHEN** 输入 `LXGWWenKai-Regular.ttf`
- **THEN** 成功加载字体并读取 family name 和 glyph 总数

#### Scenario: 不支持的格式
- **WHEN** 输入非 TTF/OTF 文件
- **THEN** 报告错误并退出

### Requirement: 字符范围指定
工具 SHALL 支持通过命令行参数指定要包含的 Unicode 字符范围：
- `--range U+0020-U+007E`: 指定连续范围
- `--charset gb2312`: 预设字符集
- `--charset gb18030`: 预设字符集
- 多个 `--range` 和 `--charset` 可组合使用

#### Scenario: GB2312 一级字符集
- **WHEN** 使用 `--charset gb2312`
- **THEN** 包含 GB2312 一级汉字（约 3755 个）+ ASCII + 常用标点

#### Scenario: 组合范围
- **WHEN** 使用 `--charset gb2312 --range U+2600-U+26FF`
- **THEN** 包含 GB2312 字符集和杂项符号区块

### Requirement: 4bpp 灰度光栅化
工具 SHALL 以指定字号（默认 32px）光栅化每个字符为 8bpp 灰度 bitmap（FreeType 输出），然后量化为 4bpp（16 级灰度）。

量化规则：`gray_4bit = round(gray_8bit / 17.0)`，范围 0–15。

#### Scenario: 32px 中文字符光栅化
- **WHEN** 对 '你' 进行 32px 光栅化
- **THEN** 产生约 24×28 的 4bpp 灰度 bitmap，包含抗锯齿边缘

#### Scenario: 空格字符
- **WHEN** 对空格字符 (U+0020) 光栅化
- **THEN** bitmap 宽高为 0，advance_x 为字号的约 1/4

### Requirement: RLE 编码
工具 SHALL 将每个 glyph 的 4bpp bitmap 使用 RLE 编码压缩：
- 每个 RLE 单元：高 4 位 = 重复次数 (1-15)，低 4 位 = 灰度值
- 每行末尾添加行结束标记（重复次数为 0 的字节）
- 连续超过 15 个相同灰度像素时分段为多个 RLE 单元

#### Scenario: 编码一行混合灰度
- **WHEN** 一行像素为 `[15,15,15,0,0,0,0,0,8,8,15]`
- **THEN** 编码为 `[0x3F, 0x50, 0x28, 0x1F, 0x00]`（3 白, 5 黑, 2 灰(8), 1 白, 行结束）

### Requirement: .bin 文件输出
工具 SHALL 按 bin-font-format 规范输出 .bin 文件：128 字节文件头 + glyph 条目（按 unicode 升序排列）+ RLE 位图数据。

#### Scenario: 生成完整 .bin 文件
- **WHEN** 使用 `python generate_bin_font.py LXGWWenKai-Regular.ttf -o reading_font.bin --charset gb2312 --size 32`
- **THEN** 生成 .bin 文件，包含文件头、约 4000 个 glyph 条目和 RLE 位图数据

### Requirement: 过滤不支持的字符
工具 SHALL 在光栅化前检查字体是否支持目标字符。不支持的字符（FreeType 返回 glyph index 0）SHALL 被跳过并输出警告日志。

#### Scenario: 字体不支持某字符
- **WHEN** 请求的字符在字体中没有对应 glyph
- **THEN** 跳过该字符并打印警告，不中断生成过程

### Requirement: 统计信息输出
工具 SHALL 在生成完成后输出统计摘要：
- 总 glyph 数
- 文件大小
- 平均 RLE 压缩率
- 跳过的字符数

#### Scenario: 生成完成输出统计
- **WHEN** .bin 文件成功生成
- **THEN** 打印总 glyph 数、文件大小（MB）、压缩率、跳过字符数
