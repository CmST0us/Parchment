/**
 * @file ui_canvas.c
 * @brief Canvas 绘图实现。
 *
 * 逻辑坐标系为竖屏 540×960，通过转置映射到物理 framebuffer（960×540）。
 * 映射关系: physical_x = logical_y, physical_y = logical_x
 *
 * framebuffer 布局 (物理):
 *   buf[py * 480 + px/2]，偶数 px 在低 4 位，奇数 px 在高 4 位。
 */

#include "ui_canvas.h"
#include <string.h>

/** 物理 framebuffer 尺寸（横屏）。 */
#define FB_PHYS_WIDTH  960
#define FB_PHYS_HEIGHT 540

/**
 * @brief 在 framebuffer 上写入单个像素（含转置映射）。
 *
 * 逻辑 (lx, ly) → 物理 (px=ly, py=lx)
 */
static inline void fb_set_pixel(uint8_t *fb, int lx, int ly, uint8_t gray) {
    int px = ly;
    int py = lx;
    uint8_t *buf_ptr = &fb[py * (FB_PHYS_WIDTH / 2) + px / 2];
    if (px % 2) {
        *buf_ptr = (*buf_ptr & 0x0F) | (gray & 0xF0);
    } else {
        *buf_ptr = (*buf_ptr & 0xF0) | (gray >> 4);
    }
}

void ui_canvas_draw_pixel(uint8_t *fb, int x, int y, uint8_t gray) {
    if (fb == NULL || x < 0 || x >= UI_SCREEN_WIDTH || y < 0 || y >= UI_SCREEN_HEIGHT) {
        return;
    }
    fb_set_pixel(fb, x, y, gray);
}

void ui_canvas_fill(uint8_t *fb, uint8_t gray) {
    if (fb == NULL) {
        return;
    }
    uint8_t byte_val = (gray & 0xF0) | (gray >> 4);
    memset(fb, byte_val, FB_PHYS_WIDTH * FB_PHYS_HEIGHT / 2);
}

void ui_canvas_fill_rect(uint8_t *fb, int x, int y, int w, int h, uint8_t gray) {
    if (fb == NULL || w <= 0 || h <= 0) {
        return;
    }

    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;

    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > UI_SCREEN_WIDTH) x1 = UI_SCREEN_WIDTH;
    if (y1 > UI_SCREEN_HEIGHT) y1 = UI_SCREEN_HEIGHT;

    if (x0 >= x1 || y0 >= y1) {
        return;
    }

    for (int iy = y0; iy < y1; iy++) {
        for (int ix = x0; ix < x1; ix++) {
            fb_set_pixel(fb, ix, iy, gray);
        }
    }
}

void ui_canvas_draw_rect(uint8_t *fb, int x, int y, int w, int h, uint8_t gray, int thickness) {
    if (fb == NULL || w <= 0 || h <= 0 || thickness <= 0) {
        return;
    }

    ui_canvas_fill_rect(fb, x, y, w, thickness, gray);
    ui_canvas_fill_rect(fb, x, y + h - thickness, w, thickness, gray);
    ui_canvas_fill_rect(fb, x, y + thickness, thickness, h - 2 * thickness, gray);
    ui_canvas_fill_rect(fb, x + w - thickness, y + thickness, thickness, h - 2 * thickness, gray);
}

void ui_canvas_draw_hline(uint8_t *fb, int x, int y, int w, uint8_t gray) {
    ui_canvas_fill_rect(fb, x, y, w, 1, gray);
}

void ui_canvas_draw_vline(uint8_t *fb, int x, int y, int h, uint8_t gray) {
    ui_canvas_fill_rect(fb, x, y, 1, h, gray);
}

void ui_canvas_draw_bitmap(uint8_t *fb, int x, int y, int w, int h, const uint8_t *bitmap) {
    if (fb == NULL || bitmap == NULL || w <= 0 || h <= 0) {
        return;
    }

    for (int by = 0; by < h; by++) {
        int dy = y + by;
        if (dy < 0 || dy >= UI_SCREEN_HEIGHT) {
            continue;
        }

        for (int bx = 0; bx < w; bx++) {
            int dx = x + bx;
            if (dx < 0 || dx >= UI_SCREEN_WIDTH) {
                continue;
            }

            int bmp_idx = by * w + bx;
            uint8_t bmp_byte = bitmap[bmp_idx / 2];
            uint8_t gray_val;
            if (bmp_idx % 2) {
                gray_val = bmp_byte & 0xF0;
            } else {
                gray_val = (bmp_byte & 0x0F) << 4;
            }

            fb_set_pixel(fb, dx, dy, gray_val);
        }
    }
}
