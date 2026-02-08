/**
 * @file main.c
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 初始化硬件和 UI 框架，推入字体渲染验证页面，启动事件循环。
 * 验证页面展示 4 档字号的中英文混排渲染效果，
 * 支持左右滑动切换字号。
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board.h"
#include "epd_driver.h"
#include "gt911.h"
#include "sd_storage.h"
#include "ui_core.h"
#include "ui_font.h"
#include "esp_littlefs.h"

static const char *TAG = "parchment";

/* ========================================================================== */
/*  字体渲染验证页面                                                            */
/* ========================================================================== */

/** 4 档预设字号对应的 .bin 文件路径（LittleFS 内置）。 */
static const char *FONT_PATHS[] = {
    "/fonts/fonts/songti_24.bin",
    "/fonts/fonts/songti_28.bin",
    "/fonts/fonts/songti_32.bin",
    "/fonts/fonts/songti_36.bin",
};
static const int FONT_SIZES[] = {24, 28, 32, 36};
#define FONT_COUNT  4

/** 演示文本：中英文混排。 */
static const char *DEMO_TEXT =
    "Parchment E-Reader\n"
    "\n"
    "天地玄黄，宇宙洪荒。日月盈昃，辰宿列张。"
    "寒来暑往，秋收冬藏。闰余成岁，律吕调阳。\n"
    "\n"
    "The quick brown fox jumps over the lazy dog. "
    "1234567890\n"
    "\n"
    "混排测试：Hello世界！This is a mixed中英文text。"
    "标点符号：，。！？、；：""''——……";

/** 页面状态。 */
static int s_font_idx = 0;

/** 布局常量。 */
#define MARGIN_X     24
#define MARGIN_TOP   60
#define CONTENT_W    (UI_SCREEN_WIDTH - 2 * MARGIN_X)

/**
 * @brief 加载当前字号的字体并标记全屏重绘。
 */
static void load_current_font(void) {
    esp_err_t ret = ui_font_load(FONT_PATHS[s_font_idx]);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load font: %s (err=%d)", FONT_PATHS[s_font_idx], ret);
    } else {
        ESP_LOGI(TAG, "Font loaded: %dpx", FONT_SIZES[s_font_idx]);
    }
}

/* ---- 页面回调 ---- */

static void font_page_on_enter(void *arg) {
    ESP_LOGI(TAG, "Font demo page entered");
    s_font_idx = 0;
    load_current_font();
    ui_render_request_clear();
    ui_render_mark_full_dirty();
}

static void font_page_on_exit(void) {
    ui_font_unload();
}

static void font_page_on_event(ui_event_t *event) {
    switch (event->type) {

    case UI_EVENT_TOUCH_SWIPE_LEFT:
        /* 下一档字号 */
        if (s_font_idx < FONT_COUNT - 1) {
            s_font_idx++;
            load_current_font();
            ui_render_mark_full_dirty();
        }
        break;

    case UI_EVENT_TOUCH_SWIPE_RIGHT:
        /* 上一档字号 */
        if (s_font_idx > 0) {
            s_font_idx--;
            load_current_font();
            ui_render_mark_full_dirty();
        }
        break;

    default:
        break;
    }
}

static void font_page_on_render(uint8_t *fb) {
    ui_canvas_fill(fb, UI_COLOR_WHITE);

    int font_h = ui_font_get_height();
    int line_h = font_h > 0 ? (font_h * 16 / 10) : 36; /* 行高 1.6 倍 */
    int y = MARGIN_TOP;

    /* 标题栏：显示当前字号信息 */
    char header[64];
    snprintf(header, sizeof(header), "Songti SC  %dpx  [%d/%d]",
             FONT_SIZES[s_font_idx], s_font_idx + 1, FONT_COUNT);

    if (font_h > 0) {
        ui_font_draw_text(fb, MARGIN_X, 16, CONTENT_W, line_h, header, UI_COLOR_DARK);
    }

    /* 分隔线 */
    ui_canvas_draw_hline(fb, MARGIN_X, y - 4, CONTENT_W, UI_COLOR_MEDIUM);

    /* 正文内容 */
    if (font_h > 0) {
        ui_font_draw_text(fb, MARGIN_X, y, CONTENT_W, line_h, DEMO_TEXT, UI_COLOR_BLACK);
    }

    /* 底部操作提示 */
    int hint_y = UI_SCREEN_HEIGHT - 40;
    ui_canvas_draw_hline(fb, MARGIN_X, hint_y - 8, CONTENT_W, UI_COLOR_MEDIUM);

    if (font_h > 0) {
        ui_font_draw_text(fb, MARGIN_X, hint_y, CONTENT_W, line_h,
                          "< swipe left/right >", UI_COLOR_MEDIUM);
    }
}

/** 字体验证页面定义。 */
static ui_page_t font_demo_page = {
    .name      = "font_demo",
    .on_enter  = font_page_on_enter,
    .on_exit   = font_page_on_exit,
    .on_event  = font_page_on_event,
    .on_render = font_page_on_render,
};

/* ========================================================================== */
/*  app_main                                                                   */
/* ========================================================================== */

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.3");
    ESP_LOGI(TAG, "  M5Stack PaperS3 (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    /* 1. 板级初始化 */
    ESP_LOGI(TAG, "[1/5] Board init...");
    board_init();

    /* 2. EPD 显示初始化 */
    ESP_LOGI(TAG, "[2/5] EPD init...");
    if (epd_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "EPD init failed!");
        return;
    }

    /* 3. GT911 触摸初始化 */
    ESP_LOGI(TAG, "[3/5] Touch init...");
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

    /* 4. 内置字体文件系统挂载 (LittleFS) */
    ESP_LOGI(TAG, "[4/6] Fonts LittleFS mount...");
    const esp_vfs_littlefs_conf_t fonts_conf = {
        .base_path = "/fonts",
        .partition_label = "fonts",
        .format_if_mount_failed = false,
        .dont_mount = false,
    };
    if (esp_vfs_littlefs_register(&fonts_conf) != ESP_OK) {
        ESP_LOGE(TAG, "Fonts LittleFS mount failed!");
    } else {
        size_t total = 0, used = 0;
        esp_littlefs_info("fonts", &total, &used);
        ESP_LOGI(TAG, "Fonts partition: %u KB used / %u KB total",
                 (unsigned)(used / 1024), (unsigned)(total / 1024));
    }

    /* 5. SD 卡挂载 */
    ESP_LOGI(TAG, "[5/6] SD card mount...");
    sd_storage_config_t sd_cfg = {
        .miso_gpio = BOARD_SD_MISO,
        .mosi_gpio = BOARD_SD_MOSI,
        .clk_gpio = BOARD_SD_CLK,
        .cs_gpio = BOARD_SD_CS,
    };
    if (sd_storage_mount(&sd_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
    }

    /* 6. UI 框架初始化 + 启动 */
    ESP_LOGI(TAG, "[6/6] UI init...");
    ui_core_init();
    ui_touch_start(BOARD_TOUCH_INT);
    ui_page_push(&font_demo_page, NULL);
    ui_core_run();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  All systems running");
    ESP_LOGI(TAG, "========================================");
}
