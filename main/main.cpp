/**
 * @file main.cpp
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 初始化板级硬件，通过 InkUI Application 启动事件循环。
 */

#include <cstdio>
#include <memory>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
}

#include "board.h"
#include "gt911.h"
#include "sd_storage.h"
#include "settings_store.h"
#include "ui_font.h"

#include "ink_ui/InkUI.h"
#include "controllers/BootViewController.h"

static const char* TAG = "parchment";

static ink::Application app;

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.6");
    ESP_LOGI(TAG, "  M5Stack PaperS3 (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    /* 0. NVS 初始化 */
    ESP_LOGI(TAG, "[0/5] NVS init...");
    if (settings_store_init() != ESP_OK) {
        ESP_LOGW(TAG, "NVS init failed, settings unavailable");
    }

    /* 1. 板级初始化 */
    ESP_LOGI(TAG, "[1/5] Board init...");
    board_init();

    /* 2. GT911 触摸初始化 */
    ESP_LOGI(TAG, "[2/5] Touch init...");
    gt911_config_t touch_cfg = {};
    touch_cfg.sda_gpio = BOARD_TOUCH_SDA;
    touch_cfg.scl_gpio = BOARD_TOUCH_SCL;
    touch_cfg.int_gpio = BOARD_TOUCH_INT;
    touch_cfg.rst_gpio = BOARD_TOUCH_RST;
    touch_cfg.i2c_port = BOARD_TOUCH_I2C_NUM;
    touch_cfg.i2c_freq = BOARD_TOUCH_I2C_FREQ;
    if (gt911_init(&touch_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed, touch unavailable");
    }

    /* 3. SD 卡挂载 */
    ESP_LOGI(TAG, "[3/5] SD card mount...");
    sd_storage_config_t sd_cfg = {};
    sd_cfg.miso_gpio = BOARD_SD_MISO;
    sd_cfg.mosi_gpio = BOARD_SD_MOSI;
    sd_cfg.clk_gpio = BOARD_SD_CLK;
    sd_cfg.cs_gpio = BOARD_SD_CS;
    if (sd_storage_mount(&sd_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
    }

    /* 4. 字体子系统初始化 */
    ESP_LOGI(TAG, "[4/5] Font init...");
    ui_font_init();

    /* 5. InkUI Application 启动 */
    ESP_LOGI(TAG, "[5/5] InkUI init...");
    if (!app.init()) {
        ESP_LOGE(TAG, "InkUI init failed!");
        return;
    }

    // 推入启动画面
    auto bootVC = std::make_unique<BootViewController>(app);
    app.navigator().push(std::move(bootVC));

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  InkUI running");
    ESP_LOGI(TAG, "========================================");

    // 进入主事件循环（永不返回）
    app.run();
}
