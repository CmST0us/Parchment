/**
 * @file page_library.c
 * @brief 书库列表页面实现。
 *
 * 扫描 SD 卡中的 TXT 文件并以列表形式展示，支持滚动浏览、排序切换和
 * 书目选择。布局遵循 ui-wireframe-spec.md 第 2 节线框规格。
 */

#include "page_library.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"

#include "book_store.h"
#include "settings_store.h"
#include "ui_canvas.h"
#include "ui_event.h"
#include "ui_font.h"
#include "ui_icon.h"
#include "ui_render.h"
#include "ui_widget.h"

static const char *TAG = "page_library";

/* ── 布局常量 ─────────────────────────────────────────────────── */

/** Header 高度。 */
#define HEADER_H       48
/** Subheader 高度。 */
#define SUBHEADER_H    36
/** 列表起始 Y 坐标。 */
#define LIST_Y         (HEADER_H + SUBHEADER_H)  /* 84 */
/** 列表可视高度。 */
#define LIST_H         (UI_SCREEN_HEIGHT - LIST_Y)  /* 876 */
/** 每个书目项高度。 */
#define ITEM_H         96
/** 滚动步长（像素）。 */
#define SCROLL_STEP    ITEM_H

/** 排序选项数量。 */
#define SORT_OPTION_COUNT 2

/** 排序选项标签。 */
static const char *s_sort_labels[SORT_OPTION_COUNT] = {
    "按名称排序",
    "按大小排序",
};

/* ── 页面状态 ─────────────────────────────────────────────────── */

/** 当前书籍数量。 */
static size_t s_book_count;

/** 当前排序方式 (book_sort_t)。 */
static uint8_t s_sort_order;

/** 阅读进度缓存（百分比 0-100，每本书一个）。 */
static uint8_t s_progress_cache[BOOK_STORE_MAX_BOOKS];

/** Header Widget 配置。 */
static ui_header_t s_header;

/** 书目列表 Widget。 */
static ui_list_t s_book_list;

/** 排序弹窗 Widget。 */
static ui_dialog_t s_sort_dialog;

/* ── 前向声明 ─────────────────────────────────────────────────── */

static void draw_book_item(uint8_t *fb, int index, int x, int y, int w, int h);
static void draw_sort_item(uint8_t *fb, int index, int x, int y, int w, int h);
static void cache_all_progress(void);
static void draw_subheader(uint8_t *fb);
static void apply_sort(uint8_t order);
static bool handle_subheader_tap(int x, int y);

/* ── 辅助函数 ─────────────────────────────────────────────────── */

/** 批量缓存所有书籍的阅读进度到 s_progress_cache。 */
static void cache_all_progress(void) {
    for (size_t i = 0; i < s_book_count; i++) {
        const book_info_t *book = book_store_get_book(i);
        if (book == NULL) {
            s_progress_cache[i] = 0;
            continue;
        }
        reading_progress_t prog = {0};
        if (settings_store_load_progress(book->path, &prog) == ESP_OK &&
            prog.total_bytes > 0) {
            uint32_t pct = (uint32_t)prog.byte_offset * 100 / prog.total_bytes;
            s_progress_cache[i] = (pct > 100) ? 100 : (uint8_t)pct;
        } else {
            s_progress_cache[i] = 0;
        }
    }
}

/** 应用排序并刷新状态。 */
static void apply_sort(uint8_t order) {
    s_sort_order = order;
    book_store_sort((book_sort_t)order);
    cache_all_progress();
    s_book_list.scroll_offset = 0;
    settings_store_save_sort_order(order);
    ui_render_mark_full_dirty();
}

/* ── 绘制回调 ─────────────────────────────────────────────────── */

/** 绘制 Subheader 区域（书籍数量 + 排序按钮）。 */
static void draw_subheader(uint8_t *fb) {
    /* LIGHT 底色背景。 */
    ui_canvas_fill_rect(fb, 0, HEADER_H, UI_SCREEN_WIDTH, SUBHEADER_H,
                        UI_COLOR_LIGHT);

    const EpdFont *font = ui_font_get(20);

    /* 左侧: "N 本书" */
    char count_str[32];
    snprintf(count_str, sizeof(count_str), "%d 本书", (int)s_book_count);
    ui_canvas_draw_text(fb, 16, HEADER_H + 26, font, count_str,
                        UI_COLOR_BLACK);

    /* 右侧: 排序方式文字 */
    const char *sort_text = s_sort_labels[s_sort_order < SORT_OPTION_COUNT
                                              ? s_sort_order : 0];
    int text_w = ui_canvas_measure_text(font, sort_text);
    ui_canvas_draw_text(fb, UI_SCREEN_WIDTH - 16 - text_w, HEADER_H + 26,
                        font, sort_text, UI_COLOR_DARK);
}

