/**
 * @file ui_icon.c
 * @brief 图标资源实例与绘制实现。
 */

#include "ui_icon.h"
#include "ui_canvas.h"
#include "ui_icon_data.h"

/** 图标统一尺寸。 */
#define ICON_SIZE 32

/* ── 预定义图标常量 ── */

const ui_icon_t UI_ICON_ARROW_LEFT = {
    .data = icon_arrow_left_data,
    .w = ICON_SIZE,
    .h = ICON_SIZE,
};

const ui_icon_t UI_ICON_MENU = {
    .data = icon_menu_2_data,
    .w = ICON_SIZE,
    .h = ICON_SIZE,
};

const ui_icon_t UI_ICON_SETTINGS = {
    .data = icon_settings_data,
    .w = ICON_SIZE,
    .h = ICON_SIZE,
};

const ui_icon_t UI_ICON_BOOKMARK = {
    .data = icon_bookmark_data,
    .w = ICON_SIZE,
    .h = ICON_SIZE,
};

const ui_icon_t UI_ICON_LIST = {
    .data = icon_list_data,
    .w = ICON_SIZE,
    .h = ICON_SIZE,
};

const ui_icon_t UI_ICON_SORT_DESC = {
    .data = icon_sort_descending_data,
    .w = ICON_SIZE,
    .h = ICON_SIZE,
};

const ui_icon_t UI_ICON_TYPOGRAPHY = {
    .data = icon_typography_data,
    .w = ICON_SIZE,
    .h = ICON_SIZE,
};

const ui_icon_t UI_ICON_CHEVRON_RIGHT = {
    .data = icon_chevron_right_data,
    .w = ICON_SIZE,
    .h = ICON_SIZE,
};

/* ── 绘制 API ── */

void ui_icon_draw(uint8_t *fb, int x, int y, const ui_icon_t *icon, uint8_t color) {
    if (fb == NULL || icon == NULL || icon->data == NULL) {
        return;
    }
    ui_canvas_draw_bitmap_fg(fb, x, y, icon->w, icon->h, icon->data, color);
}
