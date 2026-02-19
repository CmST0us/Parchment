/**
 * @file main.cpp
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 通过 InkUI Application 启动事件循环。
 * M5.begin() 和字体引擎初始化由 Application::init() 内部完成。
 */

#include <cstdio>
#include <memory>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_littlefs.h"
}

#include "sd_storage.h"
#include "settings_store.h"

#include "ink_ui/InkUI.h"
#include "controllers/BootViewController.h"

static const char* TAG = "parchment";

static ink::Application app;

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.7");
    ESP_LOGI(TAG, "  M5Stack PaperS3 (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    /* 0. NVS 初始化 */
    ESP_LOGI(TAG, "[0/3] NVS init...");
    if (settings_store_init() != ESP_OK) {
        ESP_LOGW(TAG, "NVS init failed, settings unavailable");
    }

    /* 0.5. LittleFS 挂载（字体分区）*/
    esp_vfs_littlefs_conf_t lfs_conf = {};
    lfs_conf.base_path = "/littlefs";
    lfs_conf.partition_label = "fonts";
    lfs_conf.format_if_mount_failed = false;
    lfs_conf.dont_mount = false;
    if (esp_vfs_littlefs_register(&lfs_conf) != ESP_OK) {
        ESP_LOGW(TAG, "LittleFS mount failed, fonts unavailable");
    } else {
        ESP_LOGI(TAG, "LittleFS mounted at /littlefs");
    }

    /* 1. SD 卡挂载 */
    ESP_LOGI(TAG, "[1/3] SD card mount...");
    sd_storage_config_t sd_cfg = {};
    sd_cfg.miso_gpio = 13;
    sd_cfg.mosi_gpio = 11;
    sd_cfg.clk_gpio = 12;
    sd_cfg.cs_gpio = 14;
    if (sd_storage_mount(&sd_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
    }

    /* 2. InkUI Application 启动 */
    // M5.begin(), 字体引擎, RenderEngine, GestureRecognizer 都在 init() 中初始化
    ESP_LOGI(TAG, "[2/3] InkUI init...");
    if (!app.init()) {
        ESP_LOGE(TAG, "InkUI init failed!");
        return;
    }

    /* 3. 推入启动画面 */
    ESP_LOGI(TAG, "[3/3] Launching...");
    auto bootVC = std::make_unique<BootViewController>(app);
    app.navigator().push(std::move(bootVC));

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  InkUI running");
    ESP_LOGI(TAG, "========================================");

    // 进入主事件循环（永不返回）
    app.run();
}
