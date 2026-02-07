/**
 * @file main.c
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 主页面 (home): 书架，扫描 ePub 文件列表，点击打开。
 * 阅读页面 (reading): 分页渲染 ePub 章节文本。
 *   - SWIPE LEFT:  下一页
 *   - SWIPE RIGHT: 上一页
 *   - SWIPE DOWN:  下一章
 *   - SWIPE UP:    上一章
 *   - LONG PRESS:  返回书架
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board.h"
#include "epd_driver.h"
#include "gt911.h"
#include "sd_storage.h"
#include "ui_core.h"
#include "ui_text.h"
#include "epub_reader.h"

static const char *TAG = "parchment";

/* ========================================================================== */
/*  字体                                                                       */
/* ========================================================================== */

static const char *s_font_search_paths[] = {
    "/sdcard/font.ttf",
    "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/System/Library/Fonts/PingFang.ttc",
    "/System/Library/Fonts/Helvetica.ttc",
    "font.ttf",
    NULL,
};

static ui_font_t *s_font_title = NULL;  /* 32px */
static ui_font_t *s_font_body  = NULL;  /* 22px */
static ui_font_t *s_font_btn   = NULL;  /* 18px */

/* ========================================================================== */
/*  书架：ePub 文件扫描                                                         */
/* ========================================================================== */

/** 最大支持的图书数量 */
#define SHELF_MAX_BOOKS  32

/** 单本书的元数据（从 ePub 中提取）。 */
typedef struct {
    char filepath[256];
    char title[256];
    char author[256];
    int  chapter_count;
} shelf_book_t;

static shelf_book_t s_shelf_books[SHELF_MAX_BOOKS];
static int s_shelf_count = 0;

/** ePub 搜索目录列表 */
static const char *s_epub_search_files[] = {
    "/sdcard/book.epub",
    "/sdcard/alice.epub",
    "../../testdata/alice.epub", /* simulator: build/ -> 项目根/testdata/ */
    "../testdata/alice.epub",    /* simulator: 从 simulator/ 目录运行 */
    "testdata/alice.epub",       /* 从项目根目录运行 */
    "book.epub",
    "alice.epub",
    NULL,
};

/** 扫描一个文件，成功则加入书架 */
static void shelf_try_add(const char *path) {
    if (s_shelf_count >= SHELF_MAX_BOOKS) return;

    FILE *f = fopen(path, "rb");
    if (!f) return;
    fclose(f);

    /* 检查是否已添加 */
    for (int i = 0; i < s_shelf_count; i++) {
        if (strcmp(s_shelf_books[i].filepath, path) == 0) return;
    }

    /* 尝试打开获取元数据 */
    epub_book_t *book = epub_open(path);
    if (!book) return;

    shelf_book_t *entry = &s_shelf_books[s_shelf_count];
    snprintf(entry->filepath, sizeof(entry->filepath), "%s", path);
    snprintf(entry->title, sizeof(entry->title), "%s", epub_get_title(book));
    snprintf(entry->author, sizeof(entry->author), "%s", epub_get_author(book));
    entry->chapter_count = epub_get_chapter_count(book);
    epub_close(book);

    ESP_LOGI(TAG, "Shelf[%d]: \"%s\" by %s (%d ch) — %s",
             s_shelf_count, entry->title, entry->author,
             entry->chapter_count, entry->filepath);
    s_shelf_count++;
}

/** 扫描所有已知路径 */
static void shelf_scan(void) {
    s_shelf_count = 0;
    for (int i = 0; s_epub_search_files[i] != NULL; i++) {
        shelf_try_add(s_epub_search_files[i]);
    }
    ESP_LOGI(TAG, "Shelf scan complete: %d book(s) found", s_shelf_count);
}

/* ========================================================================== */
/*  阅读页面                                                                    */
/* ========================================================================== */

#define READ_MARGIN_X     30
#define READ_HEADER_Y     12
#define READ_BODY_Y       55
#define READ_BODY_W       (UI_SCREEN_WIDTH - 2 * READ_MARGIN_X)
#define READ_BODY_H       855
#define READ_FOOTER_Y     920
#define READ_LINE_SPACING 2
#define READ_MAX_PAGES    4096

static epub_book_t *s_book = NULL;
static int  s_chapter_idx  = 0;
static char *s_chapter_text = NULL;

