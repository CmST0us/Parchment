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

#include <stdlib.h>
#include <string.h>

#include "epdiy.h"
#include <miniz.h>

/** 物理 framebuffer 尺寸（横屏）。 */
#define FB_PHYS_WIDTH  960
#define FB_PHYS_HEIGHT 540

/**
 * @brief 在 framebuffer 上写入单个像素（含转置+水平翻转映射）。
 *
 * 逻辑 (lx, ly) → 物理 (px=ly, py=539-lx)
 */
static inline void fb_set_pixel(uint8_t *fb, int lx, int ly, uint8_t gray) {
    int px = ly;
    int py = (FB_PHYS_HEIGHT - 1) - lx;
    uint8_t *buf_ptr = &fb[py * (FB_PHYS_WIDTH / 2) + px / 2];
    if (px % 2) {
        *buf_ptr = (*buf_ptr & 0x0F) | (gray & 0xF0);
    } else {
        *buf_ptr = (*buf_ptr & 0xF0) | (gray >> 4);
    }
}

/**
 * @brief 从 framebuffer 读取单个像素的灰度值（含转置+水平翻转映射）。
 */
static inline uint8_t fb_get_pixel(const uint8_t *fb, int lx, int ly) {
    int px = ly;
    int py = (FB_PHYS_HEIGHT - 1) - lx;
    uint8_t byte_val = fb[py * (FB_PHYS_WIDTH / 2) + px / 2];
    if (px % 2) {
        return byte_val & 0xF0;
    } else {
        return (byte_val & 0x0F) << 4;
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

void ui_canvas_draw_line(uint8_t *fb, int x0, int y0, int x1, int y1, uint8_t gray) {
    if (fb == NULL) {
        return;
    }

    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        if (x0 >= 0 && x0 < UI_SCREEN_WIDTH && y0 >= 0 && y0 < UI_SCREEN_HEIGHT) {
            fb_set_pixel(fb, x0, y0, gray);
        }
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
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

void ui_canvas_draw_bitmap_fg(uint8_t *fb, int x, int y, int w, int h,
                               const uint8_t *bitmap, uint8_t fg_color) {
    if (fb == NULL || bitmap == NULL || w <= 0 || h <= 0) {
        return;
    }

    uint8_t fg4 = fg_color >> 4;  /* 0-15 */

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

            /* 提取 4bpp alpha 值 */
            int bmp_idx = by * w + bx;
            uint8_t bmp_byte = bitmap[bmp_idx / 2];
            uint8_t alpha;
            if (bmp_idx % 2) {
                alpha = bmp_byte >> 4;
            } else {
                alpha = bmp_byte & 0x0F;
            }

            if (alpha == 0) {
                continue;  /* 完全透明 */
            }

            if (alpha == 0x0F) {
                /* 完全不透明，直接写入前景色 */
                fb_set_pixel(fb, dx, dy, fg_color);
            } else {
                /* 线性插值: result = bg + alpha * (fg - bg) / 15 */
                uint8_t bg4 = fb_get_pixel(fb, dx, dy) >> 4;
                uint8_t result4 = (uint8_t)(bg4 + (int)(alpha * ((int)fg4 - (int)bg4)) / 15);
                fb_set_pixel(fb, dx, dy, result4 << 4);
            }
        }
    }
}

/* ── UTF-8 解码 ── */

/**
 * @brief 返回 UTF-8 编码的首字节对应的字节长度。
 */
static int utf8_byte_len(uint8_t ch) {
    if ((ch & 0x80) == 0x00) return 1;       /* 0xxxxxxx */
    if ((ch & 0xE0) == 0xC0) return 2;       /* 110xxxxx */
    if ((ch & 0xF0) == 0xE0) return 3;       /* 1110xxxx */
    if ((ch & 0xF8) == 0xF0) return 4;       /* 11110xxx */
    return 1; /* malformed, skip one byte */
}

/**
 * @brief 从 UTF-8 字符串中读取下一个 code point，并前进指针。
 *
 * @param str 指向字符串指针的指针，调用后会前进到下一个字符。
 * @return Unicode code point，字符串结束时返回 0。
 */
static uint32_t next_codepoint(const char **str) {
    const uint8_t *s = (const uint8_t *)*str;
    if (*s == 0) return 0;

    int len = utf8_byte_len(*s);

    /* 验证续字节存在（防止截断字符串越界读取） */
    for (int i = 1; i < len; i++) {
        if (s[i] == 0) {
            *str += 1;
            return 0xFFFD;
        }
    }

    uint32_t cp = 0;
    switch (len) {
        case 1: cp = s[0]; break;
        case 2: cp = (s[0] & 0x1F) << 6  | (s[1] & 0x3F); break;
        case 3: cp = (s[0] & 0x0F) << 12 | (s[1] & 0x3F) << 6  | (s[2] & 0x3F); break;
        case 4: cp = (s[0] & 0x07) << 18 | (s[1] & 0x3F) << 12 | (s[2] & 0x3F) << 6 | (s[3] & 0x3F); break;
        default: cp = 0xFFFD; break;
    }

    *str += len;
    return cp;
}

/* ── 字符级渲染 ── */

/**
 * @brief 解压 glyph bitmap 数据（zlib 压缩）。
 *
 * @return 解压后的 bitmap 指针（调用方负责 free），失败返回 NULL。
 */
static uint8_t *decompress_glyph(const EpdFont *font, const EpdGlyph *glyph,
                                  size_t bitmap_size) {
    uint8_t *buf = (uint8_t *)malloc(bitmap_size);
    if (!buf) return NULL;

    tinfl_decompressor *decomp = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
    if (!decomp) {
        free(buf);
        return NULL;
    }
    tinfl_init(decomp);

    size_t src_size = glyph->compressed_size;
    size_t out_size = bitmap_size;
    tinfl_status status = tinfl_decompress(
        decomp,
        &font->bitmap[glyph->data_offset],
        &src_size,
        buf, buf, &out_size,
        TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF
    );
    free(decomp);

    if (status != TINFL_STATUS_DONE) {
        free(buf);
        return NULL;
    }
    return buf;
}

/**
 * @brief 在逻辑坐标系下绘制单个字符，前进 cursor_x。
 *
 * @param fb       framebuffer 指针。
 * @param font     字体。
 * @param cursor_x 当前 X 光标（会被 advance_x 前进）。
 * @param cursor_y 基线 Y 坐标。
 * @param cp       Unicode code point。
 * @param fg       前景色（高 4 位有效）。
 */
static void draw_char_logical(uint8_t *fb, const EpdFont *font,
                               int *cursor_x, int cursor_y,
                               uint32_t cp, uint8_t fg) {
    const EpdGlyph *glyph = epd_get_glyph(font, cp);
    if (!glyph) return;

    uint16_t w = glyph->width;
    uint16_t h = glyph->height;
    int byte_width = (w / 2 + w % 2);
    size_t bitmap_size = (size_t)byte_width * h;

    const uint8_t *bitmap = NULL;
    bool need_free = false;

    if (bitmap_size > 0) {
        if (font->compressed) {
            bitmap = decompress_glyph(font, glyph, bitmap_size);
            if (!bitmap) {
                *cursor_x += glyph->advance_x;
                return;
            }
            need_free = true;
        } else {
            bitmap = &font->bitmap[glyph->data_offset];
        }
    }

    /* 前景/背景色 LUT：alpha 0-15 映射到灰度值。 */
    uint8_t fg4 = fg >> 4;       /* 0-15 */
    uint8_t bg4 = 0x0F;          /* 白色背景 */

    for (int by = 0; by < h; by++) {
        int ly = cursor_y - glyph->top + by;
        if (ly < 0 || ly >= UI_SCREEN_HEIGHT) continue;

        for (int bx = 0; bx < w; bx++) {
            int lx = *cursor_x + glyph->left + bx;
            if (lx < 0 || lx >= UI_SCREEN_WIDTH) continue;

            /* 从 bitmap 中提取 4bpp alpha 值。 */
            uint8_t bm = bitmap[by * byte_width + bx / 2];
            uint8_t alpha;
            if ((bx & 1) == 0) {
                alpha = bm & 0x0F;
            } else {
                alpha = bm >> 4;
            }

            if (alpha == 0) continue;

            /* 线性插值: color = bg + alpha * (fg - bg) / 15 */
            uint8_t color4 = (uint8_t)(bg4 + (int)(alpha * ((int)fg4 - (int)bg4)) / 15);
            fb_set_pixel(fb, lx, ly, color4 << 4);
        }
    }

    if (need_free) {
        free((void *)bitmap);
    }
    *cursor_x += glyph->advance_x;
}

/* ── 公开 API ── */

void ui_canvas_draw_text(uint8_t *fb, int x, int y,
                         const EpdFont *font, const char *text,
                         uint8_t fg_color) {
    if (!fb || !font || !text || *text == '\0') return;

    int cursor_x = x;
    uint32_t cp;
    while ((cp = next_codepoint(&text)) != 0) {
        if (cp == '\n') break;
        draw_char_logical(fb, font, &cursor_x, y, cp, fg_color);
    }
}

int ui_canvas_measure_text(const EpdFont *font, const char *text) {
    if (!font || !text || *text == '\0') return 0;

    int width = 0;
    uint32_t cp;
    while ((cp = next_codepoint(&text)) != 0) {
        if (cp == '\n') break;
        const EpdGlyph *glyph = epd_get_glyph(font, cp);
        if (glyph) {
            width += glyph->advance_x;
        }
    }
    return width;
}

void ui_canvas_draw_text_n(uint8_t *fb, int x, int y,
                           const EpdFont *font, const char *text,
                           int max_bytes, uint8_t fg_color) {
    if (!fb || !font || !text || max_bytes <= 0) return;

    const char *end = text + max_bytes;
    int cursor_x = x;
    while (text < end && *text != '\0') {
        /* 检查剩余字节是否足够容纳此字符 */
        int char_len = utf8_byte_len((uint8_t)*text);
        if (text + char_len > end) break;
        uint32_t cp = next_codepoint(&text);
        if (cp == 0 || cp == '\n') break;
        draw_char_logical(fb, font, &cursor_x, y, cp, fg_color);
    }
}
