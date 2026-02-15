## Why

InkUI 框架的核心基础设施（View 树、FlexLayout、RenderEngine、Canvas、GestureRecognizer、Widget 组件）已全部就位，ViewController 和 NavigationController 基类也已实现。但当前 main.cpp 中只有一个临时的 `BootVC` 测试页面（验证 widget 组件），没有真正的应用页面。需要实现第一批正式 ViewController，完成从框架到应用的跨越，让设备能实际启动、浏览书库并开始阅读。

## What Changes

- 新增 `BootViewController` — 启动画面，显示 Logo 和加载状态，自动过渡到书库
- 新增 `LibraryViewController` — 书库页面，扫描 SD 卡 TXT 文件列表，支持翻页浏览和选择打开
- 新增 `ReaderViewController` — 阅读页面，加载 TXT 文件内容，分页显示文本，支持翻页
- 将 ViewController 从 main.cpp 移到独立文件 `main/controllers/` 目录下
- 重构 main.cpp 为干净的初始化 + BootViewController push 入口
- 实现页面间导航流: Boot → Library → Reader，支持 Reader 返回 Library

## Capabilities

### New Capabilities
- `boot-viewcontroller`: 启动画面控制器，显示应用名称和加载状态，初始化完成后自动跳转到书库
- `library-viewcontroller`: 书库控制器，扫描 SD 卡书籍文件，分页列表展示，点击打开阅读
- `reader-viewcontroller`: 阅读控制器，加载 TXT 文件分页显示，左右翻页，返回书库
- `app-navigation-flow`: 应用导航流程定义，Boot → Library → Reader 页面切换逻辑

### Modified Capabilities
（无需修改现有 spec 的需求）

## Impact

- **新增文件**: `main/controllers/` 下 6 个文件（3 个 VC 各含 .h/.cpp）
- **修改文件**: `main/main.cpp`（清理临时 BootVC，改为 include 正式 BootViewController）、`main/CMakeLists.txt`（添加 controllers 源文件）
- **依赖**: sd_storage（文件扫描）、ui_font（字体加载）、settings_store（阅读进度持久化）
- **现有 ink_ui 组件无需修改** — 所有 VC 基于现有 View/Widget API 构建
