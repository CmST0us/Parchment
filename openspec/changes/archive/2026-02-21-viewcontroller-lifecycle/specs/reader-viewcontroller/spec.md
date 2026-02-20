## MODIFIED Requirements

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

### Requirement: ReaderViewController 页面布局
ReaderViewController SHALL 在 `loadView()` 中创建以下元素：
1. 顶部 HeaderView：返回按钮 + 书名（默认隐藏，通过 tap 中间区域切换）
2. ReaderTouchView 容器（处理三分区触摸），内嵌 ReaderContentView（文本渲染）
3. 底部页脚：书名 左侧，"currentPage/totalPages percent%" 右侧，小字体 DARK

页脚的页码和百分比 SHALL 从 `ReaderContentView` 的 `totalPages()` 和 `currentPage()` 获取。

#### Scenario: 阅读页面布局
- **WHEN** loadView 执行
- **THEN** View 树已创建（Header + TouchView/ContentView + Footer），等待 viewDidLoad 加载数据
