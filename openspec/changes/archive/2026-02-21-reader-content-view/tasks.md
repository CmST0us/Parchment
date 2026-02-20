## 1. ReaderContentView 核心实现

- [x] 1.1 创建 `main/views/ReaderContentView.h` — 类定义、配置 API、分页 API、私有成员声明
- [x] 1.2 实现 `layoutPage()` 统一布局引擎 — UTF-8 遍历、glyph 宽度测量、折行、按像素高度填充、段间距处理、`\r\n` 支持
- [x] 1.3 实现 `paginate()` — 循环调用 `layoutPage()` 构建页表 `pages_`
- [x] 1.4 实现 `onDraw()` — 懒分页检查、调用 `layoutPage()` 获取行布局、逐行 `canvas.drawTextN()` 绘制
- [x] 1.5 实现配置方法 — `setTextBuffer`/`setFont`/`setLineSpacing`/`setParagraphSpacing`/`setTextColor`，变更时清空 `pages_`
- [x] 1.6 实现分页查询 API — `totalPages`/`currentPage`/`setCurrentPage`/`currentPageOffset`/`pageForByteOffset`

## 2. ReaderViewController 重构

- [x] 2.1 替换 `contentLabel_`（TextLabel）为 `ReaderContentView`，配置字体、行距、段间距、颜色
- [x] 2.2 删除 ViewController 内部的 `paginate()`、`renderPage()` 方法及相关成员（`pages_`、`contentAreaWidth_`、`contentAreaHeight_`、`lineHeight_`）
- [x] 2.3 重构 `nextPage()`/`prevPage()` — 调用 `contentView->setCurrentPage()`，更新页脚文本
- [x] 2.4 重构进度保存/恢复 — `viewWillDisappear` 从 ReaderContentView 获取进度，`viewDidLoad` 将目标页码传递给 ReaderContentView
- [x] 2.5 清理头文件 — 删除 ReaderViewController 中不再需要的私有方法声明和成员变量
