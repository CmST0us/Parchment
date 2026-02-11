/**
 * @file ui_widget.c
 * @brief 通用 UI Widget 组件实现。
 */

#include "ui_widget.h"

#include <stdio.h>
#include <string.h>

#include "ui_canvas.h"
#include "ui_font.h"
#include "ui_icon.h"

/* ── Header ─────────────────────────────────────────────────────── */

void ui_widget_draw_header(uint8_t *fb, const ui_header_t *header) {
    if (!fb || !header) {
        return;
    }

    /* 黑底 48px 标题栏。 */
    ui_canvas_fill_rect(fb, 0, 0, UI_SCREEN_WIDTH, UI_HEADER_HEIGHT,
                        UI_COLOR_BLACK);

    /* 左侧图标。 */
    if (header->icon_left) {
        ui_icon_draw(fb, 8, 8, header->icon_left, UI_COLOR_WHITE);
    }

    /* 右侧图标。 */
    if (header->icon_right) {
        ui_icon_draw(fb, 500, 8, header->icon_right, UI_COLOR_WHITE);
    }

    /* 居中标题文字 (24px)。 */
    if (header->title) {
        const EpdFont *font = ui_font_get(24);
        int text_w = ui_canvas_measure_text(font, header->title);
        int text_x = (UI_SCREEN_WIDTH - text_w) / 2;
        /* 基线 Y: 垂直居中近似 = (48 + 24) / 2 = 36。 */
        int text_y = 36;
        ui_canvas_draw_text(fb, text_x, text_y, font, header->title,
                            UI_COLOR_WHITE);
    }
}

int ui_widget_header_hit_test(const ui_header_t *header, int x, int y) {
    if (!header) {
        return UI_HEADER_HIT_NONE;
    }

    /* 超出 header 区域。 */
    if (y > 47 || y < 0 || x < 0 || x >= UI_SCREEN_WIDTH) {
        return UI_HEADER_HIT_NONE;
    }

    /* 左侧图标区域: x 0-47, y 0-47。 */
    if (header->icon_left && x <= 47) {
        return UI_HEADER_HIT_LEFT;
    }

    /* 右侧图标区域: x 492-539, y 0-47。 */
    if (header->icon_right && x >= 492) {
        return UI_HEADER_HIT_RIGHT;
    }

    return UI_HEADER_HIT_NONE;
}

/* ── Button ─────────────────────────────────────────────────────── */

void ui_widget_draw_button(uint8_t *fb, const ui_button_t *btn) {
    if (!fb || !btn) {
        return;
    }

    uint8_t bg_color = UI_COLOR_WHITE;
    uint8_t fg_color = UI_COLOR_BLACK;
    bool draw_border = false;
    bool draw_bg = true;

    switch (btn->style) {
        case UI_BTN_PRIMARY:
            bg_color = UI_COLOR_BLACK;
            fg_color = UI_COLOR_WHITE;
            break;
        case UI_BTN_SECONDARY:
            bg_color = UI_COLOR_WHITE;
            fg_color = UI_COLOR_BLACK;
            draw_border = true;
            break;
        case UI_BTN_ICON:
            draw_bg = false;
            fg_color = UI_COLOR_BLACK;
            break;
        case UI_BTN_SELECTED:
            bg_color = UI_COLOR_LIGHT;
            fg_color = UI_COLOR_BLACK;
            break;
    }

    /* 背景。 */
    if (draw_bg) {
        ui_canvas_fill_rect(fb, btn->x, btn->y, btn->w, btn->h, bg_color);
    }

    /* 边框 (SECONDARY 样式, 2px)。 */
    if (draw_border) {
        ui_canvas_draw_rect(fb, btn->x, btn->y, btn->w, btn->h,
                            UI_COLOR_BLACK, 2);
    }

    bool has_icon = (btn->icon != NULL);
    bool has_label = (btn->label != NULL && btn->label[0] != '\0');

    if (has_icon && has_label) {
        /* 图标在上，文字在下，整体垂直居中。 */
        int icon_h = btn->icon->h;
        int gap = 4;
        const EpdFont *font = ui_font_get(20);
        int text_w = ui_canvas_measure_text(font, btn->label);
        int total_h = icon_h + gap + 20;  /* icon + gap + text_height */
        int start_y = btn->y + (btn->h - total_h) / 2;

        /* 图标居中。 */
        int icon_x = btn->x + (btn->w - btn->icon->w) / 2;
        ui_icon_draw(fb, icon_x, start_y, btn->icon, fg_color);

        /* 文字居中。 */
        int text_x = btn->x + (btn->w - text_w) / 2;
        int text_y = start_y + icon_h + gap + 16;  /* 基线近似 */
        ui_canvas_draw_text(fb, text_x, text_y, font, btn->label, fg_color);
    } else if (has_icon) {
        /* 仅图标，居中。 */
        int icon_x = btn->x + (btn->w - btn->icon->w) / 2;
        int icon_y = btn->y + (btn->h - btn->icon->h) / 2;
        ui_icon_draw(fb, icon_x, icon_y, btn->icon, fg_color);
    } else if (has_label) {
        /* 仅文字，水平垂直居中。 */
        const EpdFont *font = ui_font_get(24);
        int text_w = ui_canvas_measure_text(font, btn->label);
        int text_x = btn->x + (btn->w - text_w) / 2;
        /* 基线近似: btn->y + (btn->h + font_size) / 2 - descent */
        int text_y = btn->y + (btn->h + 24) / 2 - 4;
        ui_canvas_draw_text(fb, text_x, text_y, font, btn->label, fg_color);
    }
}

