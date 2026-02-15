## ADDED Requirements

### Requirement: BootViewController 启动画面
`BootViewController` SHALL 继承 `ink::ViewController`，在 `viewDidLoad()` 中创建启动画面 View 树。画面 SHALL 在 540x960 全屏范围内居中显示应用名称 "Parchment" 和副标题 "E-Ink Reader"，以及底部状态文字 "Initializing..."。

#### Scenario: 启动画面布局
- **WHEN** BootViewController 被 push 并首次调用 getView()
- **THEN** viewDidLoad 创建 View 树，包含居中的 "Parchment" 标题（大号字体、BLACK）、"E-Ink Reader" 副标题（中号字体、DARK）和底部 "Initializing..." 状态文字（小号字体、MEDIUM）

### Requirement: BootViewController 构造函数
`BootViewController` SHALL 通过构造函数接收 `ink::Application&` 引用，用于导航和延迟事件。title_ SHALL 设为 "Boot"。

#### Scenario: 构造
- **WHEN** 创建 `BootViewController(app)`
- **THEN** title_ 为 "Boot"，持有 app 引用

### Requirement: BootViewController 自动跳转
`BootViewController` SHALL 在 `viewDidAppear()` 中通过 `Application::postDelayed()` 发送一个 2 秒延迟的 TimerEvent。在 `handleEvent()` 中接收到该 TimerEvent 时，SHALL 调用 `app.navigator().replace()` 将自身替换为 `LibraryViewController`。

#### Scenario: 2 秒后自动跳转到书库
- **WHEN** BootViewController 已可见，2 秒延迟到期
- **THEN** TimerEvent 到达，handleEvent 中执行 navigator().replace(LibraryVC)，画面切换到书库页面

#### Scenario: Boot 页面不保留在栈中
- **WHEN** 跳转到 LibraryViewController 完成后
- **THEN** BootViewController 已被 replace 销毁，navigator.depth() 保持为 1

### Requirement: BootViewController 初始化状态
BootViewController SHALL 在 `viewDidLoad()` 中执行 `book_store_scan()` 扫描 SD 卡书籍。扫描完成后 SHALL 更新状态文字为 "Found N books" 或在 SD 卡不可用时显示 "SD card not available"。

#### Scenario: SD 卡扫描成功
- **WHEN** viewDidLoad 执行，SD 卡已挂载
- **THEN** book_store_scan() 返回 ESP_OK，状态文字更新为 "Found N books"

#### Scenario: SD 卡不可用
- **WHEN** viewDidLoad 执行，SD 卡未挂载
- **THEN** book_store_scan() 返回错误，状态文字显示 "SD card not available"，仍自动跳转到书库（书库显示空列表）
