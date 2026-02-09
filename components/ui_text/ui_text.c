/**
 * @file ui_text.c
 * @brief Text rendering engine implementation.
 *
 * Uses stb_truetype for TrueType font parsing and glyph rasterization.
 * Includes a direct-mapped glyph cache for performance, UTF-8 decoding,
 * and alpha blending onto the 4bpp grayscale framebuffer.
 */

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb/stb_truetype.h"

#include "ui_text.h"
#include "ui_canvas.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ========================================================================== */
/*  Constants                                                                  */
/* ========================================================================== */

/** Glyph cache size (must be power of 2). */
#define GLYPH_CACHE_SIZE 512
#define GLYPH_CACHE_MASK (GLYPH_CACHE_SIZE - 1)

/** Physical framebuffer dimensions. */
#define FB_PHYS_WIDTH  960
#define FB_PHYS_HEIGHT 540

/* ========================================================================== */
/*  Glyph cache                                                                */
/* ========================================================================== */

/** Cached glyph entry. */
typedef struct {
    uint32_t codepoint;     /**< Unicode codepoint (0 = empty slot) */
    uint8_t *bitmap;        /**< 8bpp alpha bitmap (heap-allocated) */
    int      width;         /**< Bitmap width */
    int      height;        /**< Bitmap height */
    int      x_offset;      /**< Horizontal offset from pen position */
    int      y_offset;      /**< Vertical offset from pen position (from ascent line) */
    int      advance;       /**< Horizontal advance width (pixels, rounded) */
} glyph_entry_t;

/** Font structure (opaque to callers). */
struct ui_font {
    stbtt_fontinfo info;
    float          scale;       /**< Pixels-per-em scale factor */
    int            ascent;      /**< Scaled ascent (pixels, positive) */
    int            descent;     /**< Scaled descent (pixels, negative) */
    int            line_gap;    /**< Scaled line gap (pixels) */
    int            line_height; /**< ascent - descent + line_gap */
    uint8_t       *owned_data;  /**< TTF data owned by this font (from load_file), or NULL */

    glyph_entry_t  cache[GLYPH_CACHE_SIZE];
};

/* ========================================================================== */
/*  UTF-8 decoder                                                              */
/* ========================================================================== */

/**
 * @brief Decode one UTF-8 codepoint from a string.
 *
 * @param[in]  text  Pointer to current position in the string.
 * @param[out] cp    Decoded Unicode codepoint.
 * @return Number of bytes consumed (1-4), or 0 if at end-of-string/invalid.
 */
static int utf8_decode(const char *text, uint32_t *cp) {
    const uint8_t *s = (const uint8_t *)text;

    if (s[0] == 0) {
        *cp = 0;
        return 0;
    }

    if (s[0] < 0x80) {
        /* 1-byte: 0xxxxxxx */
        *cp = s[0];
        return 1;
    }

    if ((s[0] & 0xE0) == 0xC0) {
        /* 2-byte: 110xxxxx 10xxxxxx */
        if ((s[1] & 0xC0) != 0x80) { *cp = 0xFFFD; return 1; }
        *cp = ((uint32_t)(s[0] & 0x1F) << 6) |
               (uint32_t)(s[1] & 0x3F);
        return 2;
    }

    if ((s[0] & 0xF0) == 0xE0) {
        /* 3-byte: 1110xxxx 10xxxxxx 10xxxxxx */
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) { *cp = 0xFFFD; return 1; }
        *cp = ((uint32_t)(s[0] & 0x0F) << 12) |
              ((uint32_t)(s[1] & 0x3F) << 6) |
               (uint32_t)(s[2] & 0x3F);
        return 3;
    }

    if ((s[0] & 0xF8) == 0xF0) {
        /* 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
        if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) {
            *cp = 0xFFFD; return 1;
        }
        *cp = ((uint32_t)(s[0] & 0x07) << 18) |
              ((uint32_t)(s[1] & 0x3F) << 12) |
              ((uint32_t)(s[2] & 0x3F) << 6) |
               (uint32_t)(s[3] & 0x3F);
        return 4;
    }

    /* Invalid lead byte */
    *cp = 0xFFFD;
    return 1;
}

/* ========================================================================== */
/*  Framebuffer pixel read/write with transpose mapping                        */
/* ========================================================================== */

/**
 * @brief Read a pixel from the framebuffer (logical coordinates).
 *
 * Returns 8-bit grayscale (upper 4 bits significant).
 */
static inline uint8_t fb_read_pixel(const uint8_t *fb, int lx, int ly) {
    int px = ly;
    int py = lx;
    uint8_t byte_val = fb[py * (FB_PHYS_WIDTH / 2) + px / 2];
    return (px & 1) ? (byte_val & 0xF0) : ((byte_val & 0x0F) << 4);
}

