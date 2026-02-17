### Requirement: Boot 画面 Logo 图标
BootViewController SHALL 在画面上部居中显示一个 100×100px 的书本 Logo 图标。Logo 由 `BootLogoView` 自定义 View 绘制，使用 Canvas 直线段绘制简化的书本轮廓（打开的书本形状 + 中间书脊线 + 两侧书页线条）。Logo 下方与标题间距 SHALL 为 24px。

#### Scenario: Logo 显示
- **WHEN** BootViewController 首次渲染
- **THEN** 画面中上部显示 100×100px 书本图标，居中对齐，图标下方有 24px 间距

### Requirement: Boot 画面标题和副标题
BootViewController SHALL 显示主标题 "Parchment"（28px 字体, Color::Black, 居中）和中文副标题 "墨水屏阅读器"（20px 字体, Color::Dark, 居中）。副标题下方与状态区域间距 SHALL 为 48px。

#### Scenario: 标题副标题显示
- **WHEN** BootViewController 首次渲染
- **THEN** 标题 "Parchment" 以 28px 黑色居中显示，副标题 "墨水屏阅读器" 以 20px 深灰居中显示，位于 Logo 下方

### Requirement: Boot 画面进度条
BootViewController SHALL 在状态文字下方显示一个 200px 宽、3px 高的 ProgressBarView。进度条 track 颜色为 Color::Light，fill 颜色为 Color::Dark。进度条 SHALL 居中对齐。

#### Scenario: 进度条显示
- **WHEN** BootViewController 首次渲染
- **THEN** 画面底部居中显示 200×3px 进度条，初始填充为 0%

### Requirement: Boot 画面状态文字
BootViewController SHALL 在进度条上方显示状态文字（16px 字体, Color::Medium, 居中）。初始文字为 "正在初始化..."，SD 卡扫描完成后更新为 "发现 N 本书" 或 "SD 卡不可用"。

#### Scenario: 状态文字初始化
- **WHEN** viewDidLoad 开始执行
- **THEN** 状态文字显示 "正在初始化..."

#### Scenario: SD 卡扫描成功
- **WHEN** book_store_scan() 返回 ESP_OK
- **THEN** 状态文字更新为 "发现 N 本书"（N 为实际书籍数量）

#### Scenario: SD 卡不可用
- **WHEN** book_store_scan() 返回错误
- **THEN** 状态文字显示 "SD 卡不可用"

### Requirement: Boot 画面整体布局
Boot 画面 SHALL 在 540×960 全屏范围内采用 FlexBox Column 布局，垂直居中排列所有元素。从上到下: 弹性空间 → Logo(100px) → 间距(24px) → 标题(40px) → 副标题(28px) → 间距(48px) → 状态文字(24px) → 间距(16px) → 进度条(3px) → 弹性空间。背景色 SHALL 为 Color::White。

#### Scenario: 垂直居中布局
- **WHEN** BootViewController 渲染完成
- **THEN** Logo + 标题 + 副标题组整体垂直居中，状态文字和进度条位于下方
