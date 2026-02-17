## Why

状态栏（StatusBarView）当前作为 `LibraryViewController` 的子 View 创建，导致页面切换后状态栏消失。进入 `ReaderViewController` 后状态栏不再显示，退出阅读后也无法恢复。状态栏应该像 iOS 的系统状态栏一样，始终显示在所有页面之上。

## What Changes

- **BREAKING**: `Application` 引入 `Window` 概念 — 一个全屏容器 View，包含两个层级：顶部的持久化状态栏区域和下方的 ViewController 内容区域
- `Application::run()` 的 renderCycle 从渲染当前 VC 的 root view 改为渲染 Window root view
- ViewController 的 root view 不再占满全屏 (540×960)，而是占据状态栏下方的内容区域 (540×940)
- `StatusBarView` 的创建和管理从 `LibraryViewController` 移到 `Application` 层级
- 触摸事件 hitTest 改为从 Window root view 开始，自动覆盖状态栏和 VC 内容区域
- ViewController 可声明是否需要状态栏（如 `Boot` 页面可隐藏状态栏）

## Capabilities

### New Capabilities

- `window-root`: Window 根容器概念 — Application 持有一个全屏 Window View，内部分为状态栏区域和内容区域，NavigationController 切换 VC 时只替换内容区域，状态栏持久存在

### Modified Capabilities

- `ink-application`: Application 的 run loop 和 touch dispatch 需从 VC root view 改为 Window root view
- `ink-navigation-controller`: ViewController 切换时需要将新 VC 的 root view 挂载到 Window 的内容区域，而非独立渲染

## Impact

- **核心框架变更**: `Application.h/cpp`, `NavigationController.h/cpp`
- **新增**: `Window` 类（或在 Application 内部实现 Window 逻辑）
- **ViewController 适配**: 所有现有 VC（Boot, Library, Reader）的 root view frame 从 540×960 变为 540×940（或由 Window 自动约束）
- **StatusBarView 迁移**: 从 `LibraryViewController` 移除状态栏创建代码，改由 Application 管理
- **BootViewController**: 需要支持隐藏状态栏（启动画面不需要状态栏）
- **构建**: 无新依赖，纯框架层重构
