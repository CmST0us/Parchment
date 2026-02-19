## MODIFIED Requirements

### Requirement: Application 初始化
`ink::Application` SHALL 提供 `init()` 方法，按顺序初始化所有子系统：
1. 调用 `M5.begin(cfg)` 初始化 M5PaperS3 硬件（显示 + 触摸 + 电源）
2. 设置 `M5.Display.setAutoDisplay(false)` 关闭自动刷新
3. 设置 `M5.Display.setEpdMode(epd_mode_t::epd_quality)` 默认刷新模式
4. 初始化字体引擎（加载 .bin 字体文件，构建 HashMap 索引，填充常用字缓存）
5. 创建 FreeRTOS 事件队列
6. 创建 RenderEngine（绑定 `&M5.Display`）
7. 创建 GestureRecognizer（绑定事件队列）
8. 启动 GestureRecognizer 触摸任务
9. 创建 Window View 树（windowRoot_ + statusBar_ + contentArea_）
10. 设置 NavigationController 的 onViewControllerChange 回调

init() 成功后 SHALL 返回 true。

#### Scenario: 成功初始化
- **WHEN** 调用 app.init()，硬件正常
- **THEN** 返回 true，M5GFX 显示就绪，字体引擎已加载，触摸任务已启动，Window View 树已创建

### Requirement: 主事件循环
`run()` SHALL 启动永不返回的主事件循环（标记 `[[noreturn]]`）。循环逻辑：
1. 从事件队列阻塞等待事件（超时 30 秒）
2. 如有事件，调用 dispatchEvent() 分发
3. 检查时间变化，必要时更新 StatusBar
4. 调用 RenderEngine::renderCycle(windowRoot_.get()) 执行布局和渲染

无论是否有事件，每轮循环末尾 SHALL 执行 renderCycle。

#### Scenario: 事件驱动渲染
- **WHEN** 触摸事件到达，View 处理后调用 setNeedsDisplay()
- **THEN** renderCycle 重绘该 View 并通过 M5GFX 刷新屏幕

#### Scenario: 30 秒无事件
- **WHEN** 无任何事件超过 30 秒
- **THEN** renderCycle 仍执行一次，并检查时间更新
