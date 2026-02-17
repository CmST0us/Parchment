## Why

BootViewController 和 LibraryViewController 已有基本功能，但视觉效果与 `design/index.html` 的设计稿差距较大：Boot 缺少 Logo 图标、中文副标题、进度条；Library 缺少书籍封面缩略图、作者信息、子标题栏图标和排序按钮等。需要像素级还原设计稿，使实际设备上的显示效果与 web prototype 一致。

## What Changes

- **BootViewController**: 重写 `viewDidLoad()` 视图树，增加居中 Logo 图标（书本图标 bitmap）、"Parchment" 主标题（大号衬线字体）、"墨水屏阅读器" 中文副标题、状态文本、进度条，布局间距精确匹配设计稿
- **LibraryViewController**: 重写 `viewDidLoad()` 和 `buildPage()` 视图树：
  - Header 左侧增加汉堡菜单图标按钮
  - 子标题栏增加书本图标 + 排序按钮（文字+下箭头）
  - 书籍列表项改为：左侧封面缩略图（52×72px，含格式标签 "TXT"）+ 右侧信息区（标题用衬线字体、作者名、进度条+百分比）
  - 底部翻页指示器文案改为中文 "← 上一页 · 1/1 · 下一页 →"
- **新增图标资源**: 汉堡菜单、书本、排序箭头、设置齿轮等 Tabler Icons 32×32 4bpp bitmap
- **新增 BookCoverView 组件**: 52×72px 书籍封面缩略图视图，包含格式标签和折角装饰

## Capabilities

### New Capabilities
- `boot-page-ui`: 启动画面的完整视觉实现，包含 Logo、标题、副标题、状态文本、进度条的精确布局
- `library-page-ui`: 书库页面的完整视觉实现，包含 Header、子标题栏、书籍列表项（封面+信息）、翻页指示器的精确布局

### Modified Capabilities
- `boot-viewcontroller`: viewDidLoad() 视图树结构变更，增加 Logo 和进度条
- `library-viewcontroller`: viewDidLoad() 和 buildPage() 视图树结构变更，增加封面和作者信息

## Impact

- **修改文件**: `main/controllers/BootViewController.cpp`, `main/controllers/LibraryViewController.cpp` 及对应 `.h`
- **新增文件**: 图标 bitmap 头文件（icons/）、BookCoverView 组件
- **依赖**: 现有 InkUI widget 组件（TextLabel, HeaderView, ProgressBarView, IconView, ButtonView, PageIndicatorView, SeparatorView）
- **字体**: 需要确认阅读字体（衬线）是否已加载到 PSRAM，用于书名显示
