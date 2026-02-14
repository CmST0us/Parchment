## Why

InkUI 框架已完成 Phase 0-1（基础设施 + 核心渲染），`View`、`Canvas`、`Event` 类型均已实现并通过验证。但当前框架缺少交互系统——没有手势识别、页面生命周期管理、事件分发循环和布局算法实现。没有这些，View 树只能静态渲染，无法响应触摸、管理页面栈或自动排列子 View。本阶段实现交互系统，使 InkUI 成为可运行的完整框架。

## What Changes

- 实现 `FlexLayout` 布局算法：基于已定义的 FlexStyle 类型，计算子 View 的 frame
- 实现 `RenderEngine`：5 阶段渲染循环（Layout → 收集脏区域 → 重绘 → 多区域刷新 → 残影管理）
- 实现 `GestureRecognizer`：从 GT911 原始触摸数据识别 Tap / LongPress / Swipe 手势
- 实现 `ViewController`：页面生命周期（viewDidLoad / viewWillAppear / viewDidDisappear 等）
- 实现 `NavigationController`：页面栈管理（push / pop / replace），驱动 ViewController 生命周期
- 实现 `Application`：主事件循环（FreeRTOS 事件队列 + renderCycle），整合所有子系统
- 更新 `main.cpp`：移除旧 ui_core 调用，使用 Application + NavigationController 启动

## Capabilities

### New Capabilities
- `ink-flex-layout`: FlexBox 布局算法实现（Column/Row 方向、flexGrow 弹性分配、gap 间距、padding 内边距、alignItems/justifyContent 对齐）
- `ink-render-engine`: 墨水屏渲染引擎（脏区域收集、局部重绘、多区域独立刷新、刷新模式决策、残影计数自动 GC16）
- `ink-gesture-recognizer`: 触摸手势识别状态机（Tap / LongPress / Swipe 检测，从 GT911 I2C 数据到 Event 转换）
- `ink-view-controller`: 页面生命周期管理（viewDidLoad / viewWillAppear / viewDidAppear / viewWillDisappear / viewDidDisappear / handleEvent）
- `ink-navigation-controller`: 页面栈导航控制器（push / pop / replace，ViewController 生命周期驱动，栈深度限制）
- `ink-application`: 应用主循环（FreeRTOS 事件队列、事件分发、触摸 hitTest、renderCycle 调度、postDelayed 延迟事件）

### Modified Capabilities
- `ink-view`: View 基类新增 `onLayout()` 默认实现调用 FlexLayout 算法（当前 `onLayout()` 为空虚函数）

## Impact

- **新增文件**: `components/ink_ui/src/core/` 下 6 个 .cpp 和对应 .h 文件
- **修改文件**: `View.cpp`（onLayout 默认实现）、`CMakeLists.txt`（注册新源文件）、`main.cpp`（切换到 Application 入口）
- **依赖**: 使用 FreeRTOS 队列、esp_timer；桥接 GT911 C 驱动和 epdiy C 驱动
- **旧框架**: main.cpp 不再调用 ui_core，但 ui_core component 暂不删除（Phase 5 再清理）
