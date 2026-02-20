## MODIFIED Requirements

### Requirement: LibraryViewController 页面结构
`LibraryViewController` SHALL 继承 `ink::ViewController`，构造时接收 `ink::Application&` 引用。title_ SHALL 设为 "Parchment"。`loadView()` 中创建 View 树 SHALL 包含：
1. HeaderView 顶部栏（48px）：左侧汉堡菜单图标（UI_ICON_MENU, 暂不响应）+ 标题 "Parchment" + 右侧设置图标（UI_ICON_SETTINGS, 暂不响应）
2. Subheader 区域（36px）：浅灰背景，左侧书本图标 + "共 N 本"，右侧 "按名称排序" 文字
3. 1px 分隔线
4. 书籍列表区域：按当前页码显示 book_info_t 条目
5. 1px 分隔线
6. PageIndicatorView 底部翻页指示器（48px），中文文案

`viewDidLoad()` 中 SHALL 调用 `updatePageInfo()` 和 `buildPage()` 加载初始数据。

#### Scenario: 页面初始布局
- **WHEN** LibraryViewController 首次 view()
- **THEN** loadView 创建 View 树结构，viewDidLoad 加载初始数据，显示 Header(含左右图标) + Subheader(书本图标+"共 N 本"+排序) + 分隔线 + 书籍列表(第 1 页) + 分隔线 + 翻页指示器
