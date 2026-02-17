## ADDED Requirements

### Requirement: Window View 树结构
Application SHALL 在 `init()` 中创建一个固定的 Window View 树结构：
- `windowRoot_`: 全屏根 View (540×960)，FlexBox Column 方向
- `statusBar_`: StatusBarView 子 View (540×20)，作为 windowRoot_ 的第一个子 View
- `contentArea_`: 普通 View 子 View，flexGrow=1 填满剩余空间 (540×940)

Window View 树的生命期 SHALL 与 Application 一致，不随 ViewController 切换而销毁。

#### Scenario: Application 初始化后 Window 树就绪
- **WHEN** 调用 `app.init()` 成功
- **THEN** `windowRoot_` 已创建，包含 statusBar_ 和 contentArea_ 两个子 View

#### Scenario: VC 切换不影响 Window 树
- **WHEN** NavigationController push 新 VC
- **THEN** windowRoot_ 和 statusBar_ 保持不变，仅 contentArea_ 的子 View 更新

### Requirement: StatusBar 持久显示
StatusBarView SHALL 在所有 ViewController 切换中保持存在，不随页面栈操作而销毁或重建。Application SHALL 负责 StatusBarView 的创建、字体设置和时间更新。

#### Scenario: 进入阅读页面后状态栏仍显示
- **WHEN** 从 Library 页面 push ReaderViewController
- **THEN** 状态栏仍然显示在屏幕顶部 20px 区域

#### Scenario: 退出阅读后状态栏仍显示
- **WHEN** 从 Reader 页面 pop 返回 Library
- **THEN** 状态栏仍然显示在屏幕顶部 20px 区域

### Requirement: StatusBar 可隐藏
ViewController SHALL 可通过 `prefersStatusBarHidden()` 虚方法声明是否需要隐藏状态栏。默认返回 false（显示）。

当状态栏隐藏时，contentArea_ SHALL 扩展到全屏高度 (960px)。当状态栏显示时，contentArea_ 高度为 940px。

#### Scenario: Boot 页面隐藏状态栏
- **WHEN** BootViewController 的 `prefersStatusBarHidden()` 返回 true
- **THEN** 状态栏 hidden，contentArea_ 占满 960px

#### Scenario: Library 页面显示状态栏
- **WHEN** LibraryViewController 的 `prefersStatusBarHidden()` 返回 false
- **THEN** 状态栏可见 (20px)，contentArea_ 占 940px

### Requirement: VC View 挂载到 contentArea
当 NavigationController 当前 VC 变化时，Application SHALL：
1. 清除 contentArea_ 现有的子 View（卸载旧 VC 的 View 树）
2. 将新 VC 的 root view 添加为 contentArea_ 的子 View
3. 新 VC 的 root view SHALL 设置 `flexGrow_ = 1` 以填满 contentArea_

VC 的 root view SHALL 不再直接设置 540×960 的 frame，而是由 contentArea_ 的 FlexBox 布局自动约束。

#### Scenario: push 新 VC 时 View 挂载
- **WHEN** 调用 `navigator().push(vcB)`，当前 VC 从 vcA 变为 vcB
- **THEN** vcA 的 root view 从 contentArea_ 卸载，vcB 的 root view 挂载到 contentArea_

#### Scenario: pop 时 View 重新挂载
- **WHEN** 调用 `navigator().pop()`，当前 VC 从 vcB 变回 vcA
- **THEN** vcB 的 root view 从 contentArea_ 卸载，vcA 的 root view 重新挂载到 contentArea_

### Requirement: StatusBar 时间更新
Application SHALL 在主事件循环中定期检查时间变化。当分钟数发生变化时，调用 StatusBarView 的 `updateTime()` 方法更新显示。

#### Scenario: 时间变化时更新
- **WHEN** 系统时间从 14:05 变为 14:06
- **THEN** 状态栏显示更新为 "14:06"

### Requirement: 渲染目标变更
Application 的 renderCycle SHALL 以 `windowRoot_` 为根执行渲染，而非当前 VC 的 root view。

#### Scenario: renderCycle 渲染整个 Window 树
- **WHEN** 主循环执行 renderCycle
- **THEN** `renderEngine_->renderCycle(windowRoot_.get())` 被调用，覆盖状态栏和内容区域

### Requirement: 触摸事件从 Window 根开始 hitTest
触摸事件的 hitTest SHALL 从 `windowRoot_` 开始，而非当前 VC 的 root view。这确保状态栏区域的触摸能被正确处理。

#### Scenario: 触摸状态栏区域
- **WHEN** 触摸坐标 (100, 10) 落在状态栏区域
- **THEN** hitTest 命中 StatusBarView，事件由 StatusBarView 处理（当前不消费，冒泡丢弃）

#### Scenario: 触摸内容区域
- **WHEN** 触摸坐标 (100, 500) 落在内容区域
- **THEN** hitTest 穿过 contentArea_ 到达 VC 的 View 树，正常处理
