## Why

ui_core 框架目前只有几何绘图原语（pixel、line、rect、fill、bitmap），没有任何文字渲染能力。阅读器的核心功能——显示文本内容——完全无法实现。需要为 ui_canvas 增加文字渲染引擎，支持中日英混排、自动换行和分页，为后续所有 UI 页面和阅读视图提供基础。

## What Changes

- 新增自研字体渲染器，复用 epdiy 的 `EpdFont`/`EpdGlyph` 数据结构，但在逻辑坐标系下直接渲染（不依赖 epdiy 的绘图函数）
- 使用 epdiy 的 `fontconvert.py` 将 Noto Sans CJK SC 字体预编译为 C header，包含 6 个字号（20/24/28/32/36/40px）
- 新增文本排版引擎，支持 CJK 逐字断行、英文单词断行、标点禁则（Kinsoku Shori）和分页
- 扩展 `ui_canvas.h` API，增加文字绘制和度量函数
- 新增 `ui_text` 模块，提供自动换行和分页渲染 API

## Capabilities

### New Capabilities
- `font-assets`: 预编译字体资源管理（fontconvert 生成的 .h 文件，字号选择）
- `text-rendering`: 基础文字渲染（单字符/单行绘制、文字度量、逻辑坐标系映射）
- `text-layout`: 文本排版引擎（自动换行、CJK 断行规则、标点禁则、分页渲染）

### Modified Capabilities
- `ui-canvas`: 追加文字绘制 API 声明（`draw_text`、`measure_text`）

## Impact

- **新增文件**: `components/ui_core/fonts/*.h`（生成的字体数据）、`components/ui_core/include/ui_font.h`、`components/ui_core/include/ui_text.h`、`components/ui_core/ui_text.c`
- **修改文件**: `components/ui_core/include/ui_canvas.h`（追加 API）、`components/ui_core/ui_canvas.c`（追加实现）、`components/ui_core/CMakeLists.txt`（追加源文件和 include 路径）
- **依赖**: 引用 epdiy 的 `EpdFont`/`EpdGlyph`/`EpdUnicodeInterval` 结构定义和 `epd_get_glyph()` 函数；需要 Python 环境运行 `fontconvert.py`
- **资源**: Noto Sans CJK SC 字体文件（~16MB OTF，仅构建时需要，不上传到仓库）
- **Flash 占用**: 预估 ~750KB（312 唯一字符 × 6 字号，含 zlib 压缩）
