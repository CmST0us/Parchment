/**
 * @file bookshelf_page.c
 * @brief 书架页面实现。
 *
 * 扫描 /sdcard/books/ 下的 .txt 文件，渲染为书名列表，
 * 支持上下翻页和点击选书进入阅读。
 */

#include "bookshelf_page.h"
#include "reader_page.h"
#include "ui_core.h"
#include "font_manager.h"
#include "text_layout.h"
#include "sd_storage.h"

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "esp_log.h"

static const char *TAG = "bookshelf";

/* ---- 配置 ---- */

#define BOOKS_DIR       "/sdcard/book"
#define MAX_BOOKS       64
#define BOOK_PATH_MAX   160
#define BOOK_NAME_MAX   80

#define LIST_MARGIN_X   30
#define LIST_START_Y    60
#define LIST_ITEM_H     52
#define LIST_FONT_SIZE  28

/* ---- 状态 ---- */

typedef struct {
    char path[BOOK_PATH_MAX];
    char name[BOOK_NAME_MAX];
} book_entry_t;

static book_entry_t s_books[MAX_BOOKS];
static int s_book_count = 0;
static int s_page_offset = 0;  /* 列表滚动偏移（首项 index） */
static int s_items_per_page = 0;
static bool s_full_render = false;

/* ---- 辅助 ---- */

/**
 * @brief 从文件名中提取书名（去掉 .txt 后缀）。
 */
static void extract_book_name(const char *filename, char *name, size_t name_size) {
    strncpy(name, filename, name_size - 1);
    name[name_size - 1] = '\0';
    size_t len = strlen(name);
    if (len > 4 && strcasecmp(name + len - 4, ".txt") == 0) {
        name[len - 4] = '\0';
    }
}

/**
 * @brief 扫描 books 目录。
 */
static void scan_books(void) {
    s_book_count = 0;

    if (!sd_storage_is_mounted()) {
        ESP_LOGW(TAG, "SD card not mounted");
        return;
    }

    DIR *dir = opendir(BOOKS_DIR);
    if (!dir) {
        ESP_LOGW(TAG, "Cannot open %s", BOOKS_DIR);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && s_book_count < MAX_BOOKS) {
        if (entry->d_type != DT_REG) continue;
        size_t nlen = strlen(entry->d_name);
        if (nlen < 5) continue;
        if (strcasecmp(entry->d_name + nlen - 4, ".txt") != 0) continue;

        size_t need = strlen(BOOKS_DIR) + 1 + nlen + 1;
        if (need > BOOK_PATH_MAX) continue;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-truncation"
        snprintf(s_books[s_book_count].path, BOOK_PATH_MAX,
                 "%s/%s", BOOKS_DIR, entry->d_name);
#pragma GCC diagnostic pop
        extract_book_name(entry->d_name, s_books[s_book_count].name, BOOK_NAME_MAX);
        s_book_count++;
    }
    closedir(dir);

    ESP_LOGI(TAG, "Found %d books", s_book_count);
}

/**
 * @brief 计算每页可显示的书籍数量。
 */
static int calc_items_per_page(void) {
    int available_h = UI_SCREEN_HEIGHT - LIST_START_Y - 30;
    return available_h / LIST_ITEM_H;
}

/**
 * @brief 命中测试：返回点击到的书籍 index，-1 未命中。
 */
static int hit_test(int x, int y) {
    if (x < LIST_MARGIN_X || x >= UI_SCREEN_WIDTH - LIST_MARGIN_X) return -1;

    for (int i = 0; i < s_items_per_page && s_page_offset + i < s_book_count; i++) {
        int item_y = LIST_START_Y + i * LIST_ITEM_H;
        if (y >= item_y && y < item_y + LIST_ITEM_H) {
            return s_page_offset + i;
        }
    }
    return -1;
}

/* ---- 绘制 ---- */

/**
 * @brief 绘制空书架状态。
 */
static void draw_empty(uint8_t *fb) {
    /* 中央画一个大空心矩形 + 内部小矩形表示"空书架" */
    int cx = UI_SCREEN_WIDTH / 2;
    int cy = UI_SCREEN_HEIGHT / 2;
    ui_canvas_draw_rect(fb, cx - 80, cy - 60, 160, 120, UI_COLOR_MEDIUM, 3);
    ui_canvas_draw_rect(fb, cx - 50, cy - 30, 100, 60, UI_COLOR_LIGHT, 2);
    /* 小横线表示书脊 */
    for (int i = 0; i < 3; i++) {
        ui_canvas_draw_hline(fb, cx - 30, cy - 10 + i * 12, 60, UI_COLOR_MEDIUM);
    }
}

