## 1. 图标资源准备

- [x] 1.1 下载 `book-2.svg` (Tabler Icons) 到 `tools/icons/src/`
- [x] 1.2 运行 `convert_icons.py` 重新生成 `ui_icon_data.h`，包含新增的 book-2 图标
- [x] 1.3 在 `ui_icon.h` 中添加 `UI_ICON_BOOK` 常量声明，在 `ui_icon.c` 中添加对应定义

## 2. Boot 画面自定义 View

- [x] 2.1 创建 `main/views/BootLogoView.h/cpp` — 继承 `ink::View`，100×100px，`onDraw()` 中用 Canvas API 绘制简化书本图标（书本轮廓 + 中间书脊线 + 两侧书页线条），`intrinsicSize()` 返回 `{100, 100}`

## 3. BootViewController 重写

- [x] 3.1 重写 `BootViewController::viewDidLoad()` 视图树：弹性空间 → BootLogoView(100px) → 间距(24px) → 标题 "Parchment"(28px, Black, Center) → 副标题 "墨水屏阅读器"(20px, Dark, Center) → 弹性空间 → 状态文字 "正在初始化..."(16px, Medium, Center) → 间距(16px) → ProgressBarView(200×3px, 居中) → 弹性空间
- [x] 3.2 将状态文字更新为中文："发现 N 本书" / "SD 卡不可用"

## 4. Library 书籍封面 View

- [x] 4.1 创建 `main/views/BookCoverView.h/cpp` — 继承 `ink::View`，固定 52×72px，`onDraw()` 绘制：浅灰背景 + 深色边框(1px) + 居中 "TXT" 格式标签(16px) + 右上角折角装饰(白色三角)，`intrinsicSize()` 返回 `{52, 72}`

## 5. LibraryViewController 重写

- [x] 5.1 修改 `viewDidLoad()` HeaderView：添加左侧 `UI_ICON_MENU` 图标（空回调）
- [x] 5.2 新增 Subheader 区域（36px 高，浅灰背景）：左侧 IconView(book-2 图标) + TextLabel "共 N 本"，右侧 TextLabel "按名称排序"
- [x] 5.3 重写 `buildPage()` 书籍条目布局：改为 Row 布局 — 左侧 BookCoverView(52×72) + 右侧 Column(书名 20px + 文件大小 16px + 进度条行 Row(ProgressBarView + 百分比 TextLabel))
- [x] 5.4 将空列表提示改为中文 "未找到书籍"
- [x] 5.5 更新 `updatePageInfo()` 副标题文案为 "共 N 本"

## 6. 构建验证

- [x] 6.1 执行 `idf.py build` 确保编译通过无错误
