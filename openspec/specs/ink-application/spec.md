## ADDED Requirements

### Requirement: Application 初始化
`ink::Application` SHALL 提供 `init()` 方法，按顺序初始化所有子系统：
1. EpdDriver 初始化（EPD 硬件 + framebuffer）
2. 创建 FreeRTOS 事件队列
3. 创建 RenderEngine（绑定 EpdDriver）
4. 创建 GestureRecognizer（绑定事件队列）
5. 启动 GestureRecognizer 触摸任务

init() 成功后 SHALL 返回 true。

#### Scenario: 成功初始化
- **WHEN** 调用 app.init()，硬件正常
- **THEN** 返回 true，EPD 显示就绪，触摸任务已启动

### Requirement: 主事件循环
`run()` SHALL 启动永不返回的主事件循环（标记 `[[noreturn]]`）。循环逻辑：
1. 从事件队列阻塞等待事件（超时 30 秒）
2. 如有事件，调用 dispatchEvent() 分发
3. 获取 NavigationController 当前 VC 的 View 树
4. 调用 RenderEngine::renderCycle() 执行布局和渲染

无论是否有事件，每轮循环末尾 SHALL 执行 renderCycle（处理可能的 pending 布局/重绘）。

#### Scenario: 事件驱动渲染
- **WHEN** 触摸事件到达，View 处理后调用 setNeedsDisplay()
- **THEN** renderCycle 重绘该 View 并刷新屏幕

#### Scenario: 30 秒无事件
- **WHEN** 无任何事件超过 30 秒
- **THEN** renderCycle 仍执行一次（处理可能的 pending 状态），但无脏区域时快速返回

### Requirement: 触摸事件分发
Application SHALL 将 TouchEvent 通过以下路径分发：
1. 获取当前 VC 的根 View
2. 调用 rootView->hitTest(event.x, event.y) 找到目标 View
3. 调用目标 View 的 onTouchEvent()，事件沿 parent 链冒泡直到被消费

SwipeEvent SHALL 直接传递给当前 VC 的 handleEvent()。

#### Scenario: 触摸命中 Button
- **WHEN** Tap 事件坐标落在 ButtonView 区域内
- **THEN** ButtonView.onTouchEvent() 被调用

#### Scenario: Swipe 传递给 VC
- **WHEN** SwipeEvent{direction=Left} 到达
- **THEN** 当前 VC 的 handleEvent() 被调用

### Requirement: 非触摸事件分发
TimerEvent 和 CustomEvent SHALL 传递给当前 VC 的 `handleEvent()` 方法。

#### Scenario: 定时器事件
- **WHEN** TimerEvent 到达
- **THEN** 当前 VC 的 handleEvent() 接收该事件

### Requirement: navigator 访问
Application SHALL 提供 `navigator()` 方法返回 `NavigationController&`，允许外部代码 push/pop 页面。

#### Scenario: push 首页
- **WHEN** 调用 app.navigator().push(std::move(bootVC))
- **THEN** bootVC 成为当前页面

### Requirement: renderer 访问
Application SHALL 提供 `renderer()` 方法返回 `RenderEngine&`，允许外部代码调用 forceFullClear() 等。

#### Scenario: 手动清除残影
- **WHEN** 调用 app.renderer().forceFullClear()
- **THEN** 执行 GC16 全屏刷新

### Requirement: postEvent 发送事件
Application SHALL 提供 `postEvent(const Event& event)` 方法，将事件放入 FreeRTOS 队列，唤醒主循环处理。

#### Scenario: 发送自定义事件
- **WHEN** 从任意线程调用 postEvent(customEvent)
- **THEN** 事件进入队列，主循环下一轮处理该事件

### Requirement: postDelayed 延迟事件
Application SHALL 提供 `postDelayed(const Event& event, int delayMs)` 方法，使用 esp_timer 在指定延迟后将事件放入队列。

#### Scenario: 延迟 500ms 发送
- **WHEN** 调用 postDelayed(event, 500)
- **THEN** 约 500ms 后事件出现在队列中