static int  s_page_offsets[READ_MAX_PAGES];
static int  s_page_count   = 0;
static int  s_page_idx     = 0;
static bool s_read_full_render = true;

static void reading_free_chapter(void) {
    if (s_chapter_text) {
        free(s_chapter_text);
        s_chapter_text = NULL;
    }
    s_page_count = 0;
    s_page_idx = 0;
}

static void reading_paginate(void) {
    s_page_count = 0;
    if (!s_chapter_text || !s_font_body) return;

    const char *p = s_chapter_text;
    s_page_offsets[0] = 0;

    while (*p && s_page_count < READ_MAX_PAGES - 1) {
        int consumed = ui_text_paginate(s_font_body, p,
                                         READ_BODY_W, READ_BODY_H,
                                         READ_LINE_SPACING);
        if (consumed <= 0) break;

        p += consumed;
        while (*p == ' ') p++;

        s_page_count++;
        s_page_offsets[s_page_count] = (int)(p - s_chapter_text);
    }

    if (s_page_count == 0 && s_chapter_text[0]) {
        s_page_count = 1;
    }

    s_page_idx = 0;
    ESP_LOGI(TAG, "Chapter %d: %d pages", s_chapter_idx + 1, s_page_count);
}

static bool reading_load_chapter(int chapter_idx) {
    if (!s_book) return false;
    int count = epub_get_chapter_count(s_book);
    if (chapter_idx < 0 || chapter_idx >= count) return false;

    reading_free_chapter();
    s_chapter_idx = chapter_idx;
    s_chapter_text = epub_get_chapter_text(s_book, chapter_idx);
    if (!s_chapter_text) {
        ESP_LOGE(TAG, "Failed to load chapter %d", chapter_idx);
        return false;
    }

    reading_paginate();
    return true;
}

static void reading_draw(uint8_t *fb) {
    ui_canvas_fill(fb, UI_COLOR_WHITE);

    if (!s_font_body || !s_chapter_text) {
        if (s_font_body) {
            ui_text_draw_line(fb, s_font_body, READ_MARGIN_X, 100,
                              "No content available.", UI_COLOR_BLACK);
        }
        return;
    }

    /* 顶部栏：书名 + 章节号 */
    if (s_font_btn) {
        const char *title = s_book ? epub_get_title(s_book) : "Book";
        ui_text_draw_line(fb, s_font_btn, READ_MARGIN_X, READ_HEADER_Y,
                          title, UI_COLOR_DARK);

        char ch_info[64];
        snprintf(ch_info, sizeof(ch_info), "Ch %d/%d",
                 s_chapter_idx + 1, epub_get_chapter_count(s_book));
        int cw = ui_text_measure_width(s_font_btn, ch_info);
        ui_text_draw_line(fb, s_font_btn,
                          UI_SCREEN_WIDTH - READ_MARGIN_X - cw, READ_HEADER_Y,
                          ch_info, UI_COLOR_DARK);
    }

    ui_canvas_draw_hline(fb, READ_MARGIN_X, READ_BODY_Y - 8,
                         UI_SCREEN_WIDTH - 2 * READ_MARGIN_X, UI_COLOR_LIGHT);

    /* 正文 */
    const char *page_text = s_chapter_text + s_page_offsets[s_page_idx];
    ui_text_draw_wrapped(fb, s_font_body,
                         READ_MARGIN_X, READ_BODY_Y,
                         READ_BODY_W, READ_BODY_H,
                         page_text, UI_COLOR_BLACK,
                         UI_TEXT_ALIGN_LEFT, READ_LINE_SPACING);

    /* 底部栏：页码 */
    ui_canvas_draw_hline(fb, READ_MARGIN_X, READ_FOOTER_Y - 5,
                         UI_SCREEN_WIDTH - 2 * READ_MARGIN_X, UI_COLOR_LIGHT);

    if (s_font_btn && s_page_count > 0) {
        char page_info[64];
        snprintf(page_info, sizeof(page_info), "%d / %d",
                 s_page_idx + 1, s_page_count);
        int pw = ui_text_measure_width(s_font_btn, page_info);
        ui_text_draw_line(fb, s_font_btn,
                          (UI_SCREEN_WIDTH - pw) / 2, READ_FOOTER_Y,
                          page_info, UI_COLOR_MEDIUM);
    }
}

