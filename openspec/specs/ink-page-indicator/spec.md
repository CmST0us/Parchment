## ADDED Requirements

### Requirement: PageIndicatorView 翻页指示器
`ink::PageIndicatorView` SHALL 继承 `ink::View`，显示翻页导航 UI。

组合内容:
- 上一页按钮（prevButton\_，ButtonView，Icon 样式，显示 "‹" 或左箭头图标）
- 页码标签（pageLabel\_，TextLabel，居中，显示 "2/5" 格式）
- 下一页按钮（nextButton\_，ButtonView，Icon 样式，显示 "›" 或右箭头图标）

属性:
- `currentPage_`: `int`，当前页码（0-based），默认 0
- `totalPages_`: `int`，总页数，默认 1
- `onPageChange_`: `std::function<void(int)>`，页码变化回调

PageIndicatorView 的默认 `refreshHint_` SHALL 为 `RefreshHint::Fast`。

#### Scenario: 正常显示
- **WHEN** currentPage\_ = 1，totalPages\_ = 5
- **THEN** 显示 "< 2/5 >"（显示给用户的页码为 1-based）

### Requirement: PageIndicatorView 翻页行为
点击 prevButton\_ SHALL 将 currentPage\_ 减 1（不低于 0），更新显示并调用 onPageChange\_ 回调。
点击 nextButton\_ SHALL 将 currentPage\_ 加 1（不超过 totalPages\_-1），更新显示并调用 onPageChange\_ 回调。

#### Scenario: 下一页
- **WHEN** currentPage\_ = 1，totalPages\_ = 5，点击 nextButton\_
- **THEN** currentPage\_ 变为 2，pageLabel\_ 显示 "3/5"，onPageChange\_(2) 被调用

#### Scenario: 首页不能上翻
- **WHEN** currentPage\_ = 0，点击 prevButton\_
- **THEN** currentPage\_ 保持 0，prevButton\_ 处于禁用状态，不调用回调

#### Scenario: 末页不能下翻
- **WHEN** currentPage\_ = totalPages\_-1，点击 nextButton\_
- **THEN** currentPage\_ 不变，nextButton\_ 处于禁用状态，不调用回调

### Requirement: PageIndicatorView 按钮禁用状态
prevButton\_ SHALL 在 currentPage\_ == 0 时设置 enabled\_ = false。
nextButton\_ SHALL 在 currentPage\_ == totalPages\_-1 时设置 enabled\_ = false。

#### Scenario: 首页禁用上翻
- **WHEN** currentPage\_ = 0
- **THEN** prevButton\_.enabled\_ = false，nextButton\_.enabled\_ = true

#### Scenario: 末页禁用下翻
- **WHEN** currentPage\_ = totalPages\_-1
- **THEN** prevButton\_.enabled\_ = true，nextButton\_.enabled\_ = false

### Requirement: PageIndicatorView setPage 更新
`setPage(int page, int total)` SHALL 更新 currentPage\_ 和 totalPages\_，刷新页码标签和按钮状态。

#### Scenario: 外部更新页码
- **WHEN** 调用 setPage(3, 10)
- **THEN** currentPage\_ = 3，totalPages\_ = 10，pageLabel\_ 显示 "4/10"

### Requirement: PageIndicatorView 内部布局
PageIndicatorView SHALL 使用 FlexBox Row 布局:
- prevButton\_: 固定 48×48
- pageLabel\_: flexGrow=1，居中对齐
- nextButton\_: 固定 48×48

#### Scenario: 布局分配
- **WHEN** frame 宽 540px
- **THEN** prevButton 48px，pageLabel 444px（居中），nextButton 48px

### Requirement: PageIndicatorView intrinsicSize
`intrinsicSize()` SHALL 返回 `{-1, 48}`。

#### Scenario: 固有尺寸
- **WHEN** 调用 intrinsicSize()
- **THEN** 返回 {-1, 48}
