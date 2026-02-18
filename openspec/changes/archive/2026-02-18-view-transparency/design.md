## Context

InkUI 渲染管线当前对每个 View 无条件执行 `canvas.clear(backgroundColor)`。View 基类默认背景色为白色 (0xFF)。这造成两个视觉问题：

1. 叶子视图（TextLabel、IconView）在非白色容器中产生白色矩形边界
2. `drawChar()` 硬编码白色背景进行 alpha 混合，文字边缘在非白色背景上出现光晕

当前代码中有大量冗余的 `setBackgroundColor(Color::White)` 调用分散在 HeaderView、StatusBarView、Application 等处。

## Goals / Non-Goals

**Goals:**

- View 默认透明：`backgroundColor_` 默认为 `Color::Clear`，不画背景
- 仅 `windowRoot_` 保留白色背景作为全屏底色，所有其他 View 去掉显式背景色设置
- 文字反走样正确：`drawChar()` 读取 framebuffer 实际像素值进行 alpha 混合
- 保持向后兼容：显式设置了 `backgroundColor` 的外部代码行为不变

**Non-Goals:**

- 不实现半透明叠加（view-level opacity / alpha compositing），留待后续
- 不引入新的 Canvas alpha 填充原语（如 `fillRectAlpha`）
- 不修改 `opaque_` 字段的语义（保留但不使用）

## Decisions

### 1. 使用 `Color::Clear` 哨兵值而非 `opaque_` 标志

**选择**: 新增 `Color::Clear = 0x01` 作为"透明"的哨兵值，RenderEngine 通过 `backgroundColor() != Color::Clear` 判断是否 clear。

**替代方案**: 使用已有的 `opaque_` 布尔字段控制是否 clear。

**理由**: `opaque_` 的语义是"此 View 是否完全遮挡下方内容"，这是一个渲染优化提示。而"是否需要画背景"是 `backgroundColor` 属性的职责——没有背景色就不画。用单一属性表达更直观，减少需要维护的状态。`0x01` 在 4bpp 灰度中不是有效值（有效值为 0x00, 0x10, ..., 0xF0），天然适合做哨兵。

### 2. View 基类默认 Clear 而非白色

**选择**: `backgroundColor_` 默认值从 `0xFF` 改为 `Color::Clear`。

**替代方案**: 保持默认白色，只在 TextLabel/IconView 构造函数中设 Clear。

**理由**: 透明是更正确的默认语义（与 UIKit `backgroundColor = nil` 一致）。大多数 View 不需要自己的背景——它们依赖父 View 的背景透传。只有容器根节点和有特殊底色的 View 需要显式设置。当前代码中所有需要白色背景的容器级 View（windowRoot_、HeaderView）已有显式设置，改默认值后只需移除冗余设置。

### 3. drawChar 读取实际背景像素

**选择**: 将 `drawChar()` 中 `uint8_t bg4 = 0x0F` 改为 `bg4 = getPixel(absX, absY) >> 4`。

**替代方案**: 给 drawChar 传入背景色参数。

**理由**: 与 `drawBitmapFg()` 行为一致（后者已正确读取背景像素）。读取 framebuffer 比传参更通用——不需要调用方知道当前背景色。`getPixel` 是简单的数组索引，性能开销可接受。

### 4. 移除所有子 View 的显式 setBackgroundColor(White)

**选择**: HeaderView 构造函数、rebuild()、StatusBarView、Application::contentArea_ 中的 `setBackgroundColor(White)` 全部移除。

**理由**: windowRoot_ 的白色背景通过透明 View 层级自然透传到所有子 View。冗余的背景色设置是当前 bug 的根源（每个 View 都 clear 白色→覆盖父背景→产生色差边线）。

## Risks / Trade-offs

- **[BREAKING] 未显式设置背景色的第三方 View**: 从白色背景变为透明 → 如果其 `onDraw` 依赖白色底色，视觉可能异常。**缓解**: 当前无第三方代码，所有 View 均为内部实现。
- **[性能] drawChar 每像素读 framebuffer**: 增加了内存读取。**缓解**: 4bpp 像素读取仅是单次数组索引 + 位操作，在 ESP32-S3 的 PSRAM 上开销极小；`drawBitmapFg` 已使用相同模式无性能问题。
- **[语义] Color::Clear 值 0x01 被误用为灰度值**: 如果代码意外将 0x01 传给 fillRect 等函数，会写入接近黑色的像素。**缓解**: 0x01 不是标准灰度级（标准级为 0x10 步进），正常使用中不会出现；且 Canvas 的 `clear()` 在 RenderEngine 中已有 Clear 检查守护。
