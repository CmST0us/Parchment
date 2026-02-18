## ADDED Requirements

### Requirement: Color::Clear 透明色常量
`ink::Color` 命名空间 SHALL 包含 `Clear` 常量，值为 `0x01`。此值作为哨兵表示"透明，不绘制背景"。

`0x01` 不是 4bpp 灰度中的有效灰度级（有效值为 0x00, 0x10, 0x20, ..., 0xF0），仅用于 `backgroundColor` 属性的透明判断。

#### Scenario: Color::Clear 值
- **WHEN** 读取 `Color::Clear`
- **THEN** 值为 `0x01`

#### Scenario: Color::Clear 与 Color::Black 区分
- **WHEN** 比较 `Color::Clear` 与 `Color::Black`
- **THEN** 两者不相等（`0x01 != 0x00`）

## MODIFIED Requirements

### Requirement: 文字渲染
Canvas SHALL 提供文字绘制方法，接受 `const EpdFont*` 指针：
- `drawText(font, text, x, y, color)`: 绘制 UTF-8 单行文字，y 为基线坐标
- `drawTextN(font, text, maxBytes, x, y, color)`: 绘制指定最大字节数的文字
- `measureText(font, text)`: 返回文字渲染后的像素宽度（不写入 framebuffer）

文字渲染 SHALL 支持 zlib 压缩的 glyph bitmap 解压。

字符渲染的 alpha 混合 SHALL 读取 framebuffer 中的实际像素值作为背景色，使用 `bg + alpha * (fg - bg) / 15` 公式进行线性插值。此行为 SHALL 与 `drawBitmapFg()` 的 alpha 混合逻辑一致。

#### Scenario: 绘制中文文字
- **WHEN** 使用已加载的 `EpdFont*`（24px 中文字体），调用 `drawText(font, "你好", 10, 30, 0x00)`
- **THEN** "你好" 两个字在局部坐标 `(10, 30)` 基线位置渲染，使用抗锯齿 alpha 混合

#### Scenario: measureText 度量宽度
- **WHEN** 调用 `measureText(font, "ABC")`
- **THEN** 返回三个字符 advance_x 的累计值

#### Scenario: 文字被裁剪区域截断
- **WHEN** 文字渲染位置接近裁剪区域右边界
- **THEN** 超出裁剪区域的字符像素被自动丢弃，不写入 framebuffer

#### Scenario: 非白色背景上的文字反走样
- **WHEN** 在灰色背景 (0x80) 上绘制黑色文字
- **THEN** 文字边缘的半透明像素与灰色背景正确混合，无白色光晕

#### Scenario: 白色背景上的文字反走样
- **WHEN** 在白色背景 (0xF0) 上绘制黑色文字
- **THEN** alpha 混合结果与此前行为一致（背景为白色时 getPixel 返回 0xF0）
