## ADDED Requirements

### Requirement: .bin 字体文件头
.bin 字体文件 SHALL 以固定长度文件头开始，包含以下字段（小端序）：

| 偏移 | 大小 | 字段 | 说明 |
|------|------|------|------|
| 0 | 4B | magic | 固定 `"PFNT"` (0x50464E54) |
| 4 | 1B | version | 格式版本，当前为 1 |
| 5 | 1B | font_height | 基础字号像素高度（32） |
| 6 | 4B | glyph_count | 字形总数 |
| 10 | 64B | family_name | 字体族名（UTF-8，零填充） |
| 74 | 2B | ascender | 基线以上高度 |
| 76 | 2B | descender | 基线以下深度 |
| 78 | 50B | reserved | 保留，全零 |

文件头总大小为 128 字节。

#### Scenario: 验证文件头 magic
- **WHEN** 读取 .bin 文件头前 4 字节
- **THEN** 值为 `"PFNT"` (0x50, 0x46, 0x4E, 0x54)

#### Scenario: 解析 glyph 数量
- **WHEN** 读取文件头偏移 6 处的 uint32
- **THEN** 获得该字体包含的字形总数

### Requirement: Glyph 条目格式
文件头之后 SHALL 紧跟 glyph_count 个 glyph 条目，每个条目 16 字节（小端序）：

| 偏移 | 大小 | 字段 | 说明 |
|------|------|------|------|
| 0 | 4B | unicode | Unicode 码点 |
| 4 | 1B | width | 位图像素宽 |
| 5 | 1B | height | 位图像素高 |
| 6 | 1B | advance_x | 前进宽度 |
| 7 | 1B | x_offset | 水平偏移 (int8) |
| 8 | 1B | y_offset | 垂直偏移 (int8) |
| 9 | 3B | bitmap_offset | 位图数据在文件中的偏移（低 24 位） |
| 12 | 4B | bitmap_size | RLE 编码后位图大小 |

Glyph 条目 SHALL 按 unicode 值升序排列。

#### Scenario: 读取 glyph 条目
- **WHEN** 读取第 N 个 glyph 条目（文件偏移 128 + N × 16）
- **THEN** 获得该字形的 unicode 码点、位图尺寸、前进宽度和位图数据位置

#### Scenario: Glyph 按 unicode 升序排列
- **WHEN** 遍历所有 glyph 条目
- **THEN** 每个条目的 unicode 值严格大于前一个

### Requirement: 4bpp RLE 位图编码
Glyph 位图数据 SHALL 使用 4bpp RLE 编码。编码规则：
- 每个 RLE 单元为 1 字节：高 4 位 = 重复次数 (1-15)，低 4 位 = 灰度值 (0x0=黑, 0xF=白)
- 重复次数 0 表示行结束标记（低 4 位忽略）
- 每行像素之后 SHALL 有一个行结束标记
- 解码后每行像素数 SHALL 等于 glyph 的 width 字段

#### Scenario: 解码纯黑行
- **WHEN** RLE 数据为 `[0xA0, 0x00]`（10 个黑色像素 + 行结束），glyph width=10
- **THEN** 解码为 10 个 0x0 灰度值

#### Scenario: 解码混合灰度行
- **WHEN** RLE 数据为 `[0x1F, 0x30, 0x38, 0x1F, 0x00]`（1 白 + 3 黑 + 3 灰(0x8) + 1 白 + 行结束）
- **THEN** 解码为 `[0xF, 0x0, 0x0, 0x0, 0x8, 0x8, 0x8, 0xF]`

#### Scenario: 重复次数超过 15 时分段
- **WHEN** 一行中有 20 个连续白色像素
- **THEN** 编码为 `[0xFF, 0x5F, 0x00]`（15 白 + 5 白 + 行结束）

### Requirement: 文件完整性
.bin 文件 SHALL 满足以下完整性约束：
- 所有 bitmap_offset + bitmap_size SHALL 不超过文件总大小
- glyph_count × 16 + 128 SHALL 不超过最小 bitmap_offset
- 每个 glyph 的 RLE 解码后像素数 SHALL 等于 width × height

#### Scenario: 加载时校验 magic
- **WHEN** 文件头 magic 不是 `"PFNT"`
- **THEN** 加载失败，返回错误

#### Scenario: 加载时校验版本
- **WHEN** 文件头 version 不是 1
- **THEN** 加载失败，返回错误