/**
 * @brief Write a pixel to the framebuffer (logical coordinates).
 *
 * gray is 8-bit, upper 4 bits significant.
 */
static inline void fb_write_pixel(uint8_t *fb, int lx, int ly, uint8_t gray) {
    int px = ly;
    int py = lx;
    uint8_t *p = &fb[py * (FB_PHYS_WIDTH / 2) + px / 2];
    if (px & 1) {
        *p = (*p & 0x0F) | (gray & 0xF0);
    } else {
        *p = (*p & 0xF0) | (gray >> 4);
    }
}

/* ========================================================================== */
/*  Glyph cache lookup / rasterize                                             */
/* ========================================================================== */

/**
 * @brief Get a cached glyph, rasterizing it if not present.
 *
 * Uses a direct-mapped cache: hash = codepoint & mask.
 * On collision, the old entry is evicted and the new glyph is rasterized.
 */
static const glyph_entry_t *font_get_glyph(ui_font_t *font, uint32_t codepoint) {
    int slot = (int)(codepoint & GLYPH_CACHE_MASK);
    glyph_entry_t *e = &font->cache[slot];

    if (e->codepoint == codepoint && e->codepoint != 0) {
        return e; /* Cache hit */
    }

    /* Cache miss: evict old entry */
    if (e->bitmap) {
        free(e->bitmap);
        e->bitmap = NULL;
    }

    e->codepoint = codepoint;

    /* Get glyph index */
    int glyph_idx = stbtt_FindGlyphIndex(&font->info, (int)codepoint);

    /* Get horizontal metrics */
    int advance_w, lsb;
    stbtt_GetGlyphHMetrics(&font->info, glyph_idx, &advance_w, &lsb);
    e->advance = (int)(advance_w * font->scale + 0.5f);

    /* Rasterize the glyph */
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(&font->info, glyph_idx, font->scale, font->scale,
                            &x0, &y0, &x1, &y1);

    int w = x1 - x0;
    int h = y1 - y0;

    if (w > 0 && h > 0) {
        e->bitmap = (uint8_t *)malloc(w * h);
        if (e->bitmap) {
            stbtt_MakeGlyphBitmap(&font->info, e->bitmap, w, h, w,
                                  font->scale, font->scale, glyph_idx);
        }
        e->width    = w;
        e->height   = h;
        e->x_offset = x0;
        e->y_offset = y0;
    } else {
        e->bitmap   = NULL;
        e->width    = 0;
        e->height   = 0;
        e->x_offset = 0;
        e->y_offset = 0;
    }

    return e;
}

/**
 * @brief Get advance width for a codepoint (without caching bitmap if not needed).
 *
 * For measurement we still use the cache since it stores advance.
 */
static int font_get_advance(ui_font_t *font, uint32_t codepoint) {
    const glyph_entry_t *g = font_get_glyph(font, codepoint);
    return g ? g->advance : 0;
}

/* ========================================================================== */
/*  Font lifecycle                                                             */
/* ========================================================================== */

ui_font_t *ui_font_create(const uint8_t *ttf_data, float pixel_size) {
    if (!ttf_data || pixel_size <= 0) return NULL;

    ui_font_t *font = (ui_font_t *)calloc(1, sizeof(ui_font_t));
    if (!font) return NULL;

    if (!stbtt_InitFont(&font->info, ttf_data, stbtt_GetFontOffsetForIndex(ttf_data, 0))) {
        free(font);
        return NULL;
    }

    font->scale = stbtt_ScaleForPixelHeight(&font->info, pixel_size);

    int asc, desc, gap;
    stbtt_GetFontVMetrics(&font->info, &asc, &desc, &gap);

    font->ascent      = (int)(asc * font->scale + 0.5f);
    font->descent     = (int)(desc * font->scale - 0.5f); /* desc is negative */
    font->line_gap    = (int)(gap * font->scale + 0.5f);
    font->line_height = font->ascent - font->descent + font->line_gap;

    font->owned_data = NULL; /* Caller owns the TTF data */

    return font;
}

void ui_font_destroy(ui_font_t *font) {
    if (!font) return;

    /* Free cached glyph bitmaps */
    for (int i = 0; i < GLYPH_CACHE_SIZE; i++) {
        if (font->cache[i].bitmap) {
            free(font->cache[i].bitmap);
        }
    }

    /* Free owned TTF data (from ui_font_load_file) */
    if (font->owned_data) {
        free(font->owned_data);
    }

    free(font);
}

ui_font_t *ui_font_load_file(const char *path, float pixel_size) {
    if (!path) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fclose(f);
        return NULL;
    }

    uint8_t *data = (uint8_t *)malloc(size);
    if (!data) {
        fclose(f);
        return NULL;
    }

    if ((long)fread(data, 1, size, f) != size) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);

    ui_font_t *font = ui_font_create(data, pixel_size);
    if (!font) {
        free(data);
        return NULL;
    }

    font->owned_data = data; /* Font takes ownership */
    return font;
}