/**
 * @brief 绘制书名列表。
 */
static void draw_book_list(uint8_t *fb) {
    font_handle_t font = font_manager_get_default();
    if (!font) {
        ESP_LOGE(TAG, "No font available");
        return;
    }

    int ascent, descent, line_gap;
    font_manager_get_metrics(font, LIST_FONT_SIZE, &ascent, &descent, &line_gap);

    int max_width = UI_SCREEN_WIDTH - LIST_MARGIN_X * 2;

    for (int i = 0; i < s_items_per_page && s_page_offset + i < s_book_count; i++) {
        int idx = s_page_offset + i;
        int item_y = LIST_START_Y + i * LIST_ITEM_H;
        int baseline_y = item_y + ascent + (LIST_ITEM_H - ascent - descent) / 2;

        /* 书名 */
        text_layout_render_line(fb, font, s_books[idx].name,
                                 strlen(s_books[idx].name),
                                 LIST_MARGIN_X, baseline_y,
                                 max_width, LIST_FONT_SIZE);

        /* 分隔线 */
        int line_y = item_y + LIST_ITEM_H - 1;
        ui_canvas_draw_hline(fb, LIST_MARGIN_X, line_y,
                             UI_SCREEN_WIDTH - LIST_MARGIN_X * 2, UI_COLOR_LIGHT);
    }

    /* 底部页码指示 */
    if (s_book_count > s_items_per_page) {
        int total_pages = (s_book_count + s_items_per_page - 1) / s_items_per_page;
        int cur_page = s_page_offset / s_items_per_page + 1;

        /* 简单的点指示器 */
        int dot_r = 4;
        int dot_gap = 16;
        int total_w = total_pages * dot_r * 2 + (total_pages - 1) * dot_gap;
        int dot_x = (UI_SCREEN_WIDTH - total_w) / 2;
        int dot_y = UI_SCREEN_HEIGHT - 25;

        for (int p = 0; p < total_pages && p < 10; p++) {
            int cx = dot_x + dot_r + p * (dot_r * 2 + dot_gap);
            if (p + 1 == cur_page) {
                ui_canvas_fill_rect(fb, cx - dot_r, dot_y - dot_r,
                                    dot_r * 2, dot_r * 2, UI_COLOR_BLACK);
            } else {
                ui_canvas_draw_rect(fb, cx - dot_r, dot_y - dot_r,
                                    dot_r * 2, dot_r * 2, UI_COLOR_DARK, 1);
            }
        }
    }
}

/* ---- 页面回调 ---- */

static void bookshelf_on_enter(void *arg) {
    ESP_LOGI(TAG, "Bookshelf entered");
    scan_books();
    s_page_offset = 0;
    s_items_per_page = calc_items_per_page();
    s_full_render = true;
    ui_render_mark_full_dirty();
}

static void bookshelf_on_event(ui_event_t *event) {
    switch (event->type) {

    case UI_EVENT_TOUCH_TAP: {
        int idx = hit_test(event->x, event->y);
        if (idx >= 0 && idx < s_book_count) {
            ESP_LOGI(TAG, "Selected: %s", s_books[idx].name);
            ui_page_push(&reader_page, s_books[idx].path);
        }
        break;
    }

    case UI_EVENT_TOUCH_SWIPE_UP:
        if (s_page_offset + s_items_per_page < s_book_count) {
            s_page_offset += s_items_per_page;
            s_full_render = true;
            ui_render_mark_full_dirty();
        }
        break;

    case UI_EVENT_TOUCH_SWIPE_DOWN:
        if (s_page_offset > 0) {
            s_page_offset -= s_items_per_page;
            if (s_page_offset < 0) s_page_offset = 0;
            s_full_render = true;
            ui_render_mark_full_dirty();
        }
        break;

    default:
        break;
    }
}

static void bookshelf_on_render(uint8_t *fb) {
    if (s_full_render) {
        s_full_render = false;
        ui_canvas_fill(fb, UI_COLOR_WHITE);

        if (s_book_count == 0) {
            draw_empty(fb);
        } else {
            draw_book_list(fb);
        }
    }
}

ui_page_t bookshelf_page = {
    .name     = "bookshelf",
    .on_enter = bookshelf_on_enter,
    .on_exit  = NULL,
    .on_event = bookshelf_on_event,
    .on_render = bookshelf_on_render,
};
