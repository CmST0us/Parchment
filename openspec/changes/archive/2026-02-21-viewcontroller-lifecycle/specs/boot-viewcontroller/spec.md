## MODIFIED Requirements

### Requirement: BootViewController 启动画面
`BootViewController` SHALL 继承 `ink::ViewController`，在 `loadView()` 中创建启动画面 View 树。画面 SHALL 在 540x960 全屏范围内居中显示：
1. BootLogoView 书本图标（100×100px, 居中）
2. 主标题 "Parchment"（28px 字体, Color::Black, 居中）
3. 中文副标题 "墨水屏阅读器"（20px 字体, Color::Dark, 居中）
4. 状态文字（16px 字体, Color::Medium, 居中, 初始 "正在初始化..."）
5. ProgressBarView 进度条（200×3px, 居中）

#### Scenario: 启动画面布局
- **WHEN** BootViewController 被 push 并首次调用 view()
- **THEN** loadView 创建 View 树，从上到下包含: 弹性空间 → Logo(100px) → 间距(24px) → 标题 "Parchment"(28px, BLACK) → 副标题 "墨水屏阅读器"(20px, DARK) → 弹性空间 → 状态文字(16px, MEDIUM) → 间距(16px) → 进度条(200×3px) → 弹性空间(底部)

### Requirement: BootViewController 初始化状态
BootViewController SHALL 在 `viewDidLoad()` 中执行 `book_store_scan()` 扫描 SD 卡书籍。扫描完成后 SHALL 更新状态文字为 "发现 N 本书" 或在 SD 卡不可用时显示 "SD 卡不可用"。

#### Scenario: SD 卡扫描成功
- **WHEN** viewDidLoad 执行，SD 卡已挂载
- **THEN** book_store_scan() 返回 ESP_OK，状态文字更新为 "发现 N 本书"

#### Scenario: SD 卡不可用
- **WHEN** viewDidLoad 执行，SD 卡未挂载
- **THEN** book_store_scan() 返回错误，状态文字显示 "SD 卡不可用"，仍自动跳转到书库（书库显示空列表）
