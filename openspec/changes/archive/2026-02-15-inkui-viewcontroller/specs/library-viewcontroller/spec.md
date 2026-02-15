## ADDED Requirements

### Requirement: LibraryViewController 页面结构
`LibraryViewController` SHALL 继承 `ink::ViewController`，构造时接收 `ink::Application&` 引用。title_ SHALL 设为 "Parchment"。View 树 SHALL 包含：
1. HeaderView 顶部栏：标题 "Parchment"，右侧设置图标按钮（暂不响应）
2. 副标题区域：显示 "N 本书" 书籍总数
3. 书籍列表区域：按当前页码显示 book_info_t 条目
4. PageIndicatorView 底部翻页指示器

#### Scenario: 页面初始布局
- **WHEN** LibraryViewController 首次 getView()
- **THEN** 显示 Header + 副标题 + 书籍列表（第 1 页）+ 底部翻页指示器

### Requirement: LibraryViewController 书籍列表
LibraryViewController SHALL 从 `book_store` 读取书籍列表，每页最多显示 9 个书籍条目。每个条目 SHALL 显示：
- 书名（文件名去掉 .txt 后缀），使用 TextLabel，BLACK
- 文件大小信息，使用 TextLabel，DARK
- 阅读进度条，使用 ProgressBarView（从 settings_store 加载进度）
- 底部分隔线

#### Scenario: 书籍列表有内容
- **WHEN** book_store 扫描到 12 本书
- **THEN** 第 1 页显示前 9 本，PageIndicator 显示 "1/2"

#### Scenario: 书籍列表为空
- **WHEN** book_store 扫描到 0 本书
- **THEN** 列表区域显示居中提示文字 "No books found"，PageIndicator 隐藏

### Requirement: LibraryViewController 翻页
LibraryViewController SHALL 通过 PageIndicatorView 的 onPageChange 回调响应翻页操作。翻页时 SHALL 清空当前列表内容区域的子 View，重新创建对应页码的书籍条目 View。

#### Scenario: 翻到第 2 页
- **WHEN** 用户点击 PageIndicator 的下一页按钮
- **THEN** 列表区域更新为第 10-12 本书的条目，PageIndicator 显示 "2/2"

#### Scenario: 左右滑动翻页
- **WHEN** 用户向左滑动（SwipeEvent direction=Left）
- **THEN** 等同于点击下一页

### Requirement: LibraryViewController 打开书籍
LibraryViewController SHALL 为每个书籍条目设置点击回调。点击书籍条目 SHALL 调用 `app.navigator().push()` 推入 `ReaderViewController`，传递被选中书籍的 `book_info_t` 信息。

#### Scenario: 点击打开书籍
- **WHEN** 用户点击 "三体.txt" 条目
- **THEN** navigator push ReaderViewController(app, bookInfo)，屏幕切换到阅读页面

### Requirement: LibraryViewController viewWillAppear 刷新
LibraryViewController SHALL 在 `viewWillAppear()` 中重新加载所有书籍的阅读进度并重建当前页列表，以反映从 ReaderViewController 返回后可能变化的进度。

#### Scenario: 从阅读返回后刷新
- **WHEN** 从 ReaderViewController pop 返回
- **THEN** viewWillAppear 触发，书籍列表中的进度条更新为最新阅读进度
