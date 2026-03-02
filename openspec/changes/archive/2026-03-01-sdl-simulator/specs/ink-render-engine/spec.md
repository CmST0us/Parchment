## MODIFIED Requirements

### Requirement: RenderEngine 构造
`ink::RenderEngine` SHALL 接受 `DisplayDriver&` 引用构造，并从 DisplayDriver 获取 framebuffer 指针。

#### Scenario: 构造并绑定驱动
- **WHEN** 创建 `RenderEngine(driver)`，driver 为 DisplayDriver 子类实例
- **THEN** RenderEngine 持有 driver 引用和 framebuffer 指针，脏区域为空

### Requirement: 4 阶段渲染循环
`renderCycle(View* rootView)` SHALL 按以下 4 个阶段执行：

1. **Layout Pass**：若 rootView needsLayout，递归执行 onLayout()
2. **Collect Dirty**：遍历 View 树，收集 needsDisplay==true 的 View 的 screenFrame 和 refreshHint 为脏区域
3. **Draw Dirty**：对每个脏 View，创建 Canvas(fb, screenFrame)。若 `backgroundColor != Color::Clear`，先 clear(backgroundColor)；若 backgroundColor 为 Clear 且非强制重绘，用最近祖先的非透明背景色清除。然后调用 onDraw(canvas)，再强制重绘其所有子 View
4. **Flush**：合并重叠脏区域，对每个区域通过 `driver_.updateArea()` 以 `RefreshMode::Full` 刷新

无脏区域时 SHALL 跳过 Phase 3-4。

#### Scenario: 有脏 View 时完整渲染
- **WHEN** View 树中某个 View 调用了 setNeedsDisplay()
- **THEN** renderCycle 执行全部 4 阶段，该 View 被重绘，对应屏幕区域被刷新

#### Scenario: 无脏 View 时跳过渲染
- **WHEN** View 树无脏标记
- **THEN** renderCycle 不执行重绘和刷新，快速返回

#### Scenario: 透明 View 继承祖先背景清除
- **WHEN** 一个 backgroundColor 为 Color::Clear 的 View 需要重绘（非强制）
- **THEN** RenderEngine 查找最近的非透明祖先背景色，用该颜色 clear 后调用 onDraw()

#### Scenario: 不透明 View 清除背景
- **WHEN** 一个 backgroundColor 为 Color::White 的 View 需要重绘
- **THEN** RenderEngine 先调用 canvas.clear(Color::White)，再调用 onDraw()

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
- 相邻（距离 <= 8px 容差）且 RefreshHint 相同的区域 SHALL 合并
- 不同 RefreshHint 的区域 SHALL 保持独立

#### Scenario: 相邻同 hint 合并
- **WHEN** 区域 A(0,300,540,80) hint=Fast 和 区域 B(0,380,540,80) hint=Fast
- **THEN** 合并为 (0,300,540,160) hint=Fast

#### Scenario: 不同 hint 不合并
- **WHEN** 区域 A hint=Fast 和 区域 B hint=Quality
- **THEN** A 和 B 保持独立，但均以 RefreshMode::Full 刷新

### Requirement: 统一 Full 刷新
RenderEngine SHALL 对所有脏区域统一使用 `RefreshMode::Full` 调用 `driver_.updateArea()` 刷新，忽略 RefreshHint 的具体值。

#### Scenario: 所有区域使用 Full 模式
- **WHEN** 脏区域的 RefreshHint 为 Fast、Quality 或 Full
- **THEN** 均通过 `driver_.updateArea(..., RefreshMode::Full)` 刷新

### Requirement: 逻辑到物理坐标变换
`logicalToPhysical(Rect)` SHALL 将逻辑坐标 (540×960 portrait) 转换为物理坐标 (960×540 landscape)：
- `physical_x = logical_y`
- `physical_y = kScreenWidth - logical_x - logical_w`
- 宽高互换

#### Scenario: 坐标变换
- **WHEN** 逻辑区域 {x=0, y=0, w=540, h=20}
- **THEN** 物理区域 {x=0, y=0, w=20, h=540}

### Requirement: Subtree 重绘
当父 View needsDisplay==true 时，Phase 3 SHALL 强制重绘其所有子 View（无论子 View 的 needsDisplay 状态），因为父 View 的 clear 操作可能覆盖子 View 区域。

当父 View needsDisplay==false 但 subtreeNeedsDisplay==true 时，SHALL 递归进入子 View 寻找脏节点。

透明子 View（backgroundColor == Color::Clear）被强制重绘时，SHALL 跳过 clear 直接执行 onDraw，其内容绘制在父 View 已清除的背景上。

#### Scenario: 父 View 变脏时子 View 被强制重绘
- **WHEN** 容器 View 调用 setNeedsDisplay()，其子 View 未变脏
- **THEN** 容器和所有子 View 都被重绘

#### Scenario: 只有子 View 变脏
- **WHEN** 子 View 调用 setNeedsDisplay()，父 View 未变脏
- **THEN** 只有该子 View 被重绘，父 View 的 onDraw 不被调用

### Requirement: repairDamage 损伤修复
`repairDamage(View* rootView, const Rect& damage)` SHALL 对指定矩形区域执行裁剪重绘：找到与 damage 区域重叠的所有 View，在 damage 范围内重绘它们，然后通过 `driver_.updateArea()` 以 `RefreshMode::Full` 刷新该区域。

#### Scenario: 浮层关闭后修复
- **WHEN** 浮层消失，调用 repairDamage(root, floatRect)
- **THEN** floatRect 区域内的底层 View 被重绘，通过 DisplayDriver 刷新该区域
