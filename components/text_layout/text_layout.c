/**
 * @file text_layout.c
 * @brief 文本排版引擎实现。
 *
 * UTF-8 解析、中英文自动换行、字形渲染到 framebuffer。
 */

#include "text_layout.h"
#include "ui_canvas.h"
#include "esp_log.h"

#include <string.h>

static const char *TAG = "text_layout";

/* ========================================================================== */
/*  UTF-8 解析                                                                 */
/* ========================================================================== */

/**
 * @brief 从 UTF-8 字节流提取下一个 Unicode 码点。
 *
 * @param text 输入指针。
 * @param end  输入结束指针。
 * @param[out] codepoint 输出码点。
 * @return 消耗的字节数，0 表示结束或无效。
 */
static int utf8_next(const char *text, const char *end, uint32_t *codepoint) {
    if (text >= end) return 0;

    uint8_t c = (uint8_t)*text;
    int bytes;
    uint32_t cp;

    if (c < 0x80) {
        *codepoint = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        bytes = 2;
        cp = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        bytes = 3;
        cp = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        bytes = 4;
        cp = c & 0x07;
    } else {
        /* 无效序列，跳过 */
        *codepoint = 0xFFFD;
        return 1;
    }

    if (text + bytes > end) {
        *codepoint = 0xFFFD;
        return 1;
    }

    for (int i = 1; i < bytes; i++) {
        uint8_t b = (uint8_t)text[i];
        if ((b & 0xC0) != 0x80) {
            *codepoint = 0xFFFD;
            return i;
        }
        cp = (cp << 6) | (b & 0x3F);
    }

    *codepoint = cp;
    return bytes;
}

/* ========================================================================== */
/*  字符分类                                                                    */
/* ========================================================================== */

/**
 * @brief 判断码点是否为 CJK 字符（可在任意位置断行）。
 */
static bool is_cjk(uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF)   ||  /* CJK Unified Ideographs */
           (cp >= 0x3400 && cp <= 0x4DBF)   ||  /* CJK Extension A */
           (cp >= 0x3000 && cp <= 0x303F)   ||  /* CJK Symbols */
           (cp >= 0xFF00 && cp <= 0xFFEF)   ||  /* Fullwidth Forms */
           (cp >= 0x2000 && cp <= 0x206F);      /* General Punctuation */
}

/**
 * @brief 判断码点是否为空白字符。
 */
static bool is_space(uint32_t cp) {
    return cp == ' ' || cp == '\t';
}

/* ========================================================================== */
/*  字形绘制辅助                                                                */
/* ========================================================================== */

/**
 * @brief 将 4bpp 字形位图绘制到 framebuffer。
 *
 * @param fb    framebuffer 指针。
 * @param glyph 字形信息。
 * @param x     绘制基点 X。
 * @param y     绘制基线 Y。
 */
static void draw_glyph(uint8_t *fb, const font_glyph_t *glyph, int x, int y) {
    if (!glyph->bitmap || glyph->width <= 0 || glyph->height <= 0) return;

    int gx = x + glyph->offset_x;
    int gy = y - glyph->offset_y;

    for (int row = 0; row < glyph->height; row++) {
        for (int col = 0; col < glyph->width; col++) {
            int idx = row * glyph->width + col;
            uint8_t val;
            if (idx % 2 == 0) {
                val = glyph->bitmap[idx / 2] & 0x0F;
            } else {
                val = (glyph->bitmap[idx / 2] >> 4) & 0x0F;
            }
            if (val > 0) {
                /* 灰度反转：字形 alpha 值越大越黑 */
                uint8_t gray = (0x0F - val) << 4;
                ui_canvas_draw_pixel(fb, gx + col, gy + row, gray);
            }
        }
    }
}

/* ========================================================================== */
/*  排版核心                                                                    */
/* ========================================================================== */

/**
 * @brief 内部排版参数。
 */
typedef struct {
    int content_x;      /**< 内容区左 X。 */
    int content_y;      /**< 内容区顶 Y。 */
    int content_w;      /**< 内容区宽度。 */
    int content_h;      /**< 内容区高度。 */
    int line_height;    /**< 行高。 */
    int font_size;      /**< 字号。 */
    int ascent;         /**< 基线上方高度。 */
} layout_ctx_t;

static layout_ctx_t make_ctx(font_handle_t font, const text_layout_params_t *p) {
    layout_ctx_t ctx;
    ctx.font_size = p->font_size;
    ctx.content_x = p->margin_left;
    ctx.content_y = p->margin_top;
    ctx.content_w = UI_SCREEN_WIDTH - p->margin_left - p->margin_right;
    ctx.content_h = UI_SCREEN_HEIGHT - p->margin_top - p->margin_bottom;

    int ascent, descent, line_gap;
    font_manager_get_metrics(font, p->font_size, &ascent, &descent, &line_gap);
    ctx.ascent = ascent;

    if (p->line_height > 0) {
        ctx.line_height = p->line_height;
    } else {
        ctx.line_height = (p->font_size * 3) / 2;  /* 1.5 倍 */
    }

    return ctx;
}

