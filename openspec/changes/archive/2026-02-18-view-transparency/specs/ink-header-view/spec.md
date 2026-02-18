## MODIFIED Requirements

### Requirement: HeaderView 页面标题栏
`ink::HeaderView` SHALL 继承 `ink::View`，提供页面顶部标题栏组件。

内部组合:
- 可选左侧图标按钮（leftButton\_）
- 标题文字（titleLabel\_，TextLabel，居中）
- 可选右侧图标按钮（rightButton\_）

HeaderView 使用 FlexBox Row 布局，内部子 View 自动排列。默认高度 48px。HeaderView 及其内部子 View（titleLabel\_、leftButton\_、rightButton\_）SHALL 使用默认透明背景（Color::Clear），不显式设置 backgroundColor。背景色由父 View 透传。

HeaderView 的默认 `refreshHint_` SHALL 为 `RefreshHint::Quality`。

#### Scenario: 带左右图标的完整标题栏
- **WHEN** 设置 title="书库"，leftIcon 和 rightIcon 均有效
- **THEN** 绘制标题栏：左侧图标 | 中间标题居中 | 右侧图标，背景色继承自父 View

#### Scenario: 只有标题
- **WHEN** 设置 title="Parchment"，无左右图标
- **THEN** 绘制标题栏，标题居中显示，背景色继承自父 View

### Requirement: HeaderView 构造与配置
HeaderView SHALL 提供以下配置方法:
- `setTitle(const std::string&)`: 设置标题文字
- `setLeftIcon(const uint8_t* iconData, std::function<void()> onTap)`: 设置左侧图标按钮
- `setRightIcon(const uint8_t* iconData, std::function<void()> onTap)`: 设置右侧图标按钮
- `setFont(const EpdFont* font)`: 设置标题字体

调用 setTitle 或 setLeftIcon/setRightIcon SHALL 触发 `setNeedsDisplay()` 和 `setNeedsLayout()`。

`rebuild()` 方法重建子 View 时 SHALL 不对内部子 View 设置 backgroundColor，让它们保持默认透明。

#### Scenario: 设置标题触发更新
- **WHEN** 调用 setTitle("设置")
- **THEN** titleLabel\_ 文字更新，needsDisplay\_ 和 needsLayout\_ 为 true

#### Scenario: 设置左侧图标
- **WHEN** 调用 setLeftIcon(backIcon, onBack)
- **THEN** 左侧创建 ButtonView(Icon 样式)，点击时调用 onBack
