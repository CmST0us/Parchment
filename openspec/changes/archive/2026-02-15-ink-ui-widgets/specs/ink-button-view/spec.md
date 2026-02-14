## ADDED Requirements

### Requirement: ButtonView 按钮
`ink::ButtonView` SHALL 继承 `ink::View`，提供可点击的按钮组件。

属性:
- `label_`: `std::string`，按钮文字
- `font_`: `const EpdFont*`，字体指针
- `icon_`: `const uint8_t*`，可选图标数据（32×32 4bpp），默认 nullptr
- `style_`: `ButtonStyle` 枚举（Primary/Secondary/Icon），默认 Primary
- `onTap_`: `std::function<void()>`，点击回调
- `enabled_`: `bool`，默认 true

ButtonView 的默认 `refreshHint_` SHALL 为 `RefreshHint::Fast`。

#### Scenario: Primary 样式
- **WHEN** style\_ = Primary
- **THEN** 绘制黑底白字按钮（backgroundColor = Black，文字色 = White）

#### Scenario: Secondary 样式
- **WHEN** style\_ = Secondary
- **THEN** 绘制白底黑字黑边框按钮（backgroundColor = White，文字色 = Black，边框 = Black）

#### Scenario: Icon 样式
- **WHEN** style\_ = Icon 且 icon\_ 有效
- **THEN** 绘制透明底 + 图标，无文字、无边框

### Requirement: ButtonStyle 枚举
`ink::ButtonStyle` SHALL 定义以下值：
- `Primary`: 黑底白字（主按钮）
- `Secondary`: 白底黑字黑边框（次按钮）
- `Icon`: 透明底 + 图标（图标按钮）

#### Scenario: 枚举定义
- **WHEN** 使用 ButtonStyle::Primary、Secondary、Icon
- **THEN** 编译通过

### Requirement: ButtonView 点击响应
ButtonView SHALL 覆写 `onTouchEvent()`。当收到 Tap 事件且 enabled\_ 为 true 时，SHALL 调用 onTap\_ 回调并返回 true（消费事件）。

#### Scenario: 点击触发回调
- **WHEN** 收到 TouchType::Tap 事件，enabled\_ = true，onTap\_ 已设置
- **THEN** onTap\_() 被调用，返回 true

#### Scenario: 禁用状态不响应
- **WHEN** 收到 TouchType::Tap 事件，enabled\_ = false
- **THEN** 不调用 onTap\_，返回 false

#### Scenario: 无回调时不崩溃
- **WHEN** 收到 TouchType::Tap 事件，onTap\_ 未设置（空 function）
- **THEN** 不崩溃，返回 true（仍消费事件）

### Requirement: ButtonView 禁用状态外观
当 `enabled_` 为 false 时，ButtonView SHALL 以 Color::Medium 绘制文字和边框，视觉上表示禁用状态。

#### Scenario: 禁用外观
- **WHEN** enabled\_ = false，style\_ = Primary
- **THEN** 背景为 Medium 而非 Black，文字颜色为 Light

### Requirement: ButtonView setLabel 更新
`setLabel(const std::string&)` SHALL 更新 label\_ 并调用 `setNeedsDisplay()`。

#### Scenario: 更新文字
- **WHEN** 调用 setLabel("Cancel")
- **THEN** label\_ 更新，needsDisplay\_ 为 true

### Requirement: ButtonView intrinsicSize
`intrinsicSize()` SHALL 根据 style\_ 返回合适尺寸：
- Primary/Secondary: 文字宽度 + 水平 padding（左右各 16px），高度 48px
- Icon: 48×48（触摸区域最小尺寸）

font\_ 为 nullptr 时文字部分宽度按 0 计算。

#### Scenario: Primary intrinsicSize
- **WHEN** style\_ = Primary，文字宽 60px
- **THEN** intrinsicSize() 返回 {92, 48}（60 + 32 padding）

#### Scenario: Icon intrinsicSize
- **WHEN** style\_ = Icon
- **THEN** intrinsicSize() 返回 {48, 48}