/** 检测 Subheader 排序区域点击。返回 true 表示命中。 */
static bool handle_subheader_tap(int x, int y) {
    /* Subheader 区域: y 在 HEADER_H ~ HEADER_H+SUBHEADER_H */
    if (y < HEADER_H || y >= HEADER_H + SUBHEADER_H) {
        return false;
    }
    /* 右半侧为排序按钮区域。 */
    if (x >= UI_SCREEN_WIDTH / 2) {
        return true;
    }
    return false;
}

/** 书目列表项绘制回调。 */
static void draw_book_item(uint8_t *fb, int index, int x, int y,
                           int w, int h) {
    const book_info_t *book = book_store_get_book((size_t)index);
    if (book == NULL) {
        return;
    }

    const EpdFont *font20 = ui_font_get(20);

    /* 格式标签区域: (x+16, y+8), 60×80 */
    ui_canvas_fill_rect(fb, x + 16, y + 8, 60, 80, UI_COLOR_LIGHT);
    ui_canvas_draw_rect(fb, x + 16, y + 8, 60, 80, UI_COLOR_BLACK, 1);

    /* "TXT" 文字居中 */
    const char *fmt_label = "TXT";
    int fmt_w = ui_canvas_measure_text(font20, fmt_label);
    ui_canvas_draw_text(fb, x + 16 + (60 - fmt_w) / 2, y + 8 + 48,
                        font20, fmt_label, UI_COLOR_BLACK);

    /* 书名: (x+92, y+12), 20px BLACK
     * 去掉 .txt 后缀显示。 */
    char display_name[BOOK_NAME_MAX_LEN];
    strncpy(display_name, book->name, sizeof(display_name) - 1);
    display_name[sizeof(display_name) - 1] = '\0';
    size_t name_len = strlen(display_name);
    if (name_len > 4) {
        const char *ext = display_name + name_len - 4;
        if (strcasecmp(ext, ".txt") == 0) {
            display_name[name_len - 4] = '\0';
        }
    }
    ui_canvas_draw_text(fb, x + 92, y + 28, font20, display_name,
                        UI_COLOR_BLACK);

    /* 作者: (x+92, y+40), 20px DARK — TXT 无作者元数据 */
    ui_canvas_draw_text(fb, x + 92, y + 56, font20, "Unknown",
                        UI_COLOR_DARK);

    /* 进度条: (x+92, y+66), 200×8 */
    ui_progress_t prog = {
        .x = x + 92,
        .y = y + 66,
        .w = 200,
        .h = 8,
        .value = s_progress_cache[index],
        .show_label = true,
    };
    ui_widget_draw_progress(fb, &prog);

    /* 分隔线: (x+16, y+95), 508×1 */
    ui_widget_draw_separator(fb, x + 16, y + 95, 508);
}

/** 排序弹窗列表项绘制回调。 */
static void draw_sort_item(uint8_t *fb, int index, int x, int y,
                           int w, int h) {
    if (index < 0 || index >= SORT_OPTION_COUNT) {
        return;
    }

    const EpdFont *font = ui_font_get(20);
    const char *label = s_sort_labels[index];

    /* 选中项加亮背景。 */
    if ((uint8_t)index == s_sort_order) {
        ui_canvas_fill_rect(fb, x, y, w, h, UI_COLOR_LIGHT);
    }

    /* 文字居中（水平左对齐 + 左边距，垂直居中）。 */
    ui_canvas_draw_text(fb, x + 24, y + 32, font, label, UI_COLOR_BLACK);
}

/* ── 页面回调 ─────────────────────────────────────────────────── */

/** 进入书库页面。 */
static void library_on_enter(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "Library screen entered");

    /* 扫描 SD 卡。 */
    book_store_scan();
    s_book_count = book_store_get_count();
    ESP_LOGI(TAG, "Found %d books", (int)s_book_count);

    /* 恢复排序偏好。 */
    s_sort_order = 0;
    settings_store_load_sort_order(&s_sort_order);
    if (s_sort_order >= SORT_OPTION_COUNT) {
        s_sort_order = 0;
    }
    book_store_sort((book_sort_t)s_sort_order);

    /* 批量缓存阅读进度。 */
    cache_all_progress();

    /* 初始化 Header Widget。 */
    s_header = (ui_header_t){
        .title = "Parchment",
        .icon_left = &UI_ICON_MENU,
        .icon_right = &UI_ICON_SETTINGS,
    };

    /* 初始化书目列表 Widget。 */
    s_book_list = (ui_list_t){
        .x = 0,
        .y = LIST_Y,
        .w = UI_SCREEN_WIDTH,
        .h = LIST_H,
        .item_height = ITEM_H,
        .item_count = (int)s_book_count,
        .scroll_offset = 0,
        .draw_item = draw_book_item,
    };

    /* 初始化排序弹窗（不可见）。 */
    s_sort_dialog = (ui_dialog_t){
        .title = "排序方式",
        .list = {
            .item_height = 48,
            .item_count = SORT_OPTION_COUNT,
            .scroll_offset = 0,
            .draw_item = draw_sort_item,
        },
        .visible = false,
    };

    ui_render_mark_full_dirty();
}

