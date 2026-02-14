## ADDED Requirements

### Requirement: RenderEngine 构造
`ink::RenderEngine` SHALL 接受 `EpdDriver&` 引用构造，并从 EpdDriver 获取 framebuffer 指针。

#### Scenario: 构造并绑定驱动
- **WHEN** 创建 `RenderEngine(driver)`
- **THEN** RenderEngine 持有 driver 引用和 framebuffer 指针，脏区域为空

### Requirement: 5 阶段渲染循环
`renderCycle(View* rootView)` SHALL 按以下 5 个阶段执行：

1. **Layout Pass**：若 rootView needsLayout，递归执行 onLayout()
2. **Collect Dirty**：遍历 View 树，收集 needsDisplay==true 的 View 的 screenFrame 和 refreshHint 为脏区域
3. **Draw Dirty**：对每个脏 View，创建 Canvas(fb, screenFrame)，先 clear(backgroundColor)，再调用 onDraw(canvas)，然后强制重绘其所有子 View
4. **Flush**：合并重叠脏区域，对每个区域调用 EpdDriver::updateArea() 刷新
5. **Ghost Management**：累计局部刷新计数，超过阈值时自动执行 GC16 全屏清除

无脏区域时 SHALL 跳过 Phase 3-5。

#### Scenario: 有脏 View 时完整渲染
- **WHEN** View 树中某个 View 调用了 setNeedsDisplay()
- **THEN** renderCycle 执行全部 5 阶段，该 View 被重绘，对应屏幕区域被刷新

#### Scenario: 无脏 View 时跳过渲染
- **WHEN** View 树无脏标记
- **THEN** renderCycle 不执行重绘和刷新，快速返回

### Requirement: 多脏区域管理
RenderEngine SHALL 维护最多 `MAX_DIRTY_REGIONS`（8）个独立脏区域，每个区域包含 Rect 和 RefreshHint。

当收集到的脏区域超过 MAX_DIRTY_REGIONS 时，SHALL 将相邻区域合并以腾出空间。

#### Scenario: 多个独立区域
- **WHEN** header（y=0..48）和 list item（y=300..380）分别需要刷新
- **THEN** 产生 2 个独立脏区域，各自独立刷新

#### Scenario: 区域数量超限
- **WHEN** 超过 8 个 View 同时变脏
- **THEN** 相邻区域被合并，总区域数不超过 8

### Requirement: 脏区域合并策略
合并 SHALL 遵循以下规则：
- 相邻（垂直距离 <= gap 阈值）且 RefreshHint 相同的区域 SHALL 合并
- 不同 RefreshHint 的区域 SHALL 保持独立
- 合并后总面积超过屏幕面积 60% 时 SHALL 退化为全屏刷新（Quality 模式）

#### Scenario: 相邻同 hint 合并
- **WHEN** 区域 A(0,300,540,80) hint=Quality 和 区域 B(0,380,540,80) hint=Quality
- **THEN** 合并为 (0,300,540,160) hint=Quality

#### Scenario: 不同 hint 不合并
- **WHEN** 区域 A hint=Fast 和 区域 B hint=Quality
- **THEN** A 和 B 保持独立，分别以 MODE_DU 和 MODE_GL16 刷新

### Requirement: RefreshHint 到 EPD Mode 映射
RenderEngine SHALL 按以下规则将 RefreshHint 映射到 EPD 刷新模式：
- `Fast` → MODE_DU
- `Quality` → MODE_GL16
- `Full` → MODE_GC16
- `Auto` → MODE_GL16（默认）

#### Scenario: Fast hint 使用 DU
- **WHEN** 脏区域的 RefreshHint 为 Fast
- **THEN** 使用 MODE_DU 刷新该区域

### Requirement: 残影计数与自动 GC16
RenderEngine SHALL 维护 `partialCount_` 计数器，每次局部刷新（非 GC16）加 1。当计数达到 `GHOST_THRESHOLD`（10）时，SHALL 自动执行一次全屏 GC16 刷新并重置计数器。

`forceFullClear()` SHALL 立即执行全屏 GC16 并重置计数器。

#### Scenario: 累计触发 GC16
- **WHEN** 连续执行 10 次局部刷新（MODE_DU 或 MODE_GL16）
- **THEN** 第 10 次后自动执行全屏 GC16 清除残影，计数器归零

#### Scenario: 手动清除
- **WHEN** 调用 forceFullClear()
- **THEN** 立即执行 GC16 全屏刷新，计数器归零

### Requirement: Subtree 重绘
当父 View needsDisplay==true 时，Phase 3 SHALL 强制重绘其所有子 View（无论子 View 的 needsDisplay 状态），因为父 View 的 clear 操作会覆盖子 View 区域。

当父 View needsDisplay==false 但 subtreeNeedsDisplay==true 时，SHALL 递归进入子 View 寻找脏节点。

#### Scenario: 父 View 变脏时子 View 被强制重绘
- **WHEN** 容器 View 调用 setNeedsDisplay()，其子 View 未变脏
- **THEN** 容器和所有子 View 都被重绘

#### Scenario: 只有子 View 变脏
- **WHEN** 子 View 调用 setNeedsDisplay()，父 View 未变脏
- **THEN** 只有该子 View 被重绘，父 View 的 onDraw 不被调用

### Requirement: repairDamage 损伤修复
`repairDamage(View* rootView, const Rect& damage)` SHALL 对指定矩形区域执行裁剪重绘：找到与 damage 区域重叠的所有 View，在 damage 范围内重绘它们，然后刷新该区域到 EPD。

#### Scenario: 浮层关闭后修复
- **WHEN** 浮层消失，调用 repairDamage(root, floatRect)
- **THEN** floatRect 区域内的底层 View 被重绘，EPD 刷新该区域
