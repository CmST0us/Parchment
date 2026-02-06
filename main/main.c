/**
 * @file main.c
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 演示流程: 板级初始化 → EPD 初始化/清屏 → 绘制测试图案 →
 *           触摸初始化/坐标打印 → SD 卡挂载/文件列表
 */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board.h"
#include "epd_driver.h"
#include "gt911.h"
#include "sd_storage.h"

static const char *TAG = "parchment";

/**
 * @brief 在屏幕上绘制测试图案：灰度渐变条和边框。
 */
static void draw_test_pattern(void) {
    uint8_t *fb = epd_driver_get_framebuffer();
    if (fb == NULL) {
        return;
    }

    /* 清空为白色 */
    epd_driver_set_all_white();

    /* 绘制 16 级灰度渐变条 (屏幕上方 1/3 区域) */
    int bar_width = BOARD_EPD_WIDTH / 16;
    int bar_height = BOARD_EPD_HEIGHT / 3;
    for (int i = 0; i < 16; i++) {
        uint8_t color = (uint8_t)(i * 17);  /* 0x00 ~ 0xFF */
        epd_driver_fill_rect(i * bar_width, 20, bar_width, bar_height, color);
    }

    /* 绘制屏幕边框 (2px) */
    epd_driver_fill_rect(0, 0, BOARD_EPD_WIDTH, 2, 0x00);                           /* 上 */
    epd_driver_fill_rect(0, BOARD_EPD_HEIGHT - 2, BOARD_EPD_WIDTH, 2, 0x00);        /* 下 */
    epd_driver_fill_rect(0, 0, 2, BOARD_EPD_HEIGHT, 0x00);                           /* 左 */
    epd_driver_fill_rect(BOARD_EPD_WIDTH - 2, 0, 2, BOARD_EPD_HEIGHT, 0x00);        /* 右 */

    /* 刷新到屏幕 */
    epd_driver_update_screen();
    ESP_LOGI(TAG, "Test pattern drawn");
}

/**
 * @brief 列举 SD 卡根目录文件。
 */
static void list_sd_files(void) {
    DIR *dir = opendir(SD_MOUNT_POINT);
    if (dir == NULL) {
        ESP_LOGW(TAG, "Failed to open SD card directory");
        return;
    }

    ESP_LOGI(TAG, "SD card files:");
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        ESP_LOGI(TAG, "  [%s] %s",
                 (entry->d_type == DT_DIR) ? "DIR " : "FILE",
                 entry->d_name);
        count++;
    }
    closedir(dir);
    ESP_LOGI(TAG, "Total: %d entries", count);
}

/**
 * @brief 触摸事件轮询任务（演示用，打印坐标后退出）。
 */
static void touch_demo_task(void *arg) {
    ESP_LOGI(TAG, "Touch demo: touch the screen within 10 seconds...");
    gt911_touch_point_t point;

    for (int i = 0; i < 100; i++) {  /* 轮询 10 秒 */
        if (gt911_read(&point) == ESP_OK && point.pressed) {
            ESP_LOGI(TAG, "Touch detected at (%u, %u)", point.x, point.y);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGI(TAG, "Touch demo finished");
    vTaskDelete(NULL);
}

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.1");
    ESP_LOGI(TAG, "  M5Stack PaperS3 (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    /* 1. 板级初始化 */
    ESP_LOGI(TAG, "[1/4] Board init...");
    board_init();

    /* 2. EPD 显示初始化 + 测试图案 */
    ESP_LOGI(TAG, "[2/4] EPD init...");
    if (epd_driver_init() == ESP_OK) {
        draw_test_pattern();
    } else {
        ESP_LOGE(TAG, "EPD init failed, continuing without display");
    }

    /* 3. GT911 触摸初始化 */
    ESP_LOGI(TAG, "[3/4] Touch init...");
    gt911_config_t touch_cfg = {
        .sda_gpio = BOARD_TOUCH_SDA,
        .scl_gpio = BOARD_TOUCH_SCL,
        .int_gpio = BOARD_TOUCH_INT,
        .rst_gpio = BOARD_TOUCH_RST,
        .i2c_port = BOARD_TOUCH_I2C_NUM,
        .i2c_freq = BOARD_TOUCH_I2C_FREQ,
    };
    if (gt911_init(&touch_cfg) == ESP_OK) {
        xTaskCreate(touch_demo_task, "touch_demo", 4096, NULL, 5, NULL);
    } else {
        ESP_LOGW(TAG, "Touch init failed, touch unavailable");
    }

    /* 4. SD 卡挂载 */
    ESP_LOGI(TAG, "[4/4] SD card mount...");
    sd_storage_config_t sd_cfg = {
        .miso_gpio = BOARD_SD_MISO,
        .mosi_gpio = BOARD_SD_MOSI,
        .clk_gpio = BOARD_SD_CLK,
        .cs_gpio = BOARD_SD_CS,
    };
    if (sd_storage_mount(&sd_cfg) == ESP_OK) {
        list_sd_files();
    } else {
        ESP_LOGW(TAG, "SD card not available");
    }

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  All systems initialized");
    ESP_LOGI(TAG, "========================================");
}