/** 退出书库页面。 */
static void library_on_exit(void) {
    ESP_LOGI(TAG, "Library screen exited");
}

/** 事件处理。 */
static void library_on_event(ui_event_t *event) {
    /* 排序弹窗可见时，优先处理弹窗事件。 */
    if (s_sort_dialog.visible) {
        if (event->type == UI_EVENT_TOUCH_TAP) {
            int hit = ui_widget_dialog_hit_test(&s_sort_dialog,
                                                event->x, event->y);
            if (hit >= 0 && hit < SORT_OPTION_COUNT) {
                /* 选择排序方式。 */
                ESP_LOGI(TAG, "Sort selected: %s", s_sort_labels[hit]);
                s_sort_dialog.visible = false;
                apply_sort((uint8_t)hit);
            } else if (hit == UI_DIALOG_HIT_MASK) {
                /* 点击遮罩关闭弹窗。 */
                s_sort_dialog.visible = false;
                ui_render_mark_full_dirty();
            }
            /* UI_DIALOG_HIT_TITLE: 忽略。 */
        }
        return;
    }

    switch (event->type) {
    case UI_EVENT_TOUCH_TAP: {
        /* Header 命中检测。 */
        int header_hit = ui_widget_header_hit_test(&s_header,
                                                    event->x, event->y);
        if (header_hit == UI_HEADER_HIT_LEFT) {
            ESP_LOGI(TAG, "Menu button tapped");
            return;
        }
        if (header_hit == UI_HEADER_HIT_RIGHT) {
            ESP_LOGI(TAG, "Settings button tapped");
            return;
        }

        /* Subheader 排序区域命中。 */
        if (handle_subheader_tap(event->x, event->y)) {
            ESP_LOGI(TAG, "Sort button tapped, showing dialog");
            s_sort_dialog.visible = true;
            ui_render_mark_full_dirty();
            return;
        }

        /* 书目列表命中。 */
        int item_idx = ui_widget_list_hit_test(&s_book_list,
                                                event->x, event->y);
        if (item_idx >= 0 && item_idx < (int)s_book_count) {
            const book_info_t *book = book_store_get_book((size_t)item_idx);
            if (book != NULL) {
                ESP_LOGI(TAG, "Book tapped [%d]: %s", item_idx, book->name);
                /* TODO: ui_page_push(&page_reader, (void *)book); */
            }
        }
        break;
    }

    case UI_EVENT_TOUCH_SWIPE_UP:
        if (s_book_count > 0) {
            ui_widget_list_scroll(&s_book_list, SCROLL_STEP);
            ui_render_mark_dirty(0, LIST_Y, UI_SCREEN_WIDTH, LIST_H);
        }
        break;

    case UI_EVENT_TOUCH_SWIPE_DOWN:
        if (s_book_count > 0) {
            ui_widget_list_scroll(&s_book_list, -SCROLL_STEP);
            ui_render_mark_dirty(0, LIST_Y, UI_SCREEN_WIDTH, LIST_H);
        }
        break;

    default:
        break;
    }
}

/** 渲染书库页面。 */
static void library_on_render(uint8_t *fb) {
    ui_canvas_fill(fb, UI_COLOR_WHITE);

    /* Header */
    ui_widget_draw_header(fb, &s_header);

    /* Subheader */
    draw_subheader(fb);

    /* 书目列表或空状态。 */
    if (s_book_count > 0) {
        ui_widget_draw_list(fb, &s_book_list);
    } else {
        /* 空状态: 居中提示文字。 */
        const EpdFont *font = ui_font_get(20);
        const char *empty_text = "未找到书籍";
        int text_w = ui_canvas_measure_text(font, empty_text);
        int text_x = (UI_SCREEN_WIDTH - text_w) / 2;
        int text_y = LIST_Y + LIST_H / 2;
        ui_canvas_draw_text(fb, text_x, text_y, font, empty_text,
                            UI_COLOR_MEDIUM);
    }

    /* 排序弹窗（如果可见）。 */
    ui_widget_draw_dialog(fb, &s_sort_dialog);
}

/** 书库列表页面实例。 */
ui_page_t page_library = {
    .name = "library",
    .on_enter = library_on_enter,
    .on_exit = library_on_exit,
    .on_event = library_on_event,
    .on_render = library_on_render,
};