/* ========================================================================== */
/*  Font metrics                                                               */
/* ========================================================================== */

int ui_font_line_height(const ui_font_t *font) {
    return font ? font->line_height : 0;
}

int ui_font_ascent(const ui_font_t *font) {
    return font ? font->ascent : 0;
}

int ui_font_descent(const ui_font_t *font) {
    return font ? font->descent : 0;
}

/* ========================================================================== */
/*  Text measurement                                                           */
/* ========================================================================== */

int ui_text_measure_width(const ui_font_t *font, const char *text) {
    if (!font || !text) return 0;

    int width = 0;
    const char *p = text;
    uint32_t cp;
    int bytes;

    while ((bytes = utf8_decode(p, &cp)) > 0) {
        width += font_get_advance((ui_font_t *)font, cp);
        p += bytes;
    }

    return width;
}

int ui_text_fit_width(const ui_font_t *font, const char *text, int max_width) {
    if (!font || !text || max_width <= 0) return 0;

    int width = 0;
    const char *p = text;
    const char *last_fit = text;
    uint32_t cp;
    int bytes;

    while ((bytes = utf8_decode(p, &cp)) > 0) {
        int adv = font_get_advance((ui_font_t *)font, cp);
        if (width + adv > max_width) break;
        width += adv;
        p += bytes;
        last_fit = p;
    }

    return (int)(last_fit - text);
}

/* ========================================================================== */
/*  Text drawing                                                               */
/* ========================================================================== */

/**
 * @brief Render a single cached glyph onto the framebuffer with alpha blending.
 */
static void render_glyph(uint8_t *fb, const glyph_entry_t *g,
                          int pen_x, int pen_y, uint8_t color) {
    if (!g->bitmap || g->width <= 0 || g->height <= 0) return;

    /* Color as 8-bit value (only upper 4 bits matter, but keep precision for blending) */
    int fg = color | (color >> 4); /* e.g. 0x00→0x00, 0x30→0x33, 0xF0→0xFF */

    for (int gy = 0; gy < g->height; gy++) {
        int sy = pen_y + g->y_offset + gy;
        if (sy < 0 || sy >= UI_SCREEN_HEIGHT) continue;

        for (int gx = 0; gx < g->width; gx++) {
            int sx = pen_x + g->x_offset + gx;
            if (sx < 0 || sx >= UI_SCREEN_WIDTH) continue;

            uint8_t alpha = g->bitmap[gy * g->width + gx];
            if (alpha == 0) continue;

            if (alpha == 255) {
                fb_write_pixel(fb, sx, sy, color);
            } else {
                /* Alpha blend: out = fg * alpha + bg * (1 - alpha) */
                int bg = fb_read_pixel(fb, sx, sy);
                bg = bg | (bg >> 4); /* Expand 4-bit to 8-bit */

                int blended = (fg * alpha + bg * (255 - alpha)) / 255;

                /* Convert back to 4bpp (upper nibble) */
                fb_write_pixel(fb, sx, sy, (uint8_t)(blended & 0xF0));
            }
        }
    }
}

int ui_text_draw_line(uint8_t *fb, const ui_font_t *font,
                      int x, int y, const char *text, uint8_t color) {
    if (!fb || !font || !text) return 0;

    int pen_x = x;
    int pen_y = y + font->ascent; /* y parameter is top of text box, pen is at baseline */
    const char *p = text;
    uint32_t cp;
    int bytes;

    while ((bytes = utf8_decode(p, &cp)) > 0) {
        if (cp == '\n' || cp == '\r') break; /* Stop at newline */

        const glyph_entry_t *g = font_get_glyph((ui_font_t *)font, cp);
        if (g) {
            render_glyph(fb, g, pen_x, pen_y, color);
            pen_x += g->advance;
        }

        p += bytes;
    }

    return pen_x - x;
}

/* ========================================================================== */
/*  Line-wrapping helpers                                                      */
/* ========================================================================== */

/**
 * @brief Check if a codepoint is a CJK ideograph (can break anywhere).
 */
static bool is_cjk(uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF)   || /* CJK Unified Ideographs */
           (cp >= 0x3400 && cp <= 0x4DBF)   || /* CJK Extension A */
           (cp >= 0x3000 && cp <= 0x303F)   || /* CJK Symbols & Punctuation */
           (cp >= 0xFF00 && cp <= 0xFFEF)   || /* Fullwidth Forms */
           (cp >= 0x3040 && cp <= 0x309F)   || /* Hiragana */
           (cp >= 0x30A0 && cp <= 0x30FF);     /* Katakana */
}

