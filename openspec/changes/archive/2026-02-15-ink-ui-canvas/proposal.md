## Why

InkUI 框架重构的阶段 1 需要 Canvas 绘图引擎。旧 `ui_canvas.c` 是无状态的全局函数集，没有裁剪区域概念——所有绘图直接写 framebuffer，越界靠调用方手动检查。新 View 树架构要求每个 View 的 `onDraw()` 拿到一个自带裁剪区域的 Canvas 对象，绘图自动限制在 View 边界内，坐标相对于 View 左上角。这是 RenderEngine 局部重绘的基础。

## What Changes

- 新增 `ink::Canvas` 类：带裁剪区域的绘图引擎，所有坐标为相对于 clip 左上角的局部坐标
- 支持 `clipped()` 创建子 Canvas（裁剪区域取交集），用于嵌套 View 绘制
- 迁移 `ui_canvas.c` 的全部绘图原语：fillRect, drawRect, hline, vline, line, bitmap, bitmap_fg
- 迁移文字渲染：drawText, drawTextN, measureText（直接使用 `EpdFont*`，不依赖 FontManager）
- 从 `ui_canvas.c` 搬运坐标变换逻辑（逻辑 540×960 → 物理 960×540）和 UTF-8 解码
- 在 `main.cpp` 中用临时测试代码验证 Canvas 功能

## Capabilities

### New Capabilities
- `ink-canvas`: InkUI Canvas 裁剪绘图引擎，包含几何图形、位图和文字渲染

### Modified Capabilities
（无——新 Canvas 是独立的新组件，不修改旧 `ui_canvas` 的行为）

## Impact

- 新增文件: `components/ink_ui/include/ink_ui/core/Canvas.h`, `components/ink_ui/src/core/Canvas.cpp`
- 修改文件: `components/ink_ui/CMakeLists.txt`（添加 Canvas.cpp 源文件，添加 epdiy 依赖用于 miniz）
- 修改文件: `components/ink_ui/include/ink_ui/InkUI.h`（添加 Canvas.h include）
- 修改文件: `main/main.cpp`（临时测试代码验证 Canvas）
- 依赖: epdiy（EpdFont, epd_get_glyph, miniz）
