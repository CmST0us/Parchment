## 1. 字体资源准备

- [x] 1.1 复制 `components/epdiy/scripts/fontconvert.py` 到 `tools/fontconvert.py`，将 DPI 从 150 改为 72
- [x] 1.2 创建字体生成脚本 `tools/generate_fonts.sh`，调用 fontconvert.py 为 6 个字号（20/24/28/32/36/40）生成 .h 文件，使用 `--compress --string` 参数传入歌词文本字符集
- [x] 1.3 下载 Noto Sans CJK SC Regular 字体文件，运行生成脚本，将输出的 .h 文件放入 `components/ui_core/fonts/`
- [x] 1.4 创建 `components/ui_core/include/ui_font.h`，声明 6 个字体的 `extern const EpdFont` 引用和 `ui_font_get(int size_px)` 函数
- [x] 1.5 创建 `components/ui_core/ui_font.c`，实现 `ui_font_get()` 按像素大小匹配最近字号（向下取整，边界钳位到 20/40）

## 2. 基础文字渲染

- [x] 2.1 在 `ui_canvas.c` 中实现 UTF-8 解码函数（`utf8_len` + `next_codepoint`），参考 epdiy `font.c` 逻辑
- [x] 2.2 在 `ui_canvas.c` 中实现 `draw_char_logical()`：解压 glyph bitmap，逐像素通过 `fb_set_pixel()` 写入逻辑坐标，实现灰度 alpha 混合（`bg + alpha * (fg - bg) / 15`）
- [x] 2.3 在 `ui_canvas.c` 中实现 `ui_canvas_draw_text()`：遍历 UTF-8 字符串调用 `draw_char_logical()`，遇到 `\n` 或字符串结束时停止
- [x] 2.4 在 `ui_canvas.c` 中实现 `ui_canvas_measure_text()`：遍历字符累加 `advance_x`，不写入 framebuffer
- [x] 2.5 更新 `ui_canvas.h`，追加 `ui_canvas_draw_text()` 和 `ui_canvas_measure_text()` 的声明（需 `#include "epdiy.h"` 引入 `EpdFont`）

## 3. 文本排版引擎

- [x] 3.1 创建 `components/ui_core/include/ui_text.h`，定义 `ui_text_result_t` 结构和 `ui_canvas_draw_text_wrapped()` / `ui_canvas_draw_text_page()` 函数声明
- [x] 3.2 创建 `components/ui_core/ui_text.c`，实现 CJK 字符判断辅助函数（`is_cjk_char`、`is_line_start_forbidden`、`is_line_end_forbidden`）
- [x] 3.3 实现断行算法：扫描字符计算宽度，CJK 逐字断行 + 英文单词整体断行 + 标点禁则处理
- [x] 3.4 实现 `ui_canvas_draw_text_wrapped()`：在 max_width 内自动换行渲染，返回 `ui_text_result_t`
- [x] 3.5 实现 `ui_canvas_draw_text_page()`：从 start_offset 开始渲染，到 max_height 或文本末尾停止，返回 `ui_text_result_t`（含 `bytes_consumed` 供翻页使用）

## 4. 构建集成

- [x] 4.1 更新 `components/ui_core/CMakeLists.txt`：添加 `ui_font.c`、`ui_text.c` 到 SRCS，确保 `fonts/` 目录在 include 路径中
- [x] 4.2 确认 CMakeLists.txt 的 REQUIRES 包含 `epdiy`（引用 `EpdFont` 结构和 `epd_get_glyph()`）

## 5. 集成验证

- [x] 5.1 在 `main.c` 中编写临时验证页面：白屏背景 + 用 `ui_canvas_draw_text()` 渲染 6 个字号的单行文字
- [x] 5.2 在验证页面中用 `ui_canvas_draw_text_page()` 渲染歌词全文，验证自动换行和分页效果
- [x] 5.3 `idf.py build` 编译通过，Flash 占用在预期范围内（~750KB 字体数据）
