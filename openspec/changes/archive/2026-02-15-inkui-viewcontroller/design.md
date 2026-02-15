## Context

InkUI 框架已完成核心基础设施：View 树、FlexLayout、RenderEngine、Canvas、GestureRecognizer，以及 7 个 widget 组件（TextLabel、ButtonView、IconView、HeaderView、ProgressBarView、SeparatorView、PageIndicatorView）。ViewController 和 NavigationController 基类已实现并经过验证。

当前 main.cpp 中有一个临时 `BootVC` 用于 widget 组件验证，需要替换为正式的应用 ViewController。数据层已就位：`book_store` 提供 SD 卡 TXT 文件扫描，`settings_store` 提供 NVS 阅读偏好和进度持久化。

约束：
- ESP32-S3 + 8MB PSRAM + 16MB Flash
- C++17，`-fno-exceptions -fno-rtti`
- 540x960 portrait 逻辑坐标，4bpp 灰度
- 墨水屏刷新特性：全刷 ~1s，局部刷 ~0.3s
- 字体系统：UI 字体 16/24px 常驻 PSRAM，阅读字体 24/32px 按需加载

## Goals / Non-Goals

**Goals:**
- 实现 BootViewController：启动画面，显示 Logo 和加载状态，自动过渡到书库
- 实现 LibraryViewController：书库页面，展示 SD 卡 TXT 文件列表，支持翻页和选择
- 实现 ReaderViewController：阅读页面，TXT 纯文本分页显示，左右翻页
- 建立页面间导航流：Boot → Library → Reader，Reader 可返回 Library
- 将 ViewController 文件组织到 `main/controllers/` 目录
- 重构 main.cpp 为干净的初始化入口

**Non-Goals:**
- EPUB 格式支持（仅 TXT）
- 阅读设置页面（SettingsViewController）
- 目录页面（TocViewController）
- 阅读工具栏（浮层 overlay）
- 书签功能
- 文本排版的高级功能（富文本、段落缩进检测等）
- 字体选择和切换

## Decisions

### 1. BootViewController 设计：延时定时器自动跳转

**选择**：使用 `Application::postDelayed()` 发送 TimerEvent，在 `handleEvent()` 中执行 `navigator().replace(LibraryVC)`。

**替代方案**：
- 在 `viewDidAppear()` 中直接用 `vTaskDelay()` 阻塞 → 会阻塞主事件循环，不可取
- 在 `viewDidAppear()` 中直接 replace → 用户看不到启动画面

**理由**：postDelayed 基于 esp_timer，不阻塞事件循环，且 2 秒延迟足以让用户看到启动画面。使用 `replace` 而非 `push`，因为 Boot 页面不需要保留在栈中。

### 2. BootViewController 获取 Application 引用

**选择**：ViewController 构造时传入 `Application&` 引用，存为成员变量。

**替代方案**：
- Application 单例模式 → 违反架构设计，Application 当前不是单例
- 通过事件系统间接通信 → 过度设计

**理由**：ViewController 需要调用 `app.navigator()` 进行页面导航和 `app.postDelayed()` 发送延迟事件。构造时注入 `Application&` 是最简单直接的方式，符合依赖注入原则。所有需要导航的 VC 都采用此模式。

### 3. LibraryViewController 列表：使用翻页而非滚动

**选择**：基于 PageIndicatorView 的翻页浏览，每页最多显示 9 本书（(960-48-36)/96≈9）。

**替代方案**：
- 实现滚动列表 → 墨水屏帧率低，滚动体验差
- 使用尚未实现的 PagedListView → 增加依赖，当前可手动实现等效逻辑

**理由**：符合架构设计文档的翻页决策。手动管理分页逻辑（currentPage × itemsPerPage 切片）简单直接，不需要 PagedListView 组件。

### 4. LibraryViewController 书籍项：手动布局每个 item

**选择**：在 `buildPage()` 中为当前页的每本书创建一组 View（书名 TextLabel + 作者 TextLabel + 进度 ProgressBarView + 分隔线），用 FlexBox Column 布局。翻页时清空重建。

**替代方案**：
- 自定义 BookItemView 复合组件 → 增加类层次复杂度，目前只有一处使用
- 虚拟列表复用 View → 过度优化，最多 64 本书 × 9 项/页

**理由**：最简单实现方式。每次翻页 clearSubviews + 重建 ~9 个 item 的 View 树，开销极小（微秒级）。如果以后需要复用，可以重构为 BookItemView。

### 5. ReaderViewController 文本分页：按字节偏移分页

**选择**：加载 TXT 全文到内存，按可见区域高度逐行排版计算每页的字节范围。使用 `Canvas::measureText()` 测量文本宽度进行手动折行。

**替代方案**：
- 依赖尚未实现的 TextLayout/TextContentView → 增加阻塞依赖
- 流式读取不加载全文 → 无法精确计算总页数和快速翻页

**理由**：TXT 文件通常 < 5MB，PSRAM 有余量加载全文。先实现简单的全量加载 + 预计算分页，后续可优化为流式分页。分页信息（每页起始字节偏移数组）缓存后翻页只需 O(1) 查找。

### 6. ReaderViewController 触摸区域：三分区 tap

**选择**：在 `handleEvent(SwipeEvent)` 处理左右滑动翻页，在根 View 的 `onTouchEvent` 中根据 tap 的 x 坐标判断：左 1/3 上一页、右 1/3 下一页、中 1/3 暂不处理（未来工具栏）。

**理由**：符合线框规格的交互设计。不需要额外 View 层，直接在 VC 的根 View 子类上处理 tap 事件即可。

### 7. 文件组织：controllers 目录在 main/ 下

**选择**：`main/controllers/` 目录，每个 VC 一对 .h/.cpp 文件。

**理由**：符合架构文档 Section 6 的目录结构设计。VC 是应用层代码，属于 main 组件而非 ink_ui 库。

## Risks / Trade-offs

- **[全量加载大文件]** → TXT 文件超过 5MB 会占用大量 PSRAM。缓解：设置文件大小上限（如 8MB），超出时提示用户。当前 PSRAM 预算有 ~0.5MB 余量加上阅读字体不使用时可释放 ~5MB。
- **[翻页重建 View 树]** → 每次翻页 clearSubviews + 重建有轻微延迟。缓解：View 创建是微秒级操作，远低于 EPD 刷新时间（~300ms）。
- **[阅读进度持久化频率]** → 每次翻页都写 NVS 会增加 Flash 磨损。缓解：仅在页面 disappear（退出阅读）时保存一次进度，而非每次翻页。
- **[UTF-8 折行]** → 中文 UTF-8 每字符 3 字节，英文 1 字节，混合文本的折行需逐字符测量。缓解：使用 Canvas::measureText 逐词/逐字测量，性能可接受。
- **[阅读字体按需加载]** → 进入 ReaderVC 时需加载阅读字体（~5MB），耗时约 1-2 秒。缓解：在 viewDidLoad 中异步加载字体，显示加载提示。