static void reading_on_enter(void *arg) {
    ESP_LOGI(TAG, "Reading page entered");
    s_read_full_render = true;
    ui_render_mark_full_dirty();
}

static void reading_on_exit(void) {
    reading_free_chapter();
    if (s_book) {
        epub_close(s_book);
        s_book = NULL;
    }
}

static void reading_on_event(ui_event_t *event) {
    switch (event->type) {

    case UI_EVENT_TOUCH_SWIPE_LEFT:
        if (s_page_idx < s_page_count - 1) {
            s_page_idx++;
            s_read_full_render = true;
            ui_render_mark_full_dirty();
            ESP_LOGI(TAG, "Page -> %d/%d", s_page_idx + 1, s_page_count);
        } else {
            int next = s_chapter_idx + 1;
            if (s_book && next < epub_get_chapter_count(s_book)) {
                reading_load_chapter(next);
                s_read_full_render = true;
                ui_render_mark_full_dirty();
            }
        }
        break;

    case UI_EVENT_TOUCH_SWIPE_RIGHT:
        if (s_page_idx > 0) {
            s_page_idx--;
            s_read_full_render = true;
            ui_render_mark_full_dirty();
            ESP_LOGI(TAG, "Page -> %d/%d", s_page_idx + 1, s_page_count);
        } else {
            int prev = s_chapter_idx - 1;
            if (s_book && prev >= 0) {
                reading_load_chapter(prev);
                if (s_page_count > 0) s_page_idx = s_page_count - 1;
                s_read_full_render = true;
                ui_render_mark_full_dirty();
            }
        }
        break;

    case UI_EVENT_TOUCH_SWIPE_DOWN:
        if (s_book && s_chapter_idx + 1 < epub_get_chapter_count(s_book)) {
            reading_load_chapter(s_chapter_idx + 1);
            s_read_full_render = true;
            ui_render_mark_full_dirty();
        }
        break;

    case UI_EVENT_TOUCH_SWIPE_UP:
        if (s_book && s_chapter_idx > 0) {
            reading_load_chapter(s_chapter_idx - 1);
            s_read_full_render = true;
            ui_render_mark_full_dirty();
        }
        break;

    case UI_EVENT_TOUCH_LONG_PRESS:
        ESP_LOGI(TAG, "Long press -> back to shelf");
        ui_page_pop();
        break;

    default:
        break;
    }
}

static void reading_on_render(uint8_t *fb) {
    if (s_read_full_render) {
        s_read_full_render = false;
        reading_draw(fb);
    }
}

static ui_page_t reading_page = {
    .name = "reading",
    .on_enter = reading_on_enter,
    .on_exit = reading_on_exit,
    .on_event = reading_on_event,
    .on_render = reading_on_render,
};

/* ========================================================================== */
/*  书架页面 (home)                                                             */
/* ========================================================================== */

/** 书架布局常量 */
#define SHELF_MARGIN      30
#define SHELF_TITLE_Y     20
#define SHELF_LIST_Y      75
#define SHELF_CARD_H      120
#define SHELF_CARD_GAP    15
#define SHELF_CARD_W      (UI_SCREEN_WIDTH - 2 * SHELF_MARGIN)
#define SHELF_CARD_PAD    15
#define SHELF_CARD_BORDER 2

static bool s_home_full_render = true;

/** 命中测试：返回点击到的书本索引，-1 表示未命中 */
static int shelf_hit_test(int tx, int ty) {
    for (int i = 0; i < s_shelf_count; i++) {
        int cy = SHELF_LIST_Y + i * (SHELF_CARD_H + SHELF_CARD_GAP);
        if (tx >= SHELF_MARGIN && tx < SHELF_MARGIN + SHELF_CARD_W &&
            ty >= cy && ty < cy + SHELF_CARD_H) {
            return i;
        }
    }
    return -1;
}

/** 打开指定索引的书 */
static void shelf_open_book(int idx) {
    if (idx < 0 || idx >= s_shelf_count) return;

    const char *path = s_shelf_books[idx].filepath;
    ESP_LOGI(TAG, "Opening: %s", path);

    s_book = epub_open(path);
    if (!s_book) {
        ESP_LOGE(TAG, "Failed to open ePub: %s", path);
        return;
    }

    ESP_LOGI(TAG, "Opened: \"%s\" by %s (%d chapters)",
             epub_get_title(s_book), epub_get_author(s_book),
             epub_get_chapter_count(s_book));

    if (reading_load_chapter(0)) {
        ui_page_push(&reading_page, NULL);
    } else {
        epub_close(s_book);
        s_book = NULL;
    }
}

