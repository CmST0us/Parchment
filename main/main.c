/**
 * @file main.c
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 初始化硬件、LittleFS 字体存储、字体管理器和 UI 框架，
 * 推入书架页面，启动事件循环。
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
#include "littlefs_storage.h"
#include "font_manager.h"
#include "ui_core.h"

#include "pages/bookshelf_page.h"

static const char *TAG = "parchment";

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.3");
    ESP_LOGI(TAG, "  M5Stack PaperS3 (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    /* 1. 板级初始化 */
    ESP_LOGI(TAG, "[1/7] Board init...");
    board_init();

    /* 2. EPD 显示初始化 */
    ESP_LOGI(TAG, "[2/7] EPD init...");
    if (epd_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "EPD init failed!");
        return;
    }
    epd_driver_clear();

    /* 3. GT911 触摸初始化 */
    ESP_LOGI(TAG, "[3/7] Touch init...");
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
    ESP_LOGI(TAG, "[4/7] SD card mount...");
    sd_storage_config_t sd_cfg = {
        .miso_gpio = BOARD_SD_MISO,
        .mosi_gpio = BOARD_SD_MOSI,
        .clk_gpio = BOARD_SD_CLK,
        .cs_gpio = BOARD_SD_CS,
    };
    if (sd_storage_mount(&sd_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
    }

    /* 5. LittleFS 内置字体挂载 */
    ESP_LOGI(TAG, "[5/7] LittleFS mount...");
    if (littlefs_storage_mount() != ESP_OK) {
        ESP_LOGW(TAG, "LittleFS mount failed");
    }

    /* 6. 字体管理器初始化 */
    ESP_LOGI(TAG, "[6/7] Font manager init...");
    if (font_manager_init() != ESP_OK) {
        ESP_LOGW(TAG, "Font manager init failed");
    }

    /* 7. UI 框架初始化 + 启动 */
    ESP_LOGI(TAG, "[7/7] UI init...");
    ui_core_init();
    ui_touch_start(BOARD_TOUCH_INT);
    ui_page_push(&bookshelf_page, NULL);
    ui_core_run();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  All systems running");
    ESP_LOGI(TAG, "========================================");
}