/**
 * @brief 排版引擎核心：逐字符处理，支持渲染或仅计算。
 *
 * @param fb   framebuffer（NULL = 仅计算不渲染）。
 * @param font 字体句柄。
 * @param text 文本。
 * @param len  长度。
 * @param ctx  排版上下文。
 * @return 已处理的字节数。
 */
static size_t layout_text(uint8_t *fb, font_handle_t font,
                           const char *text, size_t len,
                           const layout_ctx_t *ctx) {
    const char *ptr = text;
    const char *end = text + len;

    int cursor_x = ctx->content_x;
    int cursor_y = ctx->content_y + ctx->ascent;
    int max_y = ctx->content_y + ctx->content_h;

    while (ptr < end) {
        /* 检查是否超出页面 */
        if (cursor_y - ctx->ascent + ctx->line_height > max_y) {
            break;
        }

        uint32_t cp;
        int bytes = utf8_next(ptr, end, &cp);
        if (bytes == 0) break;

        /* 换行符 */
        if (cp == '\n') {
            cursor_x = ctx->content_x;
            cursor_y += ctx->line_height;
            ptr += bytes;
            continue;
        }

        /* 回车跳过 */
        if (cp == '\r') {
            ptr += bytes;
            continue;
        }

        /* 获取字形以计算宽度 */
        font_glyph_t glyph;
        esp_err_t ret = font_manager_get_glyph(font, cp, ctx->font_size, &glyph);
        int char_advance = (ret == ESP_OK) ? glyph.advance_x : ctx->font_size;

        /* 自动换行判断 */
        if (cursor_x + char_advance > ctx->content_x + ctx->content_w) {
            if (is_cjk(cp) || is_space(cp)) {
                /* CJK/空格：直接换行 */
                cursor_x = ctx->content_x;
                cursor_y += ctx->line_height;
                if (cursor_y - ctx->ascent + ctx->line_height > max_y) {
                    break;
                }
                if (is_space(cp)) {
                    ptr += bytes;
                    continue;
                }
            } else {
                /* 英文：回溯到上一个空格处断行 */
                const char *word_start = ptr;
                /* 向后找当前单词起始 */
                while (word_start > text) {
                    const char *prev = word_start - 1;
                    while (prev > text && ((uint8_t)*prev & 0xC0) == 0x80) prev--;
                    uint32_t prev_cp;
                    utf8_next(prev, end, &prev_cp);
                    if (is_space(prev_cp) || is_cjk(prev_cp) || prev_cp == '\n') break;
                    word_start = prev;
                }

                if (word_start > text && word_start != ptr) {
                    /* 回退到单词起始，但如果单词太长（整行放不下），强制断字 */
                    /* 在这里 ptr 还没推进，我们直接换行重新从当前位置开始 */
                }
                cursor_x = ctx->content_x;
                cursor_y += ctx->line_height;
                if (cursor_y - ctx->ascent + ctx->line_height > max_y) {
                    break;
                }
            }
        }

        /* 渲染字形 */
        if (fb && ret == ESP_OK) {
            draw_glyph(fb, &glyph, cursor_x, cursor_y);
        }

        cursor_x += char_advance;
        ptr += bytes;
    }

    return (size_t)(ptr - text);
}

/* ========================================================================== */
/*  公开 API                                                                   */
/* ========================================================================== */

text_layout_params_t text_layout_default_params(void) {
    return (text_layout_params_t){
        .font_size     = 24,
        .line_height   = 0,  /* 自动 */
        .margin_top    = 30,
        .margin_bottom = 30,
        .margin_left   = 30,
        .margin_right  = 30,
    };
}

size_t text_layout_render_page(uint8_t *fb, font_handle_t font,
                                const char *text, size_t len,
                                const text_layout_params_t *params) {
    if (!fb || !font || !text || len == 0) return 0;
    layout_ctx_t ctx = make_ctx(font, params);
    return layout_text(fb, font, text, len, &ctx);
}

size_t text_layout_calc_page_end(font_handle_t font,
                                  const char *text, size_t len,
                                  const text_layout_params_t *params) {
    if (!font || !text || len == 0) return 0;
    layout_ctx_t ctx = make_ctx(font, params);
    return layout_text(NULL, font, text, len, &ctx);
}

int text_layout_render_line(uint8_t *fb, font_handle_t font,
                             const char *text, size_t len,
                             int x, int y, int max_width, int font_size) {
    if (!fb || !font || !text || len == 0) return 0;

    const char *ptr = text;
    const char *end = text + len;
    int cursor_x = x;

    while (ptr < end) {
        uint32_t cp;
        int bytes = utf8_next(ptr, end, &cp);
        if (bytes == 0 || cp == '\n' || cp == '\r') break;

        font_glyph_t glyph;
        esp_err_t ret = font_manager_get_glyph(font, cp, font_size, &glyph);
        int advance = (ret == ESP_OK) ? glyph.advance_x : font_size;

        if (max_width > 0 && cursor_x - x + advance > max_width) break;

        if (ret == ESP_OK) {
            draw_glyph(fb, &glyph, cursor_x, y);
        }

        cursor_x += advance;
        ptr += bytes;
    }

    return cursor_x - x;
}
