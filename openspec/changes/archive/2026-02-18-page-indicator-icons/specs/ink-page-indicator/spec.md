## MODIFIED Requirements

### Requirement: PageIndicatorView 翻页指示器
`ink::PageIndicatorView` SHALL 继承 `ink::View`，显示翻页导航 UI。

组合内容:
- 上一页按钮（prevButton\_，ButtonView，Icon 样式，使用 `UI_ICON_ARROW_LEFT` 图标数据）
- 页码标签（pageLabel\_，TextLabel，居中，显示 "2/5" 格式）
- 下一页按钮（nextButton\_，ButtonView，Icon 样式，使用 `UI_ICON_ARROW_RIGHT` 图标数据）

属性:
- `currentPage_`: `int`，当前页码（0-based），默认 0
- `totalPages_`: `int`，总页数，默认 1
- `onPageChange_`: `std::function<void(int)>`，页码变化回调

PageIndicatorView 的默认 `refreshHint_` SHALL 为 `RefreshHint::Fast`。

#### Scenario: 正常显示
- **WHEN** currentPage\_ = 1，totalPages\_ = 5
- **THEN** 显示左箭头图标、"2/5" 文字、右箭头图标

#### Scenario: 按钮使用图标样式
- **WHEN** PageIndicatorView 被创建
- **THEN** prevButton\_ 和 nextButton\_ SHALL 使用 `ButtonStyle::Icon`，分别设置 `UI_ICON_ARROW_LEFT.data` 和 `UI_ICON_ARROW_RIGHT.data` 图标
