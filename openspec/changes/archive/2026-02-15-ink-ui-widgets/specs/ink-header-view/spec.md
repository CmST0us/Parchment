## ADDED Requirements

### Requirement: HeaderView 页面标题栏
`ink::HeaderView` SHALL 继承 `ink::View`，提供页面顶部标题栏组件。

内部组合:
- 可选左侧图标按钮（leftButton\_）
- 标题文字（titleLabel\_，TextLabel，居中）
- 可选右侧图标按钮（rightButton\_）

HeaderView 使用 FlexBox Row 布局，内部子 View 自动排列。默认高度 48px，背景色 Color::Black，文字色 Color::White。

HeaderView 的默认 `refreshHint_` SHALL 为 `RefreshHint::Quality`。

#### Scenario: 带左右图标的完整标题栏
- **WHEN** 设置 title="书库"，leftIcon 和 rightIcon 均有效
- **THEN** 绘制黑底白字标题栏：左侧图标 | 中间标题居中 | 右侧图标

#### Scenario: 只有标题
- **WHEN** 设置 title="Parchment"，无左右图标
- **THEN** 绘制黑底白字标题栏，标题居中显示

### Requirement: HeaderView 构造与配置
HeaderView SHALL 提供以下配置方法:
- `setTitle(const std::string&)`: 设置标题文字
- `setLeftIcon(const uint8_t* iconData, std::function<void()> onTap)`: 设置左侧图标按钮
- `setRightIcon(const uint8_t* iconData, std::function<void()> onTap)`: 设置右侧图标按钮
- `setFont(const EpdFont* font)`: 设置标题字体

调用 setTitle 或 setLeftIcon/setRightIcon SHALL 触发 `setNeedsDisplay()` 和 `setNeedsLayout()`。

#### Scenario: 设置标题触发更新
- **WHEN** 调用 setTitle("设置")
- **THEN** titleLabel\_ 文字更新，needsDisplay\_ 和 needsLayout\_ 为 true

#### Scenario: 设置左侧图标
- **WHEN** 调用 setLeftIcon(backIcon, onBack)
- **THEN** 左侧创建 ButtonView(Icon 样式)，点击时调用 onBack

### Requirement: HeaderView 内部布局
HeaderView SHALL 覆写 `onLayout()`，使用 FlexBox Row 布局:
- leftButton\_: 固定 48×48
- titleLabel\_: flexGrow=1，水平居中
- rightButton\_: 固定 48×48

未设置的按钮 SHALL 不占用空间（不添加到 subviews）。

#### Scenario: 左右按钮都存在
- **WHEN** leftIcon 和 rightIcon 均已设置，frame 宽 540px
- **THEN** leftButton 占 48px，titleLabel 占 444px，rightButton 占 48px

#### Scenario: 只有左按钮
- **WHEN** 只设置了 leftIcon，frame 宽 540px
- **THEN** leftButton 占 48px，titleLabel 占 492px

### Requirement: HeaderView 图标 tint
HeaderView 内部的 IconView SHALL 使用 Color::White 作为 tintColor（因为背景是黑色）。

#### Scenario: 图标白色渲染
- **WHEN** HeaderView 背景为 Black
- **THEN** leftIcon 和 rightIcon 以 White 颜色渲染

### Requirement: HeaderView intrinsicSize
`intrinsicSize()` SHALL 返回 `{-1, 48}`（宽度由父 View 决定，高度固定 48px）。

#### Scenario: 固有尺寸
- **WHEN** 调用 intrinsicSize()
- **THEN** 返回 {-1, 48}
