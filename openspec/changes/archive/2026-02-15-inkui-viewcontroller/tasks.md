## 1. 项目结构和构建配置

- [x] 1.1 创建 `main/controllers/` 目录
- [x] 1.2 更新 `main/CMakeLists.txt`，将 `controllers/` 下的 .cpp 文件添加到 SRCS 列表

## 2. BootViewController 实现

- [x] 2.1 创建 `main/controllers/BootViewController.h` — 类声明，构造函数接收 `ink::Application&`，声明 viewDidLoad/viewDidAppear/handleEvent override
- [x] 2.2 创建 `main/controllers/BootViewController.cpp` — viewDidLoad 实现：创建全屏根 View、居中标题 "Parchment"、副标题 "E-Ink Reader"、状态文字 "Initializing..."，并调用 book_store_scan() 更新状态文字
- [x] 2.3 实现 viewDidAppear：通过 app.postDelayed() 发送 2 秒延迟的 TimerEvent
- [x] 2.4 实现 handleEvent：收到 TimerEvent 时调用 app.navigator().replace(LibraryViewController)

## 3. LibraryViewController 实现

- [x] 3.1 创建 `main/controllers/LibraryViewController.h` — 类声明，构造函数接收 `ink::Application&`，声明生命周期回调和 private 成员（currentPage、itemsPerPage、listContainer 指针等）
- [x] 3.2 创建 `main/controllers/LibraryViewController.cpp` — viewDidLoad 实现：创建 HeaderView + 副标题 + 列表容器 + PageIndicatorView 的 FlexBox Column 布局
- [x] 3.3 实现 buildPage() 方法：根据 currentPage 切片书籍列表，为每本书创建条目 View（书名 TextLabel + 文件大小 TextLabel + ProgressBarView + SeparatorView），设置点击回调
- [x] 3.4 实现书籍条目点击回调：创建 ReaderViewController 并 push 到 navigator
- [x] 3.5 实现 PageIndicator onPageChange 回调和 SwipeEvent 翻页处理
- [x] 3.6 实现 viewWillAppear：重新调用 book_store_scan()，重新加载进度，重建当前页列表
- [x] 3.7 处理空列表状态：无书籍时显示 "No books found" 居中提示

## 4. ReaderViewController 实现

- [x] 4.1 创建 `main/controllers/ReaderViewController.h` — 类声明，构造函数接收 `ink::Application&` 和 `const book_info_t&`，声明 private 成员（文本 buffer、分页表、当前页码等）
- [x] 4.2 创建 `main/controllers/ReaderViewController.cpp` — viewDidLoad 实现：读取 TXT 文件到 PSRAM buffer，从 settings_store 加载阅读进度
- [x] 4.3 实现文本分页算法：逐字符 UTF-8 折行，按行填充页面，生成 pages_ 偏移表
- [x] 4.4 实现页面 View 布局：顶部文件名标示 + 文本内容区域 + 底部页脚（书名 + 页码/总页 百分比）
- [x] 4.5 实现 renderPage() 方法：根据 currentPage 从 pages_ 取出文本范围，更新 TextLabel 内容和页脚信息
- [x] 4.6 实现翻页交互：根 View 的 onTouchEvent 按 x 坐标三分区判断翻页方向，handleEvent 处理 SwipeEvent 翻页
- [x] 4.7 实现返回按钮：HeaderView 左侧图标设置 onTap 回调调用 navigator().pop()
- [x] 4.8 实现 viewWillDisappear：调用 settings_store_save_progress() 保存当前 byte_offset 和页码
- [x] 4.9 实现残影管理：翻页计数器，每 N 页（从 reading_prefs_t.full_refresh_pages）设置 RefreshHint::Full

## 5. main.cpp 重构

- [x] 5.1 移除 main.cpp 中的临时 BootVC 类定义
- [x] 5.2 添加 `#include "controllers/BootViewController.h"`，修改 push 为使用正式 BootViewController
- [x] 5.3 验证初始化序列不变：NVS → Board → Touch → SD → Font → InkUI → push(BootVC) → run()

## 6. 构建验证

- [x] 6.1 执行 `idf.py build` 确认编译无错误无警告
- [ ] 6.2 验证导航流程：Boot 启动 → 2 秒后自动到 Library → 点击书籍到 Reader → 返回 Library
