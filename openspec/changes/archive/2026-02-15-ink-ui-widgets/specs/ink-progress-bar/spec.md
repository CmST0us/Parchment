## ADDED Requirements

### Requirement: ProgressBarView 进度条
`ink::ProgressBarView` SHALL 继承 `ink::View`，显示水平进度条。

属性:
- `value_`: `int`，进度值 0-100，默认 0
- `trackColor_`: `uint8_t`，轨道颜色，默认 Color::Light
- `fillColor_`: `uint8_t`，填充颜色，默认 Color::Black
- `trackHeight_`: `int`，轨道高度，默认 8px

ProgressBarView 的默认 `refreshHint_` SHALL 为 `RefreshHint::Quality`。

#### Scenario: 50% 进度
- **WHEN** value\_ = 50，frame 宽 200px
- **THEN** 绘制 100px 宽的填充条 + 100px 宽的空轨道

#### Scenario: 0% 进度
- **WHEN** value\_ = 0
- **THEN** 只绘制空轨道，无填充

#### Scenario: 100% 进度
- **WHEN** value\_ = 100
- **THEN** 整条轨道被填充

### Requirement: ProgressBarView 绘制
onDraw SHALL:
1. 绘制完整轨道矩形（trackColor\_，trackHeight\_ 高，竖直居中于 frame）
2. 在轨道上绘制填充矩形（fillColor\_，宽度 = frame.w × value\_ / 100）

#### Scenario: 轨道竖直居中
- **WHEN** frame 高 24px，trackHeight\_ = 8px
- **THEN** 轨道 y 偏移 = (24 - 8) / 2 = 8px

### Requirement: ProgressBarView setValue 更新
`setValue(int v)` SHALL 将 value\_ 裁剪到 [0, 100] 范围，然后调用 `setNeedsDisplay()`。如果值未变化，SHALL 不触发重绘。

#### Scenario: 设置值触发重绘
- **WHEN** 调用 setValue(75)，当前值为 50
- **THEN** value\_ = 75，needsDisplay\_ 为 true

#### Scenario: 超出范围裁剪
- **WHEN** 调用 setValue(120)
- **THEN** value\_ = 100

#### Scenario: 负值裁剪
- **WHEN** 调用 setValue(-10)
- **THEN** value\_ = 0

### Requirement: ProgressBarView intrinsicSize
`intrinsicSize()` SHALL 返回 `{-1, trackHeight_}`（宽度由父 View 决定，高度为轨道高度）。

#### Scenario: 默认固有尺寸
- **WHEN** trackHeight\_ = 8
- **THEN** intrinsicSize() 返回 {-1, 8}
