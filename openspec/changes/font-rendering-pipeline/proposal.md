## Why

阅读器的所有界面（书库、阅读、设置、目录）都依赖文字渲染，但当前 `ui_canvas` 只提供几何图形绘制原语，没有任何字体渲染能力。字体渲染管线是构建正式 UI 的前置基础，必须先于页面开发完成。

## What Changes

- 新增 **binary font (.bin) 渲染引擎**，支持从 SD 卡加载预渲染的 1-bit bitmap 字体文件
- 新增 **字体 API**：加载/卸载字体、渲染单字符、渲染字符串（自动换行）、文本测量
- 新增 **Python 字体生成工具**，改造自 M5ReadPaper 项目的 `generate_1bit_font_bin.py`，将 TTF/OTF 字体预渲染为 .bin 格式
- 支持 **4 档预设字号**：24/28/32/36 像素
- 支持 **UTF-8 编码**的中英文混排文本渲染
- 移除 `main.c` 中的交互测试页面，替换为字体渲染验证页面

## Capabilities

### New Capabilities

- `ui-font`: 字体加载、glyph 查找、bitmap 解码、单字符/字符串渲染、文本测量，以及 UTF-8 解析
- `font-tool`: Python 字体生成工具，将 TTF/OTF 转换为设备可用的 .bin 格式

### Modified Capabilities

- `ui-canvas`: 新增 `ui_canvas_draw_bitmap_1bit` 函数，支持将 1-bit packed bitmap 渲染到 4bpp framebuffer

## Impact

- **新增文件**: `components/ui_core/ui_font.h`, `components/ui_core/ui_font.c`
- **新增工具**: `tools/generate_font_bin.py`
- **修改文件**: `components/ui_core/ui_canvas.h`, `components/ui_core/ui_canvas.c`（新增 1-bit bitmap 绘制）
- **修改文件**: `main/main.c`（测试页面替换）
- **依赖**: SD 卡文件系统（已有）、PSRAM 用于 glyph 索引缓存
- **SD 卡内容**: `/sdcard/fonts/` 目录下的 .bin 字体文件
