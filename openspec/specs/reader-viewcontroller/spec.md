## ADDED Requirements

### Requirement: ReaderViewController 构造
`ReaderViewController` SHALL 继承 `ink::ViewController`，构造时接收 `ink::Application&` 引用和 `const book_info_t&` 书籍信息。title_ SHALL 设为书名（去掉 .txt 后缀）。

#### Scenario: 构造
- **WHEN** 创建 `ReaderViewController(app, bookInfo)`
- **THEN** title_ 为书名，持有文件路径和 Application 引用

### Requirement: ReaderViewController 文件加载
ReaderViewController SHALL 在 `viewDidLoad()` 中从 SD 卡读取 TXT 文件全部内容到内存（PSRAM），**检测文件编码并转换为 UTF-8**，并从 `settings_store` 加载该书的阅读进度（byte_offset）。

加载流程 SHALL 按以下顺序执行：
1. 以二进制模式读取文件全部内容到 PSRAM 缓冲区
2. 调用 `text_encoding_detect()` 检测编码
3. 若为 `TEXT_ENCODING_GBK`：分配新 PSRAM 缓冲区（容量为原始大小的 1.5 倍 + 1），调用 `text_encoding_gbk_to_utf8()` 转码，释放原缓冲区，替换为新缓冲区
4. 若为 `TEXT_ENCODING_UTF8_BOM`：跳过前 3 字节（memmove），更新缓冲区大小
5. 若为 `TEXT_ENCODING_UTF8`：不做额外处理
6. 空终止缓冲区

#### Scenario: 文件加载成功（UTF-8）
- **WHEN** viewDidLoad 执行，文件 /sdcard/book/三体.txt 存在且为 UTF-8 编码
- **THEN** 文件内容加载到 PSRAM buffer，无需转码，阅读进度恢复到上次位置

#### Scenario: 文件加载成功（GBK 自动转码）
- **WHEN** viewDidLoad 执行，文件为 GBK 编码的中文 TXT
- **THEN** 检测到 GBK 编码，自动转码为 UTF-8 存入 PSRAM buffer，后续分页和渲染正常显示中文

#### Scenario: 文件加载成功（UTF-8 BOM 剥离）
- **WHEN** viewDidLoad 执行，文件以 `EF BB BF` 开头
- **THEN** BOM 被剥离，textBuffer_ 从实际文本内容开始，textSize_ 减少 3

#### Scenario: 文件加载失败
- **WHEN** viewDidLoad 执行，文件不存在或读取错误
- **THEN** 显示错误提示 "Failed to load file"，不崩溃

#### Scenario: GBK 转码内存分配失败
- **WHEN** GBK 文件较大，PSRAM 无法分配转码缓冲区
- **THEN** 释放原缓冲区，loadFile 返回 false，显示错误提示

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
