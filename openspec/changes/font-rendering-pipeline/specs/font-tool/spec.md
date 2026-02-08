## ADDED Requirements

### Requirement: TTF/OTF 转 .bin 格式
工具 SHALL 接受 TTF 或 OTF 字体文件作为输入，输出 1-bit packed bitmap 格式的 .bin 字体文件。

#### Scenario: 基本转换
- **WHEN** 执行 `python tools/generate_font_bin.py NotoSerifSC-Regular.otf output.bin --size 24`
- **THEN** SHALL 生成符合 V2 格式的 .bin 文件
- **AND** 文件 header 中 font_height SHALL 为 24，version SHALL 为 2

#### Scenario: 指定字号
- **WHEN** 使用 `--size` 参数指定 24/28/32/36 中的任一值
- **THEN** SHALL 以该像素大小渲染所有 glyph

### Requirement: 字符集选择
工具 SHALL 支持通过参数控制生成的字符集范围。

#### Scenario: 默认字符集
- **WHEN** 未指定字符集参数
- **THEN** SHALL 包含 ASCII 可打印字符 (U+0020~U+007E) + CJK 统一汉字基本区 (U+4E00~U+9FFF) + 中文标点符号

#### Scenario: 仅 ASCII
- **WHEN** 使用 `--ascii-only` 参数
- **THEN** SHALL 仅包含 ASCII 可打印字符

### Requirement: .bin 文件格式输出
工具生成的 .bin 文件 SHALL 严格遵循以下二进制格式：
- Header: 134 字节（char_count uint32 + font_height uint8 + version uint8 + family_name char[64] + style_name char[64]）
- Glyph Table: 每字符 20 字节（unicode uint16 + advance_w uint16 + bitmap_w uint8 + bitmap_h uint8 + x_offset int8 + y_offset int8 + data_offset uint32 + data_size uint32 + reserved uint32）
- Bitmap Data: 1-bit packed，MSB first，每行 ceil(bitmap_w/8) 字节对齐

#### Scenario: 文件格式验证
- **WHEN** 生成 .bin 文件后读取其 header
- **THEN** 前 4 字节 SHALL 为 little-endian 编码的字符数量
- **AND** 第 5 字节 SHALL 为字体高度
- **AND** 第 6 字节 SHALL 为 2 (version)

### Requirement: Edge Smoothing
工具 SHALL 支持可选的边缘平滑处理，减少 1-bit 量化导致的锯齿。

#### Scenario: 默认启用平滑
- **WHEN** 未指定 `--no-smooth` 参数
- **THEN** SHALL 对 glyph 边缘进行平滑处理后再二值化

#### Scenario: 禁用平滑
- **WHEN** 使用 `--no-smooth` 参数
- **THEN** SHALL 直接二值化，不做平滑处理

### Requirement: 白色阈值控制
工具 SHALL 支持 `--white` 参数控制二值化阈值，影响笔画粗细。

#### Scenario: 调整阈值
- **WHEN** 使用 `--white 80`（默认值）
- **THEN** 灰度值低于 (255-80)=175 的像素 SHALL 被视为前景（黑色）
- **AND** 其余像素 SHALL 被视为背景（白色）

### Requirement: Python 依赖
工具 SHALL 仅依赖以下 Python 库：`freetype-py`、`numpy`，可选依赖 `Pillow`（预览功能）。

#### Scenario: 最小依赖安装
- **WHEN** 执行 `pip install freetype-py numpy`
- **THEN** 工具的核心功能（.bin 生成）SHALL 正常工作
