## Context

InkUI 框架重构阶段 0 已完成：C++17 编译链路通过，`Geometry.h` 和 `EpdDriver.h` 已实现。阶段 1 的核心目标是实现 Canvas 绘图引擎，为后续 View 树 + RenderEngine 的局部重绘提供基础。

旧 `ui_canvas.c`（423 行）提供全局函数式绘图 API，无裁剪概念。新 `ink::Canvas` 需要支持裁剪区域和局部坐标，使得每个 View 的 `onDraw()` 只在自己的边界内绘制。

## Goals / Non-Goals

**Goals:**
- 实现 `ink::Canvas` 类，带裁剪区域 (clip rect) 和局部坐标系
- 迁移旧 `ui_canvas.c` 的全部绘图能力：几何图形、位图、文字
- Canvas 文字渲染直接接受 `EpdFont*`，不依赖 FontManager
- 提供 `clipped()` 方法创建子 Canvas，支持嵌套 View 绘制
- 用 main.cpp 临时测试代码在墨水屏上验证

**Non-Goals:**
- 不实现 FontManager（阶段 4）
- 不实现 View 基类或 RenderEngine（后续 step）
- 不做 fillRect 的物理行级 memset 优化（功能优先，性能后补）
- 不修改旧 `ui_core` 代码

## Decisions

### 1. clip_ 存储屏幕绝对坐标

**选择**: `clip_` 成员使用 540×960 逻辑坐标系的绝对坐标，绘图方法内部将局部坐标加上 `clip_.x/y` 转为绝对坐标后做裁剪检查。

**替代方案**: clip_ 存储局部坐标 + 偏移量分开存储。增加复杂度，无实际收益。

**理由**: `clipped()` 实现简洁——只需取两个绝对 Rect 的交集。RenderEngine 创建 Canvas 时直接传入 View 的 `screenFrame()`，无需额外转换。

### 2. 文字渲染内置于 Canvas

**选择**: `drawText()` / `measureText()` 作为 Canvas 的成员方法，接受 `const EpdFont*` 参数。

**替代方案**: 独立 TextRenderer 类。

**理由**: View 的 `onDraw(Canvas&)` 签名只传一个对象。如果文字在另一个对象里，每个需要画文字的 View 都得额外获取 TextRenderer 引用，增加 API 复杂度。`EpdFont*` 来自 epdiy（已是依赖），不引入新依赖。

### 3. 复用旧 ui_canvas.c 的像素操作逻辑

**选择**: 将 `fb_set_pixel`、`fb_get_pixel`、UTF-8 解码、glyph 解压等逻辑从 `ui_canvas.c` 迁移到 `Canvas.cpp`，作为 private 方法或文件内 static 函数。

**理由**: 这些是经过验证的正确实现（坐标变换、4bpp 打包、zlib 解压），没有理由重写。迁移时增加 clip 裁剪检查即可。

### 4. drawText 的 y 坐标语义为基线

**选择**: 与旧 API 一致，`drawText(font, text, x, y, color)` 的 `y` 是文字基线位置（不是左上角）。

**理由**: EpdFont 的 glyph.top 是相对于基线的偏移，用基线坐标最自然。后续 TextLabel 可以用 `y = topPadding + font->ascender` 定位基线。

## Risks / Trade-offs

**[逐像素性能]** → fillRect 对水平逻辑行仍是逐像素操作（因坐标旋转导致物理内存不连续）。对于 540px 宽的 header 填充约需 540×48=25920 次 `setPixel` 调用。
→ **缓解**: 实际测量旧框架已能正常运行，性能足够。后续可添加 `fillRowFast` 优化热路径。

**[glyph 解压内存分配]** → 每次绘制压缩字符需 malloc/free 解压缓冲区。
→ **缓解**: 与旧实现行为一致，已验证可行。后续可考虑复用缓冲区。
