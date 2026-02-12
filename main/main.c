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
#include "settings_store.h"
#include "ui_core.h"
#include "ui_event.h"
#include "ui_font.h"
#include "page_boot.h"

static const char *TAG = "parchment";

/* ========================================================================== */
/*  app_main                                                                   */
/* ========================================================================== */

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.2");
    ESP_LOGI(TAG, "  M5Stack PaperS3 (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    /* 0. NVS 初始化（settings_store 依赖） */
    ESP_LOGI(TAG, "[0/6] NVS init...");
    if (settings_store_init() != ESP_OK) {
        ESP_LOGW(TAG, "NVS init failed, settings unavailable");
    }

    /* 1. 板级初始化 */
    ESP_LOGI(TAG, "[1/6] Board init...");
    board_init();

    /* 2. EPD 显示初始化 */
    ESP_LOGI(TAG, "[2/6] EPD init...");
    if (epd_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "EPD init failed!");
        return;
    }
    ESP_LOGI(TAG, "[2/6] EPD init done, fullclear should have executed");

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

    /* 5. 字体子系统初始化（LittleFS 挂载 + 扫描阅读字体） */
    ESP_LOGI(TAG, "[5/6] Font init...");
    ui_font_init();

    /* 6. UI 框架初始化 + 启动 */
    ESP_LOGI(TAG, "[6/6] UI init...");
    ui_core_init();
    ui_touch_start(BOARD_TOUCH_INT);
    ui_page_push(&page_boot, NULL);
    ui_core_run();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  All systems running");
    ESP_LOGI(TAG, "========================================");
}
