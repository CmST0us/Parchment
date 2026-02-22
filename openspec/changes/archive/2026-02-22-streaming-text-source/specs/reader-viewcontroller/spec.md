## MODIFIED Requirements

### Requirement: ReaderViewController 文件加载
ReaderViewController SHALL 将 view 树创建和文件加载分离：

`loadView()` 中 SHALL 创建页面 View 树：
1. 顶部 HeaderView：返回按钮 + 书名（默认隐藏）
2. ReaderTouchView 容器（三分区触摸），内嵌 ReaderContentView
3. 底部页脚：书名左侧，页码/状态信息右侧

`viewDidLoad()` 中 SHALL 执行数据加载：
1. 计算缓存目录路径：`"/sdcard/.cache/" + md5(book_.path)`
2. 创建缓存目录（如不存在）
3. 调用 `textSource_.open(book_.path, cacheDirPath)` 打开文件
4. 调用 `contentView_->setTextSource(&textSource_)` 设置文本源
5. 从 `settings_store` 加载阅读进度并调用 `setInitialByteOffset()`
6. 设置状态回调以更新页脚显示

ReaderViewController SHALL 持有 `TextSource` 成员变量（值语义，owned）。不再持有 `textBuffer_` 裸指针。

#### Scenario: 文件加载成功（UTF-8）
- **WHEN** viewDidLoad 执行，文件为 UTF-8 编码
- **THEN** TextSource 打开成功，状态为 Ready，ReaderContentView 获得 TextSource 指针

#### Scenario: 文件加载成功（GBK）
- **WHEN** viewDidLoad 执行，文件为 GBK 编码
- **THEN** TextSource 打开成功，状态为 Converting，首页立即可显示，后台转换进行中

#### Scenario: 文件加载失败
- **WHEN** viewDidLoad 执行，文件不存在或读取错误
- **THEN** TextSource 状态为 Error，显示错误提示，不崩溃

### Requirement: ReaderViewController 文本分页
ReaderViewController SHALL 将文本分页职责完全委托给 `ReaderContentView`。ViewController 不维护分页表、不启动分页 task。

viewDidLoad 中 SHALL：
1. 调用 `setTextSource()` 设置文本源
2. 分页由 ReaderContentView 在首次 `onDraw` 时自动处理（加载缓存或启动后台 task）

#### Scenario: 分页委托
- **WHEN** viewDidLoad 执行，文件加载成功
- **THEN** ReaderContentView 已获得 TextSource 指针，分页将在首次渲染时自动处理

### Requirement: ReaderViewController 页脚状态显示
ReaderViewController SHALL 根据加载状态动态更新底部页脚右侧文本：

- TextSource 状态为 `Converting` 时：显示 "正在转换编码... XX%"
- PageIndex 构建中时：显示 "正在索引... XX%"
- PageIndex 完成后：显示 "currentPage/totalPages percent%"

状态更新 SHALL 通过 ReaderContentView 的 statusCallback 触发。回调中 SHALL 使用 `setNeedsDisplay()` 或主线程安全机制更新页脚 TextLabel。

#### Scenario: GBK 转换中的页脚
- **WHEN** TextSource 正在后台转换 GBK，进度 45%
- **THEN** 页脚右侧显示 "正在转换编码... 45%"

#### Scenario: 页索引构建中的页脚
- **WHEN** TextSource 已 Ready，PageIndex 构建中，进度 60%
- **THEN** 页脚右侧显示 "正在索引... 60%"

#### Scenario: 全部就绪的页脚
- **WHEN** PageIndex 构建完成，当前第 42 页共 1423 页
- **THEN** 页脚右侧显示 "42/1423 3%"

### Requirement: ReaderViewController 进度保存
ReaderViewController SHALL 在 `viewWillDisappear()` 中从 `ReaderContentView` 获取当前进度信息：
- `currentPageOffset()` → `progress.byte_offset`
- `currentPage()` → `progress.current_page`
- `totalPages()` → `progress.total_pages`（未完成时保存 0）

并通过 `settings_store_save_progress()` 持久化。

`progress.byte_offset` SHALL 对应 TextSource 的 UTF-8 偏移量。对于 GBK 文件（使用缓存的 text.utf8），该偏移量在再次打开时仍然有效。

#### Scenario: 退出时保存进度
- **WHEN** 用户从阅读页返回书库
- **THEN** 从 ReaderContentView 获取 currentPageOffset 和页码，保存到 NVS

#### Scenario: 索引未完成时保存进度
- **WHEN** 用户在页索引构建完成前退出
- **THEN** 保存 byte_offset 和 current_page，total_pages 保存为 0
