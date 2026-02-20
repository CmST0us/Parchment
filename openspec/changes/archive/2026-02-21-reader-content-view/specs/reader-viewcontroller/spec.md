## MODIFIED Requirements

### Requirement: ReaderViewController 文本分页
ReaderViewController SHALL 将文本分页职责委托给 `ReaderContentView`。ViewController 不再维护内部分页表和折行逻辑。

viewDidLoad 中 SHALL：
1. 创建 `ReaderContentView` 实例并配置字体、行距、段间距、文字颜色
2. 调用 `setTextBuffer()` 设置加载后的文本缓冲区
3. 分页由 `ReaderContentView` 在首次 `onDraw` 时自动执行

#### Scenario: 分页委托
- **WHEN** viewDidLoad 执行，文件加载成功
- **THEN** ReaderContentView 被创建并配置，文本缓冲区已设置，分页将在首次渲染时自动完成

### Requirement: ReaderViewController 页面布局
ReaderViewController SHALL 显示以下元素：
1. 顶部 HeaderView：返回按钮 + 书名
2. 顶部文件名标示：小字体 DARK，左对齐
3. ReaderTouchView 容器（处理三分区触摸），内嵌 ReaderContentView（文本渲染）
4. 底部页脚：书名 左侧，"currentPage/totalPages percent%" 右侧，小字体 DARK

页脚的页码和百分比 SHALL 从 `ReaderContentView` 的 `totalPages()` 和 `currentPage()` 获取。

#### Scenario: 阅读页面布局
- **WHEN** 文本加载完成
- **THEN** 屏幕显示 HeaderView、文件名标示、文本内容（ReaderContentView 渲染）、底部页码信息

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

### Requirement: ReaderViewController 进度保存
ReaderViewController SHALL 在 `viewWillDisappear()` 中从 `ReaderContentView` 获取当前进度信息：
- `currentPageOffset()` → `progress.byte_offset`
- `currentPage()` → `progress.current_page`
- `totalPages()` → `progress.total_pages`

并通过 `settings_store_save_progress()` 持久化。

viewDidLoad 中恢复进度时 SHALL 调用 `ReaderContentView::pageForByteOffset()` 将保存的字节偏移转换为页码，再调用 `setCurrentPage()`。

注意：由于懒分页，进度恢复需在分页完成后执行。ReaderViewController SHALL 将目标页码传递给 ReaderContentView，由其在分页完成后自动应用。

#### Scenario: 退出时保存进度
- **WHEN** 用户从阅读页返回书库
- **THEN** 从 ReaderContentView 获取 currentPageOffset 和页码，保存到 NVS

#### Scenario: 重新打开恢复进度
- **WHEN** 用户再次打开同一本书，NVS 中有保存的 byte_offset
- **THEN** ReaderContentView 在分页完成后定位到对应页码
