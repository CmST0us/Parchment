## MODIFIED Requirements

### Requirement: Application 初始化
`ink::Application` SHALL 提供 `init(DisplayDriver&, TouchDriver&, Platform&, SystemInfo&)` 方法，接受 HAL 依赖注入，按顺序初始化所有子系统：
1. DisplayDriver 初始化（调用 `display.init()`）
2. 通过 Platform 创建事件队列
3. 创建 RenderEngine（绑定 DisplayDriver）
4. 创建 GestureRecognizer（绑定 TouchDriver 和 Platform）
5. 启动 GestureRecognizer 触摸任务
6. **创建 Screen View 树（screenRoot_ + windowRoot_ + overlayRoot_ + statusBar_ + contentArea_）**
7. **创建 ModalPresenter（绑定 overlayRoot_、screenRoot_、RenderEngine、Application）**
8. **设置 NavigationController 的 onViewControllerChange 回调**

init() 成功后 SHALL 返回 true。

#### Scenario: 成功初始化
- **WHEN** 调用 `app.init(display, touch, platform, systemInfo)`，所有 HAL 正常
- **THEN** 返回 true，显示就绪，触摸任务已启动，Screen View 树已创建，ModalPresenter 已就绪

### Requirement: 主事件循环
`run()` SHALL 启动永不返回的主事件循环（标记 `[[noreturn]]`）。循环逻辑：
1. 通过 `platform_->queueReceive()` 阻塞等待事件（超时 30 秒）
2. 如有事件，调用 dispatchEvent() 分发
3. **检查时间变化，必要时更新 StatusBar 时间**
4. **检查是否需要更新电池电量（间隔 ≥30 秒），必要时调用 `system_->batteryPercent()` 并更新 statusBar**
5. **调用 RenderEngine::renderCycle(screenRoot_.get()) 执行布局和渲染**

无论是否有事件，每轮循环末尾 SHALL 执行 renderCycle（处理可能的 pending 布局/重绘）。

#### Scenario: 事件驱动渲染
- **WHEN** 触摸事件到达，View 处理后调用 setNeedsDisplay()
- **THEN** renderCycle 以 screenRoot_ 为根重绘该 View 并刷新屏幕

#### Scenario: 30 秒无事件
- **WHEN** 无任何事件超过 30 秒
- **THEN** renderCycle 仍执行一次（处理可能的 pending 状态），并检查时间和电池更新

#### Scenario: 电池电量定期更新
- **WHEN** 距上次电池更新已超过 30 秒
- **THEN** 调用 `system_->batteryPercent()` 读取电量，调用 `statusBar_->updateBattery()` 更新图标

### Requirement: 触摸事件分发
Application SHALL 将 TouchEvent 通过以下路径分发：
1. **从 screenRoot_ 开始 hitTest**（覆盖 overlayRoot_ 和 windowRoot_）
2. 调用 screenRoot_->hitTest(event.x, event.y) 找到目标 View
3. **如果 ModalPresenter::isBlocking() 为 true，且目标 View 不在 overlayRoot_ 子树中，则吞掉触摸事件不传递**
4. 否则调用目标 View 的 onTouchEvent()，事件沿 parent 链冒泡直到被消费

SwipeEvent SHALL 在 ModalPresenter::isBlocking() 为 true 时被吞掉。否则传递给当前 VC 的 handleEvent()。

TimerEvent SHALL 先交给 ModalPresenter::handleTimer() 处理。如果 ModalPresenter 消费了该事件（返回 true），则不再传递给 VC。否则传递给当前 VC 的 handleEvent()。

#### Scenario: 触摸命中 Alert 按钮
- **WHEN** Alert 模态显示中，触摸坐标落在 Alert 内的 Button 区域
- **THEN** hitTest 命中 Button（overlayRoot_ 子树中），触摸正常传递

#### Scenario: 阻塞模态时触摸底层区域
- **WHEN** Alert 模态显示中（isBlocking=true），触摸坐标落在 Alert 外的内容区域
- **THEN** hitTest 命中 windowRoot_ 子树中的 View，但触摸被吞掉不传递

#### Scenario: Toast 显示时触摸底层区域
- **WHEN** 仅 Toast 显示（isBlocking=false），触摸坐标落在 Toast 外的内容区域
- **THEN** 触摸正常传递给底层 View

#### Scenario: Swipe 被阻塞模态吞掉
- **WHEN** Alert 模态显示中，用户滑动屏幕
- **THEN** SwipeEvent 被吞掉，不传递给当前 VC

#### Scenario: Timer 事件路由
- **WHEN** Timer 事件到达，timerId 属于 ModalPresenter 保留范围
- **THEN** ModalPresenter::handleTimer() 返回 true，事件不传递给 VC

## ADDED Requirements

### Requirement: ModalPresenter 访问器
Application SHALL 提供 `modalPresenter()` 方法，返回 `ModalPresenter&` 引用。ViewController 通过 `app_.modalPresenter()` 访问模态呈现能力。

#### Scenario: VC 显示 Alert
- **WHEN** ViewController 需要显示 Alert
- **THEN** 调用 `app_.modalPresenter().showAlert(std::move(alertView))` 呈现 Alert
