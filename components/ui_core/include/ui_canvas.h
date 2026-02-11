/**
 * @file ui_canvas.h
 * @brief Canvas 绘图 API。
 *
 * 在 framebuffer 上提供竖屏坐标系（540×960）的基础绘图原语。
 * 内部负责逻辑坐标到物理 framebuffer 的旋转映射。
 */

#ifndef UI_CANVAS_H
#define UI_CANVAS_H

#include <stdint.h>

#include "epdiy.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 屏幕逻辑分辨率（竖屏）。 */
#define UI_SCREEN_WIDTH   540
#define UI_SCREEN_HEIGHT  960

/** 灰度调色板常量。 */
#define UI_COLOR_BLACK    0x00
#define UI_COLOR_DARK     0x30
#define UI_COLOR_MEDIUM   0x80
#define UI_COLOR_LIGHT    0xC0
#define UI_COLOR_WHITE    0xF0

/**
 * @brief 绘制单个像素。
 *
 * @param fb   framebuffer 指针。
 * @param x    逻辑 X 坐标 (0~539)。
 * @param y    逻辑 Y 坐标 (0~959)。
 * @param gray 灰度值，高 4 位有效。
 */
void ui_canvas_draw_pixel(uint8_t *fb, int x, int y, uint8_t gray);

/**
 * @brief 填充整个屏幕为指定灰度。
 *
 * @param fb   framebuffer 指针。
 * @param gray 灰度值，高 4 位有效。
 */
void ui_canvas_fill(uint8_t *fb, uint8_t gray);

/**
 * @brief 填充矩形区域。
 *
 * 超出屏幕范围的部分自动裁剪。
 *
 * @param fb   framebuffer 指针。
 * @param x    左上角 X 坐标。
 * @param y    左上角 Y 坐标。
 * @param w    宽度。
 * @param h    高度。
 * @param gray 灰度值。
 */
void ui_canvas_fill_rect(uint8_t *fb, int x, int y, int w, int h, uint8_t gray);

/**
 * @brief 绘制矩形边框（非填充）。
 *
 * @param fb        framebuffer 指针。
 * @param x         左上角 X 坐标。
 * @param y         左上角 Y 坐标。
 * @param w         宽度。
 * @param h         高度。
 * @param gray      灰度值。
 * @param thickness 边框粗细（像素）。
 */
void ui_canvas_draw_rect(uint8_t *fb, int x, int y, int w, int h, uint8_t gray, int thickness);

/**
 * @brief 绘制水平线。
 *
 * @param fb   framebuffer 指针。
 * @param x    起点 X 坐标。
 * @param y    Y 坐标。
 * @param w    长度。
 * @param gray 灰度值。
 */
void ui_canvas_draw_hline(uint8_t *fb, int x, int y, int w, uint8_t gray);

/**
 * @brief 绘制垂直线。
 *
 * @param fb   framebuffer 指针。
 * @param x    X 坐标。
 * @param y    起点 Y 坐标。
 * @param h    长度。
 * @param gray 灰度值。
 */
void ui_canvas_draw_vline(uint8_t *fb, int x, int y, int h, uint8_t gray);

/**
 * @brief 绘制任意方向直线（Bresenham 算法）。
 *
 * @param fb   framebuffer 指针。
 * @param x0   起点 X 坐标。
 * @param y0   起点 Y 坐标。
 * @param x1   终点 X 坐标。
 * @param y1   终点 Y 坐标。
 * @param gray 灰度值，高 4 位有效。
 */
void ui_canvas_draw_line(uint8_t *fb, int x0, int y0, int x1, int y1, uint8_t gray);

/**
 * @brief 绘制 4bpp 灰度位图。
 *
 * bitmap 格式与 framebuffer 一致：每字节 2 像素，高 4 位为左像素。
 *
 * @param fb     framebuffer 指针。
 * @param x      左上角 X 坐标。
 * @param y      左上角 Y 坐标。
 * @param w      位图宽度。
 * @param h      位图高度。
 * @param bitmap 位图数据。
 */
void ui_canvas_draw_bitmap(uint8_t *fb, int x, int y, int w, int h, const uint8_t *bitmap);

/**
 * @brief 以指定前景色绘制 4bpp alpha 位图。
 *
 * bitmap 中每个 4bpp 值作为 alpha（不透明度）：
 * - 0xF0 = 完全不透明，写入 fg_color
 * - 0x00 = 完全透明，保留 framebuffer 原值
 * - 中间值按线性插值混合
 *
 * @param fb       framebuffer 指针。
 * @param x        左上角 X 坐标。
 * @param y        左上角 Y 坐标。
 * @param w        位图宽度。
 * @param h        位图高度。
 * @param bitmap   4bpp alpha 位图数据。
 * @param fg_color 前景色（灰度值，高 4 位有效）。
 */
void ui_canvas_draw_bitmap_fg(uint8_t *fb, int x, int y, int w, int h,
                               const uint8_t *bitmap, uint8_t fg_color);

/**
 * @brief 绘制单行文字。
 *
 * 在逻辑坐标系下从 (x, y) 基线位置开始渲染一行文字。
 * 遇到 '\n' 或字符串结束时停止，不处理自动换行。
 *
 * @param fb       framebuffer 指针。
 * @param x        基线起始 X 坐标。
 * @param y        基线 Y 坐标。
 * @param font     字体指针（EpdFont）。
 * @param text     UTF-8 编码的文字字符串。
 * @param fg_color 前景色（灰度值，高 4 位有效）。
 */
void ui_canvas_draw_text(uint8_t *fb, int x, int y,
                         const EpdFont *font, const char *text,
                         uint8_t fg_color);

/**
 * @brief 度量文字渲染后的像素宽度。
 *
 * 遍历 UTF-8 字符串累加每个字符的 advance_x，不写入 framebuffer。
 * 遇到 '\n' 或字符串结束时停止。
 *
 * @param font 字体指针。
 * @param text UTF-8 编码的文字字符串。
 * @return 文字占用的像素宽度。
 */
int ui_canvas_measure_text(const EpdFont *font, const char *text);

/**
 * @brief 绘制指定字节数的文字（内部用于排版引擎）。
 *
 * 最多渲染 max_bytes 字节的文字，不跨越 UTF-8 字符边界。
 * 遇到 '\n'、'\0' 或字节限制时停止。
 *
 * @param fb        framebuffer 指针。
 * @param x         基线起始 X 坐标。
 * @param y         基线 Y 坐标。
 * @param font      字体指针。
 * @param text      UTF-8 编码文字。
 * @param max_bytes 最大渲染字节数。
 * @param fg_color  前景色。
 */
void ui_canvas_draw_text_n(uint8_t *fb, int x, int y,
                           const EpdFont *font, const char *text,
                           int max_bytes, uint8_t fg_color);

#ifdef __cplusplus
}
#endif

#endif /* UI_CANVAS_H */
