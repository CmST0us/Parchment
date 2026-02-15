## ADDED Requirements

### Requirement: ReaderViewController 构造
`ReaderViewController` SHALL 继承 `ink::ViewController`，构造时接收 `ink::Application&` 引用和 `const book_info_t&` 书籍信息。title_ SHALL 设为书名（去掉 .txt 后缀）。

#### Scenario: 构造
- **WHEN** 创建 `ReaderViewController(app, bookInfo)`
- **THEN** title_ 为书名，持有文件路径和 Application 引用

### Requirement: ReaderViewController 文件加载
ReaderViewController SHALL 在 `viewDidLoad()` 中从 SD 卡读取 TXT 文件全部内容到内存（PSRAM），并从 `settings_store` 加载该书的阅读进度（byte_offset）。

#### Scenario: 文件加载成功
- **WHEN** viewDidLoad 执行，文件 /sdcard/三体.txt 存在
- **THEN** 文件内容加载到 PSRAM buffer，阅读进度恢复到上次位置

#### Scenario: 文件加载失败
- **WHEN** viewDidLoad 执行，文件不存在或读取错误
- **THEN** 显示错误提示 "Failed to load file"，不崩溃

### Requirement: ReaderViewController 文本分页
ReaderViewController SHALL 对加载的文本进行预分页计算。分页算法 SHALL：
1. 根据当前字号和行距计算每行可容纳的字符宽度（使用 Canvas::measureText）
2. 对文本逐行折行（UTF-8 字符级别），计算每行实际占用的高度
3. 将连续行组合为页，直到填满可用内容区域高度（960 - 顶部预留 - 底部页脚）
4. 记录每页的起始字节偏移到分页表 `std::vector<uint32_t>`

#### Scenario: 分页计算
- **WHEN** 文件加载完成
- **THEN** 生成 pages_ 数组，pages_[i] 为第 i 页的起始字节偏移，totalPages 为总页数

### Requirement: ReaderViewController 页面布局
ReaderViewController SHALL 显示以下元素：
1. 顶部章节/文件名标示：小字体 DARK，左对齐
2. 文本内容区域：根据阅读偏好设置的字号和行距渲染当前页文本
3. 底部页脚：书名 左侧，"currentPage/totalPages percent%" 右侧，小字体 DARK

#### Scenario: 阅读页面布局
- **WHEN** 文本加载并分页完成
- **THEN** 屏幕显示顶部文件名、正文内容、底部页码信息

### Requirement: ReaderViewController 翻页
ReaderViewController SHALL 支持以下翻页操作：
- 点击屏幕右侧 1/3 区域（x > 360）→ 下一页
- 点击屏幕左侧 1/3 区域（x < 180）→ 上一页
- SwipeEvent direction=Left → 下一页
- SwipeEvent direction=Right → 上一页

翻页时 SHALL 更新当前页码，重绘文本内容区域和页脚。

#### Scenario: 点击右侧翻到下一页
- **WHEN** 当前第 5 页（共 100 页），用户 tap x=400
- **THEN** 切换到第 6 页，内容更新，页脚显示 "6/100 6%"

#### Scenario: 向左滑动翻页
- **WHEN** 当前第 5 页，SwipeEvent direction=Left
- **THEN** 切换到第 6 页

#### Scenario: 最后一页不翻页
- **WHEN** 当前已是最后一页，用户尝试下一页
- **THEN** 不执行翻页操作

#### Scenario: 第一页不翻页
- **WHEN** 当前第 1 页，用户尝试上一页
- **THEN** 不执行翻页操作

### Requirement: ReaderViewController 返回书库
ReaderViewController SHALL 在 HeaderView 的左侧返回按钮上设置回调，点击时调用 `app.navigator().pop()` 返回书库页面。

#### Scenario: 点击返回
- **WHEN** 用户点击左上角返回按钮
- **THEN** navigator.pop() 执行，ReaderVC 被销毁，LibraryVC 重新可见

### Requirement: ReaderViewController 进度保存
ReaderViewController SHALL 在 `viewWillDisappear()` 中将当前阅读进度（byte_offset、current_page、total_pages）通过 `settings_store_save_progress()` 持久化到 NVS。

#### Scenario: 退出时保存进度
- **WHEN** 用户从阅读页返回书库（pop）
- **THEN** viewWillDisappear 调用 settings_store_save_progress，当前 byte_offset 和页码被保存

#### Scenario: 重新打开恢复进度
- **WHEN** 用户再次打开同一本书
- **THEN** viewDidLoad 加载进度，自动定位到上次阅读的页码

### Requirement: ReaderViewController 残影管理
ReaderViewController SHALL 维护一个翻页计数器。每翻 N 页（从 `reading_prefs_t.full_refresh_pages` 读取，默认 5）SHALL 将该次翻页的刷新模式设为 RefreshHint::Full（GC16 全刷）以消除残影，其余翻页使用 RefreshHint::Quality（GL16）。

#### Scenario: 每 5 页全刷一次
- **WHEN** 连续翻页 5 次
- **THEN** 第 5 次翻页使用 GC16 全刷，之后计数器归零
