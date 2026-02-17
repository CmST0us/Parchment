## ADDED Requirements

### Requirement: Library Header 左侧菜单图标
LibraryViewController 的 HeaderView SHALL 在左侧显示汉堡菜单图标（UI_ICON_MENU），点击暂不响应。右侧 SHALL 保持设置图标（UI_ICON_SETTINGS）。

#### Scenario: Header 双图标
- **WHEN** Library 页面渲染
- **THEN** Header 左侧显示汉堡菜单图标，右侧显示设置齿轮图标，标题 "Parchment" 居中

### Requirement: Library Subheader 栏
LibraryViewController SHALL 在 Header 下方显示 36px 高的 Subheader 栏，浅灰背景（Color::Light 或自定义灰度）。左侧 SHALL 包含书本图标 + "共 N 本" 文字，右侧 SHALL 显示 "按名称排序" 文字（16px, Color::Dark）。Subheader 下方 SHALL 有 1px 分隔线。

#### Scenario: Subheader 内容
- **WHEN** Library 页面渲染，书库有 8 本书
- **THEN** Subheader 左侧显示书本图标 + "共 8 本"，右侧显示 "按名称排序"

### Requirement: 书籍封面缩略图
每个书籍列表条目 SHALL 在左侧显示 52×72px 的书籍封面缩略图。封面 SHALL 包含：
- 浅灰背景
- 1.5px 深色边框，圆角 3px（Canvas 能力范围内用直角近似）
- 居中显示格式标签 "TXT"（16px, Color::Dark, bold）
- 右上角折角装饰（用 fillRect 绘制约 10px 的白色三角区域）

封面右侧间距 SHALL 为 16px。

#### Scenario: 封面渲染
- **WHEN** 渲染一个 TXT 格式的书籍条目
- **THEN** 左侧显示 52×72px 封面，内含 "TXT" 标签和右上角折角装饰

### Requirement: 书籍信息区域布局
每个书籍列表条目的信息区域（封面右侧）SHALL 包含从上到下：
1. 书名: 去掉 .txt 后缀, 20px 字体, Color::Black, 单行截断
2. 文件大小: "X.X KB" 或 "X.X MB", 16px 字体, Color::Dark, 单行
3. 进度条行: ProgressBarView(6px 高) + 百分比文字 "N%"(16px, Color::Medium)

条目总高度 SHALL 为 96px，水平 padding 16px。

#### Scenario: 书籍条目完整渲染
- **WHEN** 渲染一本大小 2.5MB、阅读进度 42% 的书籍
- **THEN** 显示封面 + 书名 + "2.5 MB" + 进度条(42%) + "42%"

### Requirement: 书籍列表分隔线
相邻书籍条目之间 SHALL 用 1px SeparatorView 分隔。最后一个条目后不加分隔线。分隔线颜色 SHALL 与设计稿一致（Color::Light 或 border-color）。

#### Scenario: 条目间分隔
- **WHEN** 页面有 3 个书籍条目
- **THEN** 第 1 和第 2 条目之间有 1px 分隔线，第 2 和第 3 条目之间有 1px 分隔线，第 3 条目后无分隔线

### Requirement: 翻页指示器中文文案
PageIndicatorView 的上一页/下一页按钮 SHALL 使用中文文案。显示格式: "← 上一页" / "N / M" / "下一页 →"。

#### Scenario: 翻页文案
- **WHEN** 当前在第 1 页，共 2 页
- **THEN** 显示 "← 上一页"(禁用) · "1 / 2" · "下一页 →"(可用)

### Requirement: 空列表状态中文提示
当书库为空时 SHALL 在列表区域居中显示 "未找到书籍"（替代原来的 "No books found"）。

#### Scenario: 空书库
- **WHEN** book_store 扫描到 0 本书
- **THEN** 列表区域居中显示 "未找到书籍"，翻页指示器隐藏
