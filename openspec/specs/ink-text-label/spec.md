## ADDED Requirements

### Requirement: TextLabel 基本文字显示
`ink::TextLabel` SHALL 继承 `ink::View`，显示单行或多行文字。

构造参数/属性:
- `text_`: `std::string`，显示内容（UTF-8）
- `font_`: `const EpdFont*`，字体指针，默认 nullptr
- `color_`: `uint8_t`，文字颜色，默认 Color::Black
- `alignment_`: `Align` 枚举（Start/Center/End），默认 Start
- `maxLines_`: `int`，最大显示行数，默认 1。0 表示不限制

TextLabel 的默认 `refreshHint_` SHALL 为 `RefreshHint::Quality`。

#### Scenario: 显示单行文字
- **WHEN** TextLabel 设置 text\_="Hello" 且 font\_ 有效
- **THEN** onDraw 在 Canvas 上绘制 "Hello"，颜色为 color\_，竖直居中于 frame 高度

#### Scenario: font 为空时不崩溃
- **WHEN** TextLabel 的 font\_ 为 nullptr
- **THEN** onDraw 不执行任何绘制，不崩溃

### Requirement: TextLabel 水平对齐
TextLabel SHALL 根据 `alignment_` 计算文字的水平起始位置：
- `Align::Start`: x = 0（左对齐）
- `Align::Center`: x = (frame.w - textWidth) / 2（居中）
- `Align::End`: x = frame.w - textWidth（右对齐）

#### Scenario: 居中对齐
- **WHEN** TextLabel frame 宽 200px，文字宽 80px，alignment\_ = Center
- **THEN** 文字绘制起始 x = 60

#### Scenario: 右对齐
- **WHEN** TextLabel frame 宽 200px，文字宽 80px，alignment\_ = End
- **THEN** 文字绘制起始 x = 120

### Requirement: TextLabel 文字截断
当文字宽度超过 frame 宽度时，TextLabel SHALL 截断显示。单行模式下超出部分不渲染（Canvas 裁剪自动处理）。

#### Scenario: 长文字被截断
- **WHEN** 文字渲染宽度为 600px，frame 宽度为 400px
- **THEN** 只显示 frame 范围内的部分，超出部分被 Canvas 裁剪

### Requirement: TextLabel 多行支持
当 `maxLines_` > 1 时，TextLabel SHALL 支持多行显示。多行文字通过 `\n` 换行符分行。每行独立绘制，行间距为 font 的行高。超过 maxLines\_ 的行不显示。

#### Scenario: 两行文字
- **WHEN** text\_ = "Line1\nLine2"，maxLines\_ = 2
- **THEN** 绘制两行文字，第一行在顶部，第二行在下方（间距为字体行高）

#### Scenario: 超过最大行数
- **WHEN** text\_ = "L1\nL2\nL3"，maxLines\_ = 2
- **THEN** 只显示前两行

### Requirement: TextLabel setText 触发重绘
调用 `setText(const std::string&)` SHALL 更新 text\_ 并调用 `setNeedsDisplay()`。如果新文字与旧文字相同，SHALL 不触发重绘。

#### Scenario: 文字变化触发重绘
- **WHEN** 调用 setText("New")，当前 text\_ 为 "Old"
- **THEN** text\_ 更新为 "New"，needsDisplay\_ 为 true

#### Scenario: 相同文字不重绘
- **WHEN** 调用 setText("Same")，当前 text\_ 为 "Same"
- **THEN** needsDisplay\_ 不变

### Requirement: TextLabel intrinsicSize
`intrinsicSize()` SHALL 返回基于当前 font\_ 和 text\_ 计算的尺寸：
- 宽度: 文字渲染像素宽度（通过 Canvas::measureText 计算）
- 高度: font 的行高 × 行数

font\_ 为 nullptr 时返回 `{0, 0}`。

#### Scenario: 单行固有尺寸
- **WHEN** font 24px，text\_ = "Test"（渲染宽 48px），font 行高 32px
- **THEN** intrinsicSize() 返回 {48, 32}
