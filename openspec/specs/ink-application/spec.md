## MODIFIED Requirements

### Requirement: Application 初始化
`ink::Application` SHALL 提供 `init()` 方法，按顺序初始化所有子系统：
1. EpdDriver 初始化（EPD 硬件 + framebuffer）
2. 创建 FreeRTOS 事件队列
3. 创建 RenderEngine（绑定 EpdDriver）
4. 创建 GestureRecognizer（绑定事件队列）
5. 启动 GestureRecognizer 触摸任务
6. **创建 Window View 树（windowRoot_ + statusBar_ + contentArea_）**
7. **设置 NavigationController 的 onViewControllerChange 回调**

init() 成功后 SHALL 返回 true。

#### Scenario: 成功初始化
- **WHEN** 调用 app.init()，硬件正常
- **THEN** 返回 true，EPD 显示就绪，触摸任务已启动，Window View 树已创建

### Requirement: 主事件循环
`run()` SHALL 启动永不返回的主事件循环（标记 `[[noreturn]]`）。循环逻辑：
1. 从事件队列阻塞等待事件（超时 30 秒）
2. 如有事件，调用 dispatchEvent() 分发
3. **检查时间变化，必要时更新 StatusBar 时间**
4. **检查是否需要更新电池电量（间隔 ≥30 秒），必要时读取电池百分比并调用 statusBar_->updateBattery()**
5. **调用 RenderEngine::renderCycle(windowRoot_.get()) 执行布局和渲染**

无论是否有事件，每轮循环末尾 SHALL 执行 renderCycle（处理可能的 pending 布局/重绘）。

#### Scenario: 事件驱动渲染
- **WHEN** 触摸事件到达，View 处理后调用 setNeedsDisplay()
- **THEN** renderCycle 重绘该 View 并刷新屏幕

#### Scenario: 30 秒无事件
- **WHEN** 无任何事件超过 30 秒
- **THEN** renderCycle 仍执行一次（处理可能的 pending 状态），并检查时间和电池更新

#### Scenario: 电池电量定期更新
- **WHEN** 距上次电池更新已超过 30 秒
- **THEN** 调用 `battery_get_percent()` 读取电量，调用 `statusBar_->updateBattery()` 更新图标

### Requirement: 触摸事件分发
Application SHALL 将 TouchEvent 通过以下路径分发：
1. **从 windowRoot_ 开始 hitTest**（而非当前 VC 的根 View）
2. 调用 windowRoot_->hitTest(event.x, event.y) 找到目标 View
3. 调用目标 View 的 onTouchEvent()，事件沿 parent 链冒泡直到被消费

SwipeEvent SHALL 直接传递给当前 VC 的 handleEvent()。

#### Scenario: 触摸命中 Button
- **WHEN** Tap 事件坐标落在 ButtonView 区域内
- **THEN** ButtonView.onTouchEvent() 被调用

#### Scenario: Swipe 传递给 VC
- **WHEN** SwipeEvent{direction=Left} 到达
- **THEN** 当前 VC 的 handleEvent() 被调用
