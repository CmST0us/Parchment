/**
 * @file ui_text.c
 * @brief 文本排版引擎实现。
 *
 * 支持 CJK 逐字断行、英文单词断行、标点禁则（Kinsoku Shori）和分页。
 */

#include "ui_text.h"
#include "ui_canvas.h"

#include <string.h>

#include "epdiy.h"

/* ── UTF-8 辅助 ── */

static int utf8_byte_len(uint8_t ch) {
    if ((ch & 0x80) == 0x00) return 1;
    if ((ch & 0xE0) == 0xC0) return 2;
    if ((ch & 0xF0) == 0xE0) return 3;
    if ((ch & 0xF8) == 0xF0) return 4;
    return 1;
}

/**
 * @brief 预读下一个 code point，不前进指针。
 */
static uint32_t peek_codepoint(const char *str, int *byte_len) {
    const uint8_t *s = (const uint8_t *)str;
    if (*s == 0) {
        *byte_len = 0;
        return 0;
    }
    int len = utf8_byte_len(*s);

    /* 验证续字节存在（防止截断字符串越界读取） */
    for (int i = 1; i < len; i++) {
        if (s[i] == 0) {
            *byte_len = 1;
            return 0xFFFD;
        }
    }

    *byte_len = len;
    uint32_t cp = 0;
    switch (len) {
        case 1: cp = s[0]; break;
        case 2: cp = (s[0] & 0x1F) << 6  | (s[1] & 0x3F); break;
        case 3: cp = (s[0] & 0x0F) << 12 | (s[1] & 0x3F) << 6  | (s[2] & 0x3F); break;
        case 4: cp = (s[0] & 0x07) << 18 | (s[1] & 0x3F) << 12 | (s[2] & 0x3F) << 6 | (s[3] & 0x3F); break;
        default: cp = 0xFFFD; break;
    }
    return cp;
}

/* ── CJK 字符分类 ── */

static bool is_cjk_char(uint32_t cp) {
    return (cp >= 0x4E00 && cp <= 0x9FFF) ||
           (cp >= 0x3040 && cp <= 0x309F) ||
           (cp >= 0x30A0 && cp <= 0x30FF) ||
           (cp >= 0x3400 && cp <= 0x4DBF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFF00 && cp <= 0xFFEF) ||
           (cp >= 0x3000 && cp <= 0x303F);
}

static bool is_line_start_forbidden(uint32_t cp) {
    switch (cp) {
        case 0xFF0C: case 0x3002: case 0x3001: case 0xFF1B:
        case 0xFF1A: case 0xFF1F: case 0xFF01: case 0xFF09:
        case 0x300B: case 0x300D: case 0x300F: case 0x3011:
        case 0x3009: case 0x2026: case 0x2014:
        case 0x002C: case 0x002E: case 0x003B: case 0x003A:
        case 0x003F: case 0x0021: case 0x0029:
            return true;
        default:
            return false;
    }
}

static bool is_line_end_forbidden(uint32_t cp) {
    switch (cp) {
        case 0xFF08: case 0x300A: case 0x300C: case 0x300E:
        case 0x3010: case 0x3008: case 0x0028:
            return true;
        default:
            return false;
    }
}