/**
 * @brief Check if a codepoint is a breakable space.
 */
static bool is_space(uint32_t cp) {
    return cp == ' ' || cp == '\t';
}

/**
 * @brief Calculate one line of wrapped text.
 *
 * Returns the number of bytes consumed for this line, and the pixel width.
 * Wrapping rules:
 *   - CJK characters can break at any position.
 *   - Latin/other characters prefer breaking at spaces.
 *   - If a word is too long, break at character boundary.
 */
static int wrap_one_line(ui_font_t *font, const char *text, int max_width,
                          int *out_width) {
    const char *p = text;
    int width = 0;
    int last_break_bytes = 0; /* Last breakable position (byte offset) */
    int last_break_width = 0;
    bool has_break = false;
    uint32_t cp;
    int bytes;

    while ((bytes = utf8_decode(p, &cp)) > 0) {
        if (cp == '\n') {
            /* Explicit newline: consume it and end the line */
            *out_width = width;
            return (int)(p - text) + bytes;
        }

        if (cp == '\r') {
            /* Carriage return: skip, may be followed by \n */
            p += bytes;
            if (*p == '\n') p++;
            *out_width = width;
            return (int)(p - text);
        }

        int adv = font_get_advance(font, cp);

        /* Check if adding this character would exceed width */
        if (width + adv > max_width && width > 0) {
            /* Need to break */
            if (has_break) {
                /* Break at last breakable position */
                *out_width = last_break_width;
                return last_break_bytes;
            }
            /* No breakable position found: break here (mid-word) */
            *out_width = width;
            return (int)(p - text);
        }

        width += adv;
        p += bytes;

        /* Record breakable positions */
        if (is_space(cp)) {
            last_break_bytes = (int)(p - text);
            last_break_width = width;
            has_break = true;
        } else if (is_cjk(cp)) {
            /* After a CJK character is a valid break point */
            last_break_bytes = (int)(p - text);
            last_break_width = width;
            has_break = true;
        }
    }

    /* End of string */
    *out_width = width;
    return (int)(p - text);
}

int ui_text_draw_wrapped(uint8_t *fb, const ui_font_t *font,
                         int x, int y, int width, int max_height,
                         const char *text, uint8_t color,
                         ui_text_align_t align, int line_spacing) {
    if (!font || !text || width <= 0) return 0;

    int cur_y = y;
    const char *p = text;
    int lh = font->line_height + line_spacing;

    while (*p) {
        /* Check height limit */
        if (max_height > 0 && (cur_y - y + lh) > max_height) break;

        /* Calculate one wrapped line */
        int line_width;
        int consumed = wrap_one_line((ui_font_t *)font, p, width, &line_width);
        if (consumed <= 0) break;

        /* Calculate aligned X position */
        int draw_x = x;
        switch (align) {
        case UI_TEXT_ALIGN_CENTER:
            draw_x = x + (width - line_width) / 2;
            break;
        case UI_TEXT_ALIGN_RIGHT:
            draw_x = x + width - line_width;
            break;
        default:
            break;
        }

        /* Draw this line (only up to 'consumed' bytes) */
        if (fb) {
            int pen_x = draw_x;
            int pen_y = cur_y + font->ascent;
            const char *lp = p;
            const char *end = p + consumed;
            uint32_t cp;
            int bytes;

            while (lp < end && (bytes = utf8_decode(lp, &cp)) > 0) {
                if (cp == '\n' || cp == '\r') { lp += bytes; continue; }

                const glyph_entry_t *g = font_get_glyph((ui_font_t *)font, cp);
                if (g) {
                    render_glyph(fb, g, pen_x, pen_y, color);
                    pen_x += g->advance;
                }
                lp += bytes;
            }
        }

        p += consumed;
        cur_y += lh;

        /* Skip leading spaces on next line */
        while (*p == ' ') p++;
    }

    return cur_y - y;
}

int ui_text_paginate_ex(const ui_font_t *font, const char *text,
                        int width, int max_height, int line_spacing,
                        int *out_height) {
    if (!font || !text || width <= 0 || max_height <= 0) {
        if (out_height) *out_height = 0;
        return 0;
    }

    const char *p = text;
    int lh = font->line_height + line_spacing;
    int cur_h = 0;

    while (*p) {
        if (cur_h + lh > max_height) break;

        int line_width;
        int consumed = wrap_one_line((ui_font_t *)font, p, width, &line_width);
        if (consumed <= 0) break;

        p += consumed;
        cur_h += lh;

        while (*p == ' ') p++;
    }

    if (out_height) *out_height = cur_h;
    return (int)(p - text);
}

int ui_text_paginate(const ui_font_t *font, const char *text,
                     int width, int max_height, int line_spacing) {
    return ui_text_paginate_ex(font, text, width, max_height, line_spacing, NULL);
}
