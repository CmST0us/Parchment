## ADDED Requirements

### Requirement: ReaderViewController 构造
`ReaderViewController` SHALL 继承 `ink::ViewController`，构造时接收 `ink::Application&` 引用和 `const book_info_t&` 书籍信息。title_ SHALL 设为书名（去掉 .txt 后缀）。

#### Scenario: 构造
- **WHEN** 创建 `ReaderViewController(app, bookInfo)`
- **THEN** title_ 为书名，持有文件路径和 Application 引用

### Requirement: ReaderViewController 文件加载
ReaderViewController SHALL 将 view 树创建和文件加载分离：

`loadView()` 中 SHALL 创建页面 View 树：
1. 顶部 HeaderView：返回按钮 + 书名（默认隐藏）
2. ReaderTouchView 容器（三分区触摸），内嵌 ReaderContentView
3. 底部页脚：书名左侧，页码/百分比右侧

`viewDidLoad()` 中 SHALL 执行数据加载：
1. 从 SD 卡读取 TXT 文件全部内容到 PSRAM
2. 检测文件编码并转换为 UTF-8
3. 调用 `contentView_->setTextBuffer()` 设置文本缓冲区
4. 从 `settings_store` 加载阅读进度并调用 `setInitialByteOffset()`

#### Scenario: 文件加载成功（UTF-8）
- **WHEN** loadView 创建 View 树后 viewDidLoad 执行，文件为 UTF-8 编码
- **THEN** 文件内容加载到 PSRAM buffer，设置到 ReaderContentView，阅读进度恢复

#### Scenario: 文件加载失败
- **WHEN** viewDidLoad 执行，文件不存在或读取错误
- **THEN** 显示错误提示 "Failed to load file"，不崩溃

### Requirement: ReaderViewController 文本分页
ReaderViewController SHALL 将文本分页职责委托给 `ReaderContentView`。ViewController 不再维护内部分页表和折行逻辑。

viewDidLoad 中 SHALL：
1. 调用 `setTextBuffer()` 设置加载后的文本缓冲区
2. 分页由 `ReaderContentView` 在首次 `onDraw` 时自动执行

`ReaderContentView` 实例在 `loadView()` 中创建并配置字体、行距、段间距、文字颜色。

#### Scenario: 分页委托
- **WHEN** viewDidLoad 执行，文件加载成功
- **THEN** ReaderContentView 已在 loadView 中创建并配置，文本缓冲区已设置，分页将在首次渲染时自动完成

### Requirement: ReaderViewController 页面布局
ReaderViewController SHALL 在 `loadView()` 中创建以下元素：
1. 顶部 HeaderView：返回按钮 + 书名（默认隐藏，通过 tap 中间区域切换）
2. ReaderTouchView 容器（处理三分区触摸），内嵌 ReaderContentView（文本渲染）
3. 底部页脚：书名 左侧，"currentPage/totalPages percent%" 右侧，小字体 DARK

页脚的页码和百分比 SHALL 从 `ReaderContentView` 的 `totalPages()` 和 `currentPage()` 获取。

#### Scenario: 阅读页面布局
- **WHEN** loadView 执行
- **THEN** View 树已创建（Header + TouchView/ContentView + Footer），等待 viewDidLoad 加载数据

### Requirement: ReaderViewController 翻页
ReaderViewController SHALL 支持以下翻页操作：
- 点击屏幕右侧 1/3 区域 → 下一页
- 点击屏幕左侧 1/3 区域 → 上一页
- SwipeEvent direction=Left → 下一页
- SwipeEvent direction=Right → 上一页

翻页时 SHALL 调用 `ReaderContentView::setCurrentPage()` 更新页码，并更新底部页脚文本。

#### Scenario: 点击右侧翻到下一页
- **WHEN** 当前第 5 页（共 100 页），用户 tap x=400
- **THEN** 调用 `contentView->setCurrentPage(6)`，页脚更新为 "6/100 6%"

#### Scenario: 最后一页不翻页
- **WHEN** 当前已是最后一页，用户尝试下一页
- **THEN** 不执行翻页操作（currentPage + 1 >= totalPages 时跳过）

### Requirement: ReaderViewController 返回书库
ReaderViewController SHALL 在 HeaderView 的左侧返回按钮上设置回调，点击时调用 `app.navigator().pop()` 返回书库页面。

#### Scenario: 点击返回
- **WHEN** 用户点击左上角返回按钮
- **THEN** navigator.pop() 执行，ReaderVC 被销毁，LibraryVC 重新可见

### Requirement: ReaderViewController 进度保存
ReaderViewController SHALL 在 `viewWillDisappear()` 中从 `ReaderContentView` 获取当前进度信息：
- `currentPageOffset()` → `progress.byte_offset`
- `currentPage()` → `progress.current_page`
- `totalPages()` → `progress.total_pages`

并通过 `settings_store_save_progress()` 持久化。

viewDidLoad 中恢复进度时 SHALL 调用 `ReaderContentView::setInitialByteOffset()` 将保存的字节偏移传递给 ReaderContentView，由其在分页完成后自动应用。

#### Scenario: 退出时保存进度
- **WHEN** 用户从阅读页返回书库
- **THEN** 从 ReaderContentView 获取 currentPageOffset 和页码，保存到 NVS

#### Scenario: 重新打开恢复进度
- **WHEN** 用户再次打开同一本书，NVS 中有保存的 byte_offset
- **THEN** ReaderContentView 在分页完成后定位到对应页码

### Requirement: ReaderViewController 残影管理
ReaderViewController SHALL 维护一个翻页计数器。每翻 N 页（从 `reading_prefs_t.full_refresh_pages` 读取，默认 5）SHALL 将该次翻页的刷新模式设为 RefreshHint::Full（GC16 全刷）以消除残影，其余翻页使用 RefreshHint::Quality（GL16）。

#### Scenario: 每 5 页全刷一次
- **WHEN** 连续翻页 5 次
- **THEN** 第 5 次翻页使用 GC16 全刷，之后计数器归零