bool ui_widget_button_hit_test(const ui_button_t *btn, int x, int y) {
    if (!btn) {
        return false;
    }
    return (x >= btn->x && x < btn->x + btn->w &&
            y >= btn->y && y < btn->y + btn->h);
}

/* ── Progress ───────────────────────────────────────────────────── */

void ui_widget_draw_progress(uint8_t *fb, const ui_progress_t *prog) {
    if (!fb || !prog) {
        return;
    }

    int value = prog->value;
    if (value < 0) value = 0;
    if (value > 100) value = 100;

    int filled_w = prog->w * value / 100;

    /* 已完成区域 (BLACK)。 */
    if (filled_w > 0) {
        ui_canvas_fill_rect(fb, prog->x, prog->y, filled_w, prog->h,
                            UI_COLOR_BLACK);
    }

    /* 未完成区域 (LIGHT)。 */
    if (filled_w < prog->w) {
        ui_canvas_fill_rect(fb, prog->x + filled_w, prog->y,
                            prog->w - filled_w, prog->h, UI_COLOR_LIGHT);
    }

    /* 百分比标签。 */
    if (prog->show_label) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", value);
        const EpdFont *font = ui_font_get(20);
        int text_x = prog->x + prog->w + 8;
        int text_y = prog->y + (prog->h + 20) / 2 - 2;
        ui_canvas_draw_text(fb, text_x, text_y, font, buf, UI_COLOR_DARK);
    }
}

int ui_widget_progress_touch(const ui_progress_t *prog, int x, int y) {
    if (!prog) {
        return -1;
    }

    /* Y 方向 ±20px 扩展触摸区。 */
    if (y < prog->y - 20 || y > prog->y + prog->h + 20) {
        return -1;
    }

    /* X 方向须在进度条范围内。 */
    if (x < prog->x || x > prog->x + prog->w) {
        return -1;
    }

    int value = (x - prog->x) * 100 / prog->w;
    if (value < 0) value = 0;
    if (value > 100) value = 100;
    return value;
}

/* ── List ───────────────────────────────────────────────────────── */

void ui_widget_draw_list(uint8_t *fb, const ui_list_t *list) {
    if (!fb || !list || !list->draw_item || list->item_count <= 0 ||
        list->item_height <= 0) {
        return;
    }

    int first = list->scroll_offset / list->item_height;
    int offset_within = list->scroll_offset % list->item_height;

    /* 从第一个可见项开始，逐项绘制直到超出可视区域。 */
    int draw_y = list->y - offset_within;
    for (int i = first; i < list->item_count; i++) {
        if (draw_y >= list->y + list->h) {
            break;  /* 超出可视区域底部。 */
        }
        list->draw_item(fb, i, list->x, draw_y, list->w, list->item_height);
        draw_y += list->item_height;
    }
}

int ui_widget_list_hit_test(const ui_list_t *list, int x, int y) {
    if (!list || list->item_height <= 0) {
        return -1;
    }

    /* 检查是否在可视区域内。 */
    if (x < list->x || x >= list->x + list->w ||
        y < list->y || y >= list->y + list->h) {
        return -1;
    }

    int index = (y - list->y + list->scroll_offset) / list->item_height;
    if (index < 0 || index >= list->item_count) {
        return -1;
    }
    return index;
}

void ui_widget_list_scroll(ui_list_t *list, int delta_y) {
    if (!list || list->item_height <= 0) {
        return;
    }

    int max_scroll = list->item_count * list->item_height - list->h;
    if (max_scroll < 0) {
        max_scroll = 0;
    }

    list->scroll_offset += delta_y;
    if (list->scroll_offset < 0) {
        list->scroll_offset = 0;
    }
    if (list->scroll_offset > max_scroll) {
        list->scroll_offset = max_scroll;
    }
}

/* ── Separator ──────────────────────────────────────────────────── */

void ui_widget_draw_separator(uint8_t *fb, int x, int y, int w) {
    if (!fb) {
        return;
    }
    ui_canvas_draw_hline(fb, x, y, w, UI_COLOR_MEDIUM);
}
