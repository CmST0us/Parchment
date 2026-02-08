/**
 * @file reader_page.c
 * @brief 阅读页面实现。
 *
 * 分页渲染 TXT 文件内容，左半屏点击上一页，右半屏点击下一页，
 * 长按返回书架。
 */

#include "reader_page.h"
#include "ui_core.h"
#include "font_manager.h"
#include "text_layout.h"
#include "text_paginator.h"

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "reader";

/* ---- 配置 ---- */

#define PAGE_BUF_SIZE   8192
#define PAGE_NUM_Y      (UI_SCREEN_HEIGHT - 20)
#define PAGE_NUM_FONT   18
#define LEFT_ZONE_X     (UI_SCREEN_WIDTH / 2)

/* ---- 状态 ---- */

static char s_file_path[160];
static page_index_t s_index;
static uint32_t s_current_page;
static bool s_index_loaded;
static char *s_page_buf;
static text_layout_params_t s_params;
static bool s_full_render;

/* ---- 辅助 ---- */

/**
 * @brief 渲染页码信息。
 */
static void draw_page_number(uint8_t *fb) {
    font_handle_t font = font_manager_get_default();
    if (!font) return;

    char num_str[32];
    snprintf(num_str, sizeof(num_str), "%"PRIu32"/%"PRIu32,
             s_current_page + 1, s_index.total_pages);

    /* 计算居中位置 */
    int text_len = strlen(num_str);
    /* 估算宽度：每个数字/斜杠约 PAGE_NUM_FONT*0.6 宽 */
    int est_width = text_len * PAGE_NUM_FONT * 6 / 10;
    int x = (UI_SCREEN_WIDTH - est_width) / 2;

    int ascent, descent, lg;
    font_manager_get_metrics(font, PAGE_NUM_FONT, &ascent, &descent, &lg);

    text_layout_render_line(fb, font, num_str, text_len,
                             x, PAGE_NUM_Y, 0, PAGE_NUM_FONT);
}

/* ---- 页面回调 ---- */

static void reader_on_enter(void *arg) {
    const char *path = (const char *)arg;
    if (!path) {
        ESP_LOGE(TAG, "No file path provided");
        return;
    }

    strncpy(s_file_path, path, sizeof(s_file_path) - 1);
    s_file_path[sizeof(s_file_path) - 1] = '\0';
    ESP_LOGI(TAG, "Opening: %s", s_file_path);

    s_params = text_layout_default_params();
    s_current_page = 0;
    s_index_loaded = false;
    memset(&s_index, 0, sizeof(s_index));

    /* 分配页面文本缓冲区 */
    if (!s_page_buf) {
        s_page_buf = heap_caps_malloc(PAGE_BUF_SIZE, MALLOC_CAP_SPIRAM);
    }

    /* 尝试加载缓存 */
    font_handle_t font = font_manager_get_default();
    if (font) {
        esp_err_t ret = paginator_load_cache(s_file_path, "/littlefs/wqy-microhei.ttc",
                                              &s_params, &s_index);
        if (ret == ESP_OK) {
            s_index_loaded = true;
            ESP_LOGI(TAG, "Cache loaded: %"PRIu32" pages", s_index.total_pages);
        } else {
            /* 构建分页表 */
            ret = paginator_build(s_file_path, font, &s_params, &s_index);
            if (ret == ESP_OK) {
                s_index_loaded = true;
            }
        }
    }

    s_full_render = true;
    ui_render_mark_full_dirty();
}

static void reader_on_exit(void) {
    paginator_free(&s_index);
    s_index_loaded = false;
    /* 保留 s_page_buf 以复用 */
}

static void reader_on_event(ui_event_t *event) {
    if (!s_index_loaded || s_index.total_pages == 0) return;

    switch (event->type) {

    case UI_EVENT_TOUCH_TAP:
        if (event->x < LEFT_ZONE_X) {
            /* 左半屏：上一页 */
            if (s_current_page > 0) {
                s_current_page--;
                s_full_render = true;
                ui_render_mark_full_dirty();
                ESP_LOGI(TAG, "Page %"PRIu32"/%"PRIu32, s_current_page + 1, s_index.total_pages);
            }
        } else {
            /* 右半屏：下一页 */
            if (s_current_page + 1 < s_index.total_pages) {
                s_current_page++;
                s_full_render = true;
                ui_render_mark_full_dirty();
                ESP_LOGI(TAG, "Page %"PRIu32"/%"PRIu32, s_current_page + 1, s_index.total_pages);
            }
        }
        break;

    case UI_EVENT_TOUCH_LONG_PRESS:
        ESP_LOGI(TAG, "Return to bookshelf");
        ui_page_pop();
        break;

    default:
        break;
    }
}

static void reader_on_render(uint8_t *fb) {
    if (!s_full_render) return;
    s_full_render = false;

    ui_canvas_fill(fb, UI_COLOR_WHITE);

    if (!s_index_loaded || s_index.total_pages == 0 || !s_page_buf) {
        /* 无内容 */
        ui_canvas_draw_rect(fb, 100, 400, 340, 80, UI_COLOR_MEDIUM, 2);
        return;
    }

    /* 读取当前页文本 */
    size_t len = paginator_read_page(s_file_path, &s_index,
                                      s_current_page, s_page_buf, PAGE_BUF_SIZE);
    if (len > 0) {
        font_handle_t font = font_manager_get_default();
        if (font) {
            text_layout_render_page(fb, font, s_page_buf, len, &s_params);
        }
    }

    /* 页码 */
    draw_page_number(fb);
}

ui_page_t reader_page = {
    .name      = "reader",
    .on_enter  = reader_on_enter,
    .on_exit   = reader_on_exit,
    .on_event  = reader_on_event,
    .on_render = reader_on_render,
};
