## MODIFIED Requirements

### Requirement: RenderEngine 构造
`ink::RenderEngine` SHALL 接受 `M5GFX*` 显示设备指针构造，不再依赖 `EpdDriver`。

#### Scenario: 构造并绑定 M5GFX
- **WHEN** 创建 `RenderEngine(display)`
- **THEN** RenderEngine 持有 M5GFX display 指针，脏区域为空

### Requirement: 4 阶段渲染循环
`renderCycle(View* rootView)` SHALL 按以下 4 个阶段执行：

1. **Layout Pass**: 若 rootView needsLayout，递归执行 onLayout()
2. **Collect Dirty**: 遍历 View 树，收集 needsDisplay==true 的 View 的 screenFrame 和 refreshHint 为脏区域
3. **Draw Dirty**: 对每个脏 View，创建 `Canvas(display, screenFrame)`。若 `backgroundColor != Color::Clear`，先 clear(backgroundColor)；否则跳过 clear。然后调用 onDraw(canvas)，再强制重绘其所有子 View
4. **Flush**: 合并重叠脏区域，对每个区域调用 `display->display(x, y, w, h)` 刷新。坐标使用逻辑坐标（540×960 portrait），M5GFX 内部自动处理坐标变换

无脏区域时 SHALL 跳过 Phase 3-4。

#### Scenario: 有脏 View 时完整渲染
- **WHEN** View 树中某个 View 调用了 setNeedsDisplay()
- **THEN** renderCycle 执行全部 4 阶段，该 View 被重绘，对应屏幕区域通过 `display->display()` 刷新

#### Scenario: 无脏 View 时跳过渲染
- **WHEN** View 树无脏标记
- **THEN** renderCycle 不执行重绘和刷新，快速返回

#### Scenario: 透明 View 不清除背景
- **WHEN** 一个 backgroundColor 为 Color::Clear 的 View 需要重绘
- **THEN** RenderEngine 跳过 canvas.clear()，直接调用 onDraw()

#### Scenario: 不透明 View 清除背景
- **WHEN** 一个 backgroundColor 为 Color::White 的 View 需要重绘
- **THEN** RenderEngine 先调用 canvas.clear(Color::White)，再调用 onDraw()

### Requirement: 统一快速刷新
RenderEngine SHALL 在 flush 阶段使用 `epd_quality` 模式（通过 `display->setEpdMode(epd_mode_t::epd_quality)`）刷新所有脏区域。坐标直接使用逻辑坐标，无需 logicalToPhysical 变换。

#### Scenario: 逻辑坐标直接刷新
- **WHEN** 脏区域为逻辑 Rect{0, 100, 540, 80}
- **THEN** 直接调用 `display->display(0, 100, 540, 80)`，M5GFX 内部处理坐标变换

### Requirement: repairDamage 损伤修复
`repairDamage(View* rootView, const Rect& damage)` SHALL 对指定矩形区域执行裁剪重绘：找到与 damage 区域重叠的所有 View，在 damage 范围内重绘它们，然后通过 `display->display()` 刷新该区域。坐标使用逻辑坐标。

#### Scenario: 浮层关闭后修复
- **WHEN** 浮层消失，调用 repairDamage(root, floatRect)
- **THEN** floatRect 区域内的底层 View 被重绘，通过 `display->display()` 刷新

## REMOVED Requirements

### Requirement: logicalToPhysical 坐标变换
**Reason**: M5GFX 的 `setRotation()` + `display(x,y,w,h)` 自动处理 portrait→landscape 坐标变换。
**Migration**: flush 阶段直接使用逻辑坐标调用 `display->display()`，删除 `logicalToPhysical()` 方法。