/** 绘制单个书卡 */
static void draw_book_card(uint8_t *fb, int idx) {
    shelf_book_t *book = &s_shelf_books[idx];
    int cx = SHELF_MARGIN;
    int cy = SHELF_LIST_Y + idx * (SHELF_CARD_H + SHELF_CARD_GAP);

    /* 卡片背景 + 边框 */
    ui_canvas_fill_rect(fb, cx, cy, SHELF_CARD_W, SHELF_CARD_H, UI_COLOR_WHITE);
    ui_canvas_draw_rect(fb, cx, cy, SHELF_CARD_W, SHELF_CARD_H,
                        UI_COLOR_BLACK, SHELF_CARD_BORDER);

    /* 左侧书脊装饰条 */
    ui_canvas_fill_rect(fb, cx + SHELF_CARD_BORDER, cy + SHELF_CARD_BORDER,
                        8, SHELF_CARD_H - 2 * SHELF_CARD_BORDER, UI_COLOR_DARK);

    int text_x = cx + SHELF_CARD_PAD + 12;
    int text_w = SHELF_CARD_W - SHELF_CARD_PAD * 2 - 12;

    /* 书名 */
    if (s_font_body) {
        ui_text_draw_wrapped(fb, s_font_body,
                             text_x, cy + SHELF_CARD_PAD,
                             text_w, 52,
                             book->title, UI_COLOR_BLACK,
                             UI_TEXT_ALIGN_LEFT, 0);
    }

    /* 作者 */
    if (s_font_btn) {
        int author_y = cy + SHELF_CARD_PAD + 55;
        ui_text_draw_line(fb, s_font_btn, text_x, author_y,
                          book->author, UI_COLOR_MEDIUM);
    }

    /* 章节数 */
    if (s_font_btn) {
        char info[32];
        snprintf(info, sizeof(info), "%d chapters", book->chapter_count);
        int iw = ui_text_measure_width(s_font_btn, info);
        ui_text_draw_line(fb, s_font_btn,
                          cx + SHELF_CARD_W - SHELF_CARD_PAD - iw,
                          cy + SHELF_CARD_H - SHELF_CARD_PAD - ui_font_line_height(s_font_btn),
                          info, UI_COLOR_LIGHT);
    }
}

/** 绘制整个书架页面 */
static void draw_shelf(uint8_t *fb) {
    ui_canvas_fill(fb, UI_COLOR_WHITE);

    /* 标题 */
    if (s_font_title) {
        const char *title = "Parchment";
        int tw = ui_text_measure_width(s_font_title, title);
        int tx = (UI_SCREEN_WIDTH - tw) / 2;
        ui_text_draw_line(fb, s_font_title, tx, SHELF_TITLE_Y, title, UI_COLOR_BLACK);
    }

    /* 分隔线 */
    ui_canvas_draw_hline(fb, SHELF_MARGIN, SHELF_LIST_Y - 10,
                         UI_SCREEN_WIDTH - 2 * SHELF_MARGIN, UI_COLOR_MEDIUM);

    if (s_shelf_count == 0) {
        /* 空书架提示 */
        if (s_font_body) {
            const char *msg = "No books found.";
            int mw = ui_text_measure_width(s_font_body, msg);
            ui_text_draw_line(fb, s_font_body,
                              (UI_SCREEN_WIDTH - mw) / 2, 200,
                              msg, UI_COLOR_MEDIUM);

            ui_text_draw_wrapped(fb, s_font_btn,
                                 SHELF_MARGIN, 250,
                                 UI_SCREEN_WIDTH - 2 * SHELF_MARGIN, 200,
                                 "Place .epub files on the SD card "
                                 "(/sdcard/) or in the working directory.",
                                 UI_COLOR_MEDIUM, UI_TEXT_ALIGN_CENTER, 2);
        }
        return;
    }

    /* 书卡列表 */
    for (int i = 0; i < s_shelf_count; i++) {
        draw_book_card(fb, i);
    }

    /* 底部提示 */
    if (s_font_btn) {
        const char *hint = "Tap to open";
        int hw = ui_text_measure_width(s_font_btn, hint);
        int hy = SHELF_LIST_Y + s_shelf_count * (SHELF_CARD_H + SHELF_CARD_GAP) + 10;
        if (hy < UI_SCREEN_HEIGHT - 40) {
            ui_text_draw_line(fb, s_font_btn,
                              (UI_SCREEN_WIDTH - hw) / 2, hy,
                              hint, UI_COLOR_LIGHT);
        }
    }
}

