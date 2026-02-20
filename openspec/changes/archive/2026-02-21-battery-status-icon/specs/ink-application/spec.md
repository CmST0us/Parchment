## MODIFIED Requirements

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