static bool is_ascii_letter(uint32_t cp) {
    return (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
}

/* ── 断行引擎 ── */

/**
 * @brief 计算一行能容纳的文本字节数。
 *
 * @param text      行起始位置。
 * @param max_width 行宽限制（像素）。
 * @param font      字体。
 * @return 该行消耗的字节数（含行尾的 \n）。
 */
static int layout_line(const char *text, int max_width, const EpdFont *font) {
    const char *p = text;
    int line_width = 0;
    int last_break_offset = 0;
    int last_break_width = 0;
    bool has_break = false;

    while (*p != '\0') {
        int byte_len;
        uint32_t cp = peek_codepoint(p, &byte_len);
        if (cp == 0) break;

        /* 强制换行 */
        if (cp == '\n') {
            return (int)(p - text) + byte_len;
        }

        const EpdGlyph *glyph = epd_get_glyph(font, cp);
        int char_width = glyph ? glyph->advance_x : 0;

        /* 超出行宽 */
        if (line_width + char_width > max_width && line_width > 0) {
            if (has_break) {
                return last_break_offset;
            }
            /* 无合法断点，强制截断 */
            return (int)(p - text);
        }

        line_width += char_width;
        int offset_after = (int)(p - text) + byte_len;

        /* 查看下一个字符以判断断点 */
        int next_bl;
        uint32_t next_cp = peek_codepoint(p + byte_len, &next_bl);

        /* 空格后可断行 */
        if (cp == ' ') {
            last_break_offset = offset_after;
            last_break_width = line_width;
            has_break = true;
        }
        /* CJK 字符后可断行（需检查禁则） */
        else if (is_cjk_char(cp) && !is_line_end_forbidden(cp)) {
            if (next_cp == 0 || !is_line_start_forbidden(next_cp)) {
                last_break_offset = offset_after;
                last_break_width = line_width;
                has_break = true;
            }
        }
        /* 行首禁止标点本身之后也可断行（它已经在行中了） */
        else if (is_line_start_forbidden(cp)) {
            if (next_cp == 0 || !is_line_start_forbidden(next_cp)) {
                last_break_offset = offset_after;
                last_break_width = line_width;
                has_break = true;
            }
        }
        /* 非字母的其他字符后可断行 */
        else if (!is_ascii_letter(cp) && !is_line_end_forbidden(cp)) {
            if (next_cp == 0 || !is_line_start_forbidden(next_cp)) {
                last_break_offset = offset_after;
                last_break_width = line_width;
                has_break = true;
            }
        }

        (void)last_break_width; /* 仅用于调试 */
        p += byte_len;
    }

    /* 文本结束 */
    return (int)(p - text);
}

/**
 * @brief 计算一行实际渲染的字节数（排除行尾 \n 和尾部空格用于渲染）。
 */
static int render_bytes_for_line(const char *line_start, int consumed) {
    int render_len = consumed;
    /* 去除行尾的 \n（不渲染换行符） */
    if (render_len > 0 && line_start[render_len - 1] == '\n') {
        render_len--;
    }
    return render_len;
}

/* ── 公开 API ── */

ui_text_result_t ui_canvas_draw_text_wrapped(
    uint8_t *fb, int x, int y, int max_width, int line_height,
    const EpdFont *font, const char *text, uint8_t fg_color) {

    ui_text_result_t result = { 0, 0, y, false };

    if (!font || !text || *text == '\0') {
        result.reached_end = true;
        return result;
    }

    const char *p = text;
    int cursor_y = y;

    while (*p != '\0') {
        int consumed = layout_line(p, max_width, font);
        if (consumed == 0) break;

        /* 渲染该行 */
        if (fb) {
            int render_len = render_bytes_for_line(p, consumed);
            if (render_len > 0) {
                ui_canvas_draw_text_n(fb, x, cursor_y, font, p,
                                      render_len, fg_color);
            }
        }

        result.lines_rendered++;
        result.last_y = cursor_y;
        cursor_y += line_height;
        p += consumed;
    }

    result.bytes_consumed = (int)(p - text);
    result.reached_end = (*p == '\0');
    return result;
}

ui_text_result_t ui_canvas_draw_text_page(
    uint8_t *fb, int x, int y, int max_width, int max_height,
    int line_height, const EpdFont *font,
    const char *text, int start_offset, uint8_t fg_color) {

    ui_text_result_t result = { 0, 0, y, false };

    if (!font || !text) {
        result.reached_end = true;
        return result;
    }

    int text_len = (int)strlen(text);
    if (start_offset >= text_len) {
        result.reached_end = true;
        return result;
    }

    const char *start = text + start_offset;
    const char *p = start;
    int cursor_y = y;
    int y_limit = y + max_height;

    while (*p != '\0') {
        /* 检查是否还有空间容纳一行 */
        if (cursor_y + line_height > y_limit && result.lines_rendered > 0) {
            break;
        }

        int consumed = layout_line(p, max_width, font);
        if (consumed == 0) break;

        /* 渲染该行 */
        if (fb) {
            int render_len = render_bytes_for_line(p, consumed);
            if (render_len > 0) {
                ui_canvas_draw_text_n(fb, x, cursor_y, font, p,
                                      render_len, fg_color);
            }
        }

        result.lines_rendered++;
        result.last_y = cursor_y;
        cursor_y += line_height;
        p += consumed;
    }

    result.bytes_consumed = (int)(p - start);
    result.reached_end = (*p == '\0');
    return result;
}
