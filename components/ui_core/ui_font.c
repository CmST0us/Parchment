/**
 * @file ui_font.c
 * @brief 预编译字体资源和字号选择实现。
 */

#include "ui_font.h"

#include "noto_sans_cjk_20.h"
#include "noto_sans_cjk_24.h"
#include "noto_sans_cjk_28.h"
#include "noto_sans_cjk_32.h"
#include "noto_sans_cjk_36.h"
#include "noto_sans_cjk_40.h"

/** 可用字号表（升序排列）。 */
static const struct {
    int size;
    const EpdFont *font;
} s_font_table[] = {
    { 20, &noto_sans_cjk_20 },
    { 24, &noto_sans_cjk_24 },
    { 28, &noto_sans_cjk_28 },
    { 32, &noto_sans_cjk_32 },
    { 36, &noto_sans_cjk_36 },
    { 40, &noto_sans_cjk_40 },
};

#define FONT_TABLE_SIZE (sizeof(s_font_table) / sizeof(s_font_table[0]))

const EpdFont *ui_font_get(int size_px) {
    if (size_px <= s_font_table[0].size) {
        return s_font_table[0].font;
    }
    if (size_px >= s_font_table[FONT_TABLE_SIZE - 1].size) {
        return s_font_table[FONT_TABLE_SIZE - 1].font;
    }
    /* 向下取整到最近的可用字号。 */
    for (int i = FONT_TABLE_SIZE - 1; i >= 0; i--) {
        if (s_font_table[i].size <= size_px) {
            return s_font_table[i].font;
        }
    }
    return s_font_table[0].font;
}
