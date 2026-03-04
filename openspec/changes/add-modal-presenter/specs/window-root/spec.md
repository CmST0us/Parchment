## MODIFIED Requirements

### Requirement: Window View 树结构
Application SHALL 在 `init()` 中创建一个固定的 Screen View 树结构：
- `screenRoot_`: 全屏根 View (540x960)，FlexDirection::None（子节点自由叠放）
  - `windowRoot_`: 全屏子 View (540x960)，FlexBox Column 方向
    - `statusBar_`: StatusBarView 子 View (540x20)，作为 windowRoot_ 的第一个子 View
    - `contentArea_`: 普通 View 子 View，flexGrow=1 填满剩余空间 (540x940)
  - `overlayRoot_`: 全屏子 View (540x960)，FlexDirection::None，backgroundColor=Clear
    - 由 ModalPresenter 管理子 View 的添加和移除

Screen View 树的生命期 SHALL 与 Application 一致，不随 ViewController 切换而销毁。

`windowRoot_` 作为 screenRoot_ 的第一个子节点，`overlayRoot_` 作为第二个子节点。由于 hitTest 逆序遍历子节点，overlayRoot_ 中的 View SHALL 优先于 windowRoot_ 被命中。

`overlayRoot_` 在无模态显示时 SHALL 设置为 hidden（不参与渲染和 hitTest）。当有模态显示时由 ModalPresenter 设置为 visible。

#### Scenario: Application 初始化后 Screen 树就绪
- **WHEN** 调用 `app.init()` 成功
- **THEN** `screenRoot_` 已创建，包含 `windowRoot_` 和 `overlayRoot_` 两个子 View；`windowRoot_` 包含 statusBar_ 和 contentArea_

#### Scenario: VC 切换不影响 Screen 树
- **WHEN** NavigationController push 新 VC
- **THEN** screenRoot_、windowRoot_、overlayRoot_ 和 statusBar_ 保持不变，仅 contentArea_ 的子 View 更新

#### Scenario: overlayRoot_ Z 序优先
- **WHEN** overlayRoot_ 中有 View 且 hitTest 坐标同时落在 overlayRoot_ 和 windowRoot_ 的子 View 范围内
- **THEN** hitTest 优先命中 overlayRoot_ 中的 View（后添加的子节点优先）

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

VC 的 root view SHALL 不再直接设置 540x960 的 frame，而是由 contentArea_ 的 FlexBox 布局自动约束。

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
Application 的 renderCycle SHALL 以 `screenRoot_` 为根执行渲染，而非 `windowRoot_`。这确保 overlayRoot_ 中的模态 View 也被渲染引擎处理。

#### Scenario: renderCycle 渲染整个 Screen 树
- **WHEN** 主循环执行 renderCycle
- **THEN** `renderEngine_->renderCycle(screenRoot_.get())` 被调用，覆盖 windowRoot_ 和 overlayRoot_ 两个子树

### Requirement: 触摸事件从 Screen 根开始 hitTest
触摸事件的 hitTest SHALL 从 `screenRoot_` 开始，而非 `windowRoot_`。这确保 overlayRoot_ 中的模态 View 能优先接收触摸事件。

#### Scenario: 触摸模态 View 区域
- **WHEN** 模态 View 显示中，触摸坐标落在模态 View 范围内
- **THEN** hitTest 从 screenRoot_ 开始，命中 overlayRoot_ 中的模态 View

#### Scenario: 无模态时触摸内容区域
- **WHEN** 无模态显示（overlayRoot_ hidden），触摸坐标 (100, 500) 落在内容区域
- **THEN** hitTest 跳过 hidden 的 overlayRoot_，穿过 windowRoot_ 到达 VC 的 View 树
