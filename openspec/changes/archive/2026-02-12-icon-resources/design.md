## Context

当前 ui_core 框架有完整的绘图原语和文本渲染能力，但缺少图标资源。Wireframe spec 定义的 8 个页面中，header、toolbar、设置项等区域需要约 8 个图标。现有的 `ui_canvas_draw_bitmap` 函数将位图数据直接写入 framebuffer，无法对图标着色（例如白底上画黑图标，黑底上画白图标），需要新增一个支持前景色的 bitmap 绘制函数。

## Goals / Non-Goals

**Goals:**
- 建立从 SVG 源文件到嵌入式 C 数组的完整转换 pipeline
- 提供统一的图标数据结构和绘制 API
- 首批导入 wireframe spec 所需的全部核心图标
- 支持以任意灰度色绘制图标（前景色参数）

**Non-Goals:**
- 不实现运行时 SVG 解析或 PNG 解码
- 不实现图标缓存或懒加载（图标数据量极小，直接编译进 Flash）
- 不实现多尺寸图标（统一 32×32）
- 不实现 widget 组件层（属于后续 `widget-components` change）

## Decisions

### 决策 1: 图标源 — Tabler Icons

**选择:** Tabler Icons (MIT 许可, 5900+ 图标, 24×24 网格, 2px 描边)

**备选方案:**
- Lucide Icons (ISC 许可): 数量较少 (~1500)，风格类似
- Pixel Art Icons: 像素风与阅读器气质不匹配
- 自绘像素图标: 工作量大，一致性难以保证

**理由:** MIT 许可最宽松；2px 线条描边在 E-Ink 16 级灰度下清晰可辨；图标数量最全，后续扩展无忧。

### 决策 2: 图标尺寸 — 32×32 像素

**选择:** 统一 32×32 像素，4bpp 灰度

**备选方案:**
- 24×24: Tabler 原生尺寸，但在 235PPI 屏幕上偏小 (~2.6mm)
- 48×48: 触摸友好但占用过多空间

**理由:** 32px 在 235PPI 下约 3.5mm，视觉大小适中。Wireframe spec 中图标按钮的触摸区为 48×48，32px 图标居中放置留有 8px 周边间距。每个图标 512 bytes (32×32÷2)，存储开销可忽略。

### 决策 3: 存储格式 — 4bpp alpha bitmap

**选择:** 图标数据存储为 4bpp alpha 值（0x00=透明, 0xF0=不透明），绘制时与指定前景色混合

**备选方案:**
- 1bpp 单色位图: 节省空间但无抗锯齿，线条图标会有锯齿
- 直接灰度值: 无法在不同底色上着色

**理由:** SVG 渲染后天然产生抗锯齿 alpha 值。4bpp 提供 16 级 alpha，与 E-Ink 16 级灰度完美匹配。存储开销增加 4 倍（vs 1bpp）但总量仍可忽略。

### 决策 4: 转换工具 — Python 脚本

**选择:** Python 3 脚本使用 cairosvg + Pillow 渲染 SVG 并输出 C 数组

**流程:**
1. 从 Tabler Icons repo 下载所需 SVG 到 `tools/icons/src/`
2. `convert_icons.py` 批量转换：SVG → 32×32 灰度 PNG → 量化到 4bpp → C 数组
3. 输出 `ui_icon_data.h`（自动生成，包含所有图标的 `const uint8_t` 数组）

**理由:** cairosvg 渲染质量好，Pillow 处理图像简单可靠。脚本化保证可重复，后续添加新图标只需放入 SVG 并重新运行。

### 决策 5: 着色绘制函数 — `ui_canvas_draw_bitmap_fg`

**选择:** 新增函数而非修改现有 `draw_bitmap`

```c
void ui_canvas_draw_bitmap_fg(uint8_t *fb, int x, int y, int w, int h,
                               const uint8_t *bitmap, uint8_t fg_color);
```

**语义:** bitmap 中每个 4bpp 值作为 alpha（不透明度）。alpha=0xF0 时写入 fg_color，alpha=0x00 时保留 framebuffer 原值，中间值线性插值。

**理由:** 保持 `draw_bitmap` 向后兼容（直接写入灰度值）。着色函数语义不同，独立函数更清晰。

## Risks / Trade-offs

- **[cairosvg 依赖]** → 仅开发时需要，不影响固件编译。可在 README 中说明安装方式。若环境不便安装，可直接提交生成后的 `ui_icon_data.h`。
- **[SVG 渲染差异]** → 不同 cairosvg 版本可能导致细微像素差异。将生成的 .h 文件纳入版本控制，确保一致性。
- **[32×32 不够灵活]** → 若后续需要其他尺寸，可在脚本中添加 `--size` 参数。当前统一尺寸足够。
