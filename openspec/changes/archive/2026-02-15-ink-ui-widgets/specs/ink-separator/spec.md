## ADDED Requirements

### Requirement: SeparatorView 分隔线
`ink::SeparatorView` SHALL 继承 `ink::View`，绘制水平或垂直分隔线。

属性:
- `orientation_`: `Orientation` 枚举（Horizontal/Vertical），默认 Horizontal
- `lineColor_`: `uint8_t`，线条颜色，默认 Color::Medium
- `thickness_`: `int`，线条粗细，默认 1px

SeparatorView 的默认 `refreshHint_` SHALL 为 `RefreshHint::Fast`。

#### Scenario: 水平分隔线
- **WHEN** orientation\_ = Horizontal，frame 宽 500px
- **THEN** 绘制一条 500px 宽、1px 高的水平线，竖直居中于 frame

#### Scenario: 垂直分隔线
- **WHEN** orientation\_ = Vertical，frame 高 100px
- **THEN** 绘制一条 1px 宽、100px 高的垂直线，水平居中于 frame

### Requirement: Orientation 枚举
`ink::Orientation` SHALL 定义:
- `Horizontal`: 水平方向
- `Vertical`: 垂直方向

#### Scenario: 枚举可用
- **WHEN** 使用 Orientation::Horizontal 和 Orientation::Vertical
- **THEN** 编译通过

### Requirement: SeparatorView 绘制
onDraw SHALL:
- Horizontal: 使用 Canvas::drawHLine()，y 坐标 = (frame.h - thickness\_) / 2
- Vertical: 使用 Canvas::drawVLine()，x 坐标 = (frame.w - thickness\_) / 2

#### Scenario: 水平线居中
- **WHEN** frame 高 12px，thickness\_ = 1
- **THEN** 线条绘制在 y=5（(12-1)/2 取整）

### Requirement: SeparatorView intrinsicSize
`intrinsicSize()` SHALL:
- Horizontal: 返回 `{-1, thickness_}`（宽度由父决定，高度为线条粗细）
- Vertical: 返回 `{thickness_, -1}`（宽度为线条粗细，高度由父决定）

#### Scenario: 水平固有尺寸
- **WHEN** orientation\_ = Horizontal，thickness\_ = 1
- **THEN** intrinsicSize() 返回 {-1, 1}

#### Scenario: 垂直固有尺寸
- **WHEN** orientation\_ = Vertical，thickness\_ = 2
- **THEN** intrinsicSize() 返回 {2, -1}
