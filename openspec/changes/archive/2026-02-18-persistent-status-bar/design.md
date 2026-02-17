## Context

当前 `ink::Application` 的渲染循环直接渲染当前 ViewController 的 root view。每个 VC 创建 540×960 的全屏 View 树，StatusBarView 作为 `LibraryViewController` 的子 View 存在。页面切换时，整个 View 树被替换，状态栏随之消失。

关键代码路径：
- `Application::run()` → `navigator_.current()->getView()` → `renderEngine_->renderCycle(rootView)`
- `NavigationController::push()` 仅管理 VC 生命周期，不涉及 View 树的挂载

## Goals / Non-Goals

**Goals:**
- 状态栏在所有页面切换中保持持久显示
- ViewController 无需关心状态栏的创建和管理
- BootViewController 等特殊页面可以隐藏状态栏
- 最小化对现有 ViewController 代码的改动
- 保持触摸事件分发的正确性（状态栏区域和内容区域）

**Non-Goals:**
- 不实现通用的多层叠 overlay/window 系统
- 不改变 RenderEngine 的渲染流程（仍然接收单个 root view）
- 不在状态栏添加新功能（电池、Wi-Fi 等），仅保持当前时间显示
- 不改变 ViewController 的生命周期回调机制

## Decisions

### Decision 1: Window 逻辑内嵌在 Application 中，不创建独立 Window 类

**选择**: 在 `Application` 内部创建一个 root View 作为 Window 容器，而非引入新的 `Window` 类。

**理由**: 当前项目只有一个屏幕、一个 Application 实例。独立的 Window 类增加了抽象层但没有实际收益。将 Window 逻辑直接放在 Application 中更简单直接。

**替代方案**: 创建 `ink::Window` 类持有 root view + status bar + content area。优点是关注点分离更清晰，缺点是增加一层间接引用，且当前只有单 window 场景，过度设计。

### Decision 2: Application 持有固定的 Window View 树结构

```
windowRoot_ (540×960, Column)
├── statusBar_ (540×20, StatusBarView)
└── contentArea_ (540×940, View)
    └── [VC 的 root view 在这里挂载/卸载]
```

`windowRoot_` 在 `Application::init()` 中创建，生命期与 Application 一致。`contentArea_` 是一个普通的 `ink::View`，VC 的 root view 作为 `contentArea_` 的唯一子 View 动态挂载。

**理由**: 固定的 View 树结构简单可靠，避免每次页面切换重建状态栏。状态栏独立于 VC 生命周期。

### Decision 3: NavigationController 通过回调通知 Application 挂载 VC View

NavigationController 新增 `setOnViewControllerChange` 回调。当 push/pop/replace 导致当前 VC 变化时，触发回调。Application 在回调中：
1. 清除 `contentArea_` 旧子 View
2. 将新 VC 的 root view 挂载为 `contentArea_` 的子 View
3. 根据新 VC 的 `prefersStatusBarHidden()` 显示/隐藏状态栏

**理由**: 回调机制保持了 NavigationController 的解耦，不需要 NavigationController 知道 Window 结构。

**替代方案**: 让 Application 每帧检查当前 VC 是否变化（轮询）。缺点是浪费 CPU 且时机不精确。

### Decision 4: VC View 不再自行设置 frame，由 contentArea 布局

VC 在 `viewDidLoad()` 中创建的 root view 不再需要 `setFrame({0, 0, 540, 960})`。改为由 `contentArea_` 的 FlexBox 布局自动分配尺寸。VC root view 设 `flexGrow_ = 1` 即可填满内容区域。

**理由**: 避免 VC 硬编码屏幕尺寸，当状态栏隐藏/显示时内容区域自动调整。

### Decision 5: ViewController 新增 `prefersStatusBarHidden()` 虚方法

```cpp
virtual bool prefersStatusBarHidden() const { return false; }
```

默认返回 false（显示状态栏）。BootViewController 覆写返回 true。Application 在挂载 VC view 时检查此属性。

隐藏状态栏时 `statusBar_->setHidden(true)`，contentArea 自动扩展到 960px。

### Decision 6: StatusBarView 由 Application 创建和管理

Application::init() 中创建 StatusBarView，设置字体，定期（或页面切换时）调用 `updateTime()`。从 LibraryViewController 中移除状态栏相关代码。

时间更新策略：在每次渲染循环（30s 超时唤醒时）检查分钟是否变化，变化则更新。

## Risks / Trade-offs

- **VC root view 的 frame 变更** → 所有现有 VC 需要从 `setFrame({0, 0, 540, 960})` 改为不设 frame（或设为 flexGrow=1）。影响 BootVC, LibraryVC, ReaderVC 三个文件，改动量可控。

- **View 树挂载时机** → VC 的 root view 在 `viewDidLoad()` 时创建，但挂载到 contentArea 需要在 NavigationController 回调中执行。需确保 `getView()` 在回调触发前已经调用，当前代码流程已满足（NavigationController::push 先调用 `getView()` 再触发回调）。

- **contentArea 作为中间层增加 View 树深度** → 触摸事件 hitTest 多一层遍历，性能影响可忽略（View 树深度本身很浅）。

- **StatusBarView 触摸事件** → 状态栏区域的触摸会被 hitTest 命中到 StatusBarView。当前状态栏不处理触摸事件（onTouchEvent 返回 false），事件会冒泡到 windowRoot 然后丢弃。不影响 contentArea 内的正常触摸。
