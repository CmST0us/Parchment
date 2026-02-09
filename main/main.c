/**
 * @file main.c
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 初始化板级硬件（EPD、触摸、SD 卡）和 ui_core 框架，启动事件循环。
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board.h"
#include "epd_driver.h"
#include "gt911.h"
#include "sd_storage.h"
#include "ui_core.h"

static const char *TAG = "parchment";

/* ========================================================================== */
/*  临时验证页面：触摸坐标验证                                                  */
/* ========================================================================== */

#define CROSS_SIZE 20

static int s_last_x, s_last_y;
static bool s_has_new_mark;
static bool s_need_full;

static void touch_test_on_enter(void *arg) {
    ESP_LOGI(TAG, "Touch test page entered — tap to place cross marks");
    s_has_new_mark = false;
    s_need_full = true;
    ui_render_mark_full_dirty();
}

static void touch_test_on_event(ui_event_t *event) {
    if (event->type == UI_EVENT_TOUCH_TAP) {
        ESP_LOGI(TAG, "Tap at (%d, %d)", event->x, event->y);
        s_last_x = event->x;
        s_last_y = event->y;
        s_has_new_mark = true;
        /* 只标记 X 标记周围区域为脏 */
        ui_render_mark_dirty(event->x - CROSS_SIZE - 1, event->y - CROSS_SIZE - 1,
                             CROSS_SIZE * 2 + 3, CROSS_SIZE * 2 + 3);
    } else if (event->type == UI_EVENT_TOUCH_LONG_PRESS) {
        ESP_LOGI(TAG, "Long press — clear all marks");
        s_has_new_mark = false;
        s_need_full = true;
        ui_render_mark_full_dirty();
    }
}

static void draw_reference_marks(uint8_t *fb) {
    int margin = 30;
    /* 左上 */
    ui_canvas_draw_hline(fb, 0, margin, margin * 2, UI_COLOR_MEDIUM);
    ui_canvas_draw_vline(fb, margin, 0, margin * 2, UI_COLOR_MEDIUM);
    /* 右上 */
    ui_canvas_draw_hline(fb, UI_SCREEN_WIDTH - margin * 2, margin, margin * 2, UI_COLOR_MEDIUM);
    ui_canvas_draw_vline(fb, UI_SCREEN_WIDTH - 1 - margin, 0, margin * 2, UI_COLOR_MEDIUM);
    /* 左下 */
    ui_canvas_draw_hline(fb, 0, UI_SCREEN_HEIGHT - 1 - margin, margin * 2, UI_COLOR_MEDIUM);
    ui_canvas_draw_vline(fb, margin, UI_SCREEN_HEIGHT - margin * 2, margin * 2, UI_COLOR_MEDIUM);
    /* 右下 */
    ui_canvas_draw_hline(fb, UI_SCREEN_WIDTH - margin * 2, UI_SCREEN_HEIGHT - 1 - margin, margin * 2, UI_COLOR_MEDIUM);
    ui_canvas_draw_vline(fb, UI_SCREEN_WIDTH - 1 - margin, UI_SCREEN_HEIGHT - margin * 2, margin * 2, UI_COLOR_MEDIUM);
    /* 中心 */
    int cx = UI_SCREEN_WIDTH / 2, cy = UI_SCREEN_HEIGHT / 2;
    ui_canvas_draw_hline(fb, cx - CROSS_SIZE, cy, CROSS_SIZE * 2 + 1, UI_COLOR_MEDIUM);
    ui_canvas_draw_vline(fb, cx, cy - CROSS_SIZE, CROSS_SIZE * 2 + 1, UI_COLOR_MEDIUM);
}

static void draw_cross(uint8_t *fb, int mx, int my) {
    ui_canvas_draw_line(fb, mx - CROSS_SIZE, my - CROSS_SIZE,
                        mx + CROSS_SIZE, my + CROSS_SIZE, UI_COLOR_BLACK);
    ui_canvas_draw_line(fb, mx + CROSS_SIZE, my - CROSS_SIZE,
                        mx - CROSS_SIZE, my + CROSS_SIZE, UI_COLOR_BLACK);
}

static void touch_test_on_render(uint8_t *fb) {
    if (s_need_full) {
        s_need_full = false;
        ui_canvas_fill(fb, UI_COLOR_WHITE);
        draw_reference_marks(fb);
        return;
    }

    if (s_has_new_mark) {
        s_has_new_mark = false;
        draw_cross(fb, s_last_x, s_last_y);
    }
}

static ui_page_t touch_test_page = {
    .name = "touch_test",
    .on_enter = touch_test_on_enter,
    .on_exit = NULL,
    .on_event = touch_test_on_event,
    .on_render = touch_test_on_render,
};

/* ========================================================================== */
/*  app_main                                                                   */
/* ========================================================================== */

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.2");
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
    ESP_LOGI(TAG, "[2/5] EPD init done, fullclear should have executed");

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

    /* 4. SD 卡挂载 */
    ESP_LOGI(TAG, "[4/5] SD card mount...");
    sd_storage_config_t sd_cfg = {
        .miso_gpio = BOARD_SD_MISO,
        .mosi_gpio = BOARD_SD_MOSI,
        .clk_gpio = BOARD_SD_CLK,
        .cs_gpio = BOARD_SD_CS,
    };
    if (sd_storage_mount(&sd_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
    }

    /* 5. UI 框架初始化 + 启动 */
    ESP_LOGI(TAG, "[5/5] UI init...");
    ui_core_init();
    ui_touch_start(BOARD_TOUCH_INT);
    ui_page_push(&touch_test_page, NULL);
    ui_core_run();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  All systems running");
    ESP_LOGI(TAG, "========================================");
}
