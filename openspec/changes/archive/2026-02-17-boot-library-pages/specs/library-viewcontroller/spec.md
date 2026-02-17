## MODIFIED Requirements

### Requirement: LibraryViewController 页面结构
`LibraryViewController` SHALL 继承 `ink::ViewController`，构造时接收 `ink::Application&` 引用。title_ SHALL 设为 "Parchment"。View 树 SHALL 包含：
1. HeaderView 顶部栏（48px）：左侧汉堡菜单图标（UI_ICON_MENU, 暂不响应）+ 标题 "Parchment" + 右侧设置图标（UI_ICON_SETTINGS, 暂不响应）
2. Subheader 区域（36px）：浅灰背景，左侧书本图标 + "共 N 本"，右侧 "按名称排序" 文字
3. 1px 分隔线
4. 书籍列表区域：按当前页码显示 book_info_t 条目
5. 1px 分隔线
6. PageIndicatorView 底部翻页指示器（48px），中文文案

#### Scenario: 页面初始布局
- **WHEN** LibraryViewController 首次 getView()
- **THEN** 显示 Header(含左右图标) + Subheader(书本图标+"共 N 本"+排序) + 分隔线 + 书籍列表(第 1 页) + 分隔线 + 翻页指示器

### Requirement: LibraryViewController 书籍列表
LibraryViewController SHALL 从 `book_store` 读取书籍列表，每页最多显示 9 个书籍条目。每个条目（96px 高）SHALL 显示：
- 左侧: BookCoverView 封面缩略图（52×72px，含 "TXT" 格式标签和右上角折角）
- 右侧信息区:
  - 书名（文件名去掉 .txt 后缀），20px 字体，Color::Black，单行截断
  - 文件大小信息（"X.X KB" 或 "X.X MB"），16px 字体，Color::Dark
  - 阅读进度条（ProgressBarView, 6px 高）+ 百分比文字（16px, Color::Medium）
- 条目之间 1px 分隔线

#### Scenario: 书籍列表有内容
- **WHEN** book_store 扫描到 12 本书
- **THEN** 第 1 页显示前 9 本（含封面+信息），PageIndicator 显示 "1 / 2"

#### Scenario: 书籍列表为空
- **WHEN** book_store 扫描到 0 本书
- **THEN** 列表区域显示居中提示文字 "未找到书籍"，PageIndicator 隐藏
