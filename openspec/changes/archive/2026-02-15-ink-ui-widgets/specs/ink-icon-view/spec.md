## ADDED Requirements

### Requirement: IconView 图标显示
`ink::IconView` SHALL 继承 `ink::View`，显示 32×32 4bpp 位图图标。

属性:
- `iconData_`: `const uint8_t*`，指向 32×32 4bpp 位图数据（512 bytes），默认 nullptr
- `tintColor_`: `uint8_t`，前景色，默认 Color::Black

IconView 的默认 `refreshHint_` SHALL 为 `RefreshHint::Fast`。

#### Scenario: 显示图标
- **WHEN** IconView 设置了有效的 iconData\_ 和 tintColor\_
- **THEN** onDraw 使用 Canvas::drawBitmapFg() 以 tintColor\_ 渲染图标，图标在 frame 中居中

#### Scenario: 无图标数据不崩溃
- **WHEN** iconData\_ 为 nullptr
- **THEN** onDraw 不执行任何绘制，不崩溃

### Requirement: IconView 居中渲染
IconView SHALL 将 32×32 图标居中于其 frame。若 frame 大于 32×32，图标 SHALL 绘制在 frame 中心。若 frame 小于 32×32，图标 SHALL 被 Canvas 裁剪。

#### Scenario: frame 大于图标
- **WHEN** frame 为 48×48，图标 32×32
- **THEN** 图标绘制在 (8, 8) 位置（居中）

#### Scenario: frame 等于图标
- **WHEN** frame 为 32×32
- **THEN** 图标绘制在 (0, 0)

### Requirement: IconView setIcon 更新
`setIcon(const uint8_t* data)` SHALL 更新 iconData\_ 并调用 `setNeedsDisplay()`。

#### Scenario: 切换图标
- **WHEN** 调用 setIcon(newIconData)
- **THEN** iconData\_ 更新，needsDisplay\_ 为 true

### Requirement: IconView intrinsicSize
`intrinsicSize()` SHALL 返回 `{32, 32}`（固定图标尺寸）。

#### Scenario: 固有尺寸
- **WHEN** 调用 intrinsicSize()
- **THEN** 返回 {32, 32}
