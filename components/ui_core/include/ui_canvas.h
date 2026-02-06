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

#ifdef __cplusplus
}
#endif

#endif /* UI_CANVAS_H */