static void home_on_enter(void *arg) {
    ESP_LOGI(TAG, "Shelf page entered");
    s_home_full_render = true;

    /* 每次进入书架都重新扫描 */
    shelf_scan();
    ui_render_mark_full_dirty();
}

static void home_on_event(ui_event_t *event) {
    switch (event->type) {

    case UI_EVENT_TOUCH_TAP: {
        int idx = shelf_hit_test(event->x, event->y);
        if (idx >= 0) {
            ESP_LOGI(TAG, "Tap -> open book %d: \"%s\"",
                     idx, s_shelf_books[idx].title);
            shelf_open_book(idx);
        }
        break;
    }

    default:
        break;
    }
}

static void home_on_render(uint8_t *fb) {
    if (s_home_full_render) {
        s_home_full_render = false;
        draw_shelf(fb);
    }
}

static ui_page_t home_page = {
    .name = "home",
    .on_enter = home_on_enter,
    .on_exit = NULL,
    .on_event = home_on_event,
    .on_render = home_on_render,
};

/* ========================================================================== */
/*  app_main                                                                   */
/* ========================================================================== */

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.5");
    ESP_LOGI(TAG, "  M5Stack PaperS3 (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    /* 1. 板级初始化 */
    ESP_LOGI(TAG, "[1/6] Board init...");
    board_init();

    /* 2. EPD 显示初始化 */
    ESP_LOGI(TAG, "[2/6] EPD init...");
    if (epd_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "EPD init failed!");
        return;
    }

    /* 3. GT911 触摸初始化 */
    ESP_LOGI(TAG, "[3/6] Touch init...");
    gt911_config_t touch_cfg = {
        .sda_gpio = BOARD_TOUCH_SDA,
        .scl_gpio = BOARD_TOUCH_SCL,
        .int_gpio = BOARD_TOUCH_INT,
        .rst_gpio = BOARD_TOUCH_RST,
        .i2c_port = BOARD_TOUCH_I2C_NUM,
        .i2c_freq = BOARD_TOUCH_I2C_FREQ,
    };
    if (gt911_init(&touch_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed, touch unavailable");
    }

    /* 4. SD 卡挂载 */
    ESP_LOGI(TAG, "[4/6] SD card mount...");
    sd_storage_config_t sd_cfg = {
        .miso_gpio = BOARD_SD_MISO,
        .mosi_gpio = BOARD_SD_MOSI,
        .clk_gpio = BOARD_SD_CLK,
        .cs_gpio = BOARD_SD_CS,
    };
    if (sd_storage_mount(&sd_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
    }

    /* 5. 加载字体 */
    ESP_LOGI(TAG, "[5/6] Loading fonts...");
    {
        const char *font_path = NULL;
        for (int i = 0; s_font_search_paths[i] != NULL; i++) {
            FILE *f = fopen(s_font_search_paths[i], "rb");
            if (f) {
                fclose(f);
                font_path = s_font_search_paths[i];
                break;
            }
        }

        if (font_path) {
            ESP_LOGI(TAG, "Font found: %s", font_path);
            s_font_title = ui_font_load_file(font_path, 32.0f);
            s_font_body  = ui_font_load_file(font_path, 22.0f);
            s_font_btn   = ui_font_load_file(font_path, 18.0f);

            if (s_font_title && s_font_body && s_font_btn) {
                ESP_LOGI(TAG, "Fonts loaded: title=%dpx body=%dpx btn=%dpx",
                         ui_font_line_height(s_font_title),
                         ui_font_line_height(s_font_body),
                         ui_font_line_height(s_font_btn));
            }
        } else {
            ESP_LOGW(TAG, "No font file found, text rendering disabled");
        }
    }

    /* 6. UI 框架初始化 + 启动 */
    ESP_LOGI(TAG, "[6/6] UI init...");
    ui_core_init();
    ui_touch_start(BOARD_TOUCH_INT);
    ui_page_push(&home_page, NULL);
    ui_core_run();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  All systems running");
    ESP_LOGI(TAG, "========================================");
}
