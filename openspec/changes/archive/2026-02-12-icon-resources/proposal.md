## Why

Wireframe spec (`docs/ui-wireframe-spec.md`) 定义了 8 个生产页面，其中 header、toolbar、设置等区域大量使用图标（返回箭头、菜单、齿轮、书签等）。当前 ui_core 框架只有 `draw_bitmap` 原语，缺少图标资源和统一的图标管理机制。需要在实现页面之前先建立图标 pipeline。

## What Changes

- 新增 Python 转换脚本，将 SVG 图标渲染为 32×32 4bpp 灰度 C 数组
- 从 Tabler Icons (MIT 许可) 获取约 8 个核心图标的 SVG 源文件
- 新增 `ui_icon` 模块，提供图标数据结构定义、预定义图标常量和带颜色参数的绘制函数
- 生成的图标数据以 `const` 数组形式编译到 Flash `.rodata` 段

## Capabilities

### New Capabilities
- `icon-pipeline`: SVG 到 4bpp C 数组的转换工具链，以及运行时图标数据结构与绘制 API

### Modified Capabilities
- `ui-canvas`: 新增 `ui_canvas_draw_bitmap_fg` 函数，支持以指定前景色绘制 4bpp alpha bitmap（现有 `draw_bitmap` 直接写入灰度值，不支持着色）

## Impact

- 新增文件: `tools/icons/convert_icons.py`, `tools/icons/src/*.svg`
- 新增文件: `components/ui_core/ui_icon.h`, `components/ui_core/ui_icon.c`
- 修改文件: `components/ui_core/ui_canvas.h`, `components/ui_core/ui_canvas.c` (新增 `draw_bitmap_fg`)
- 依赖: Python 3 + Pillow + cairosvg (开发时工具链，不影响固件)
- 存储开销: ~8 图标 × 512 bytes = ~4KB Flash
