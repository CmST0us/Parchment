/**
 * @file main.cpp
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 初始化板级硬件（EPD、触摸、SD 卡）和 ui_core 框架，启动事件循环。
 * 阶段 1: Canvas 绘图引擎验证，保持旧 ui_core 运行。
 */

#include <cstdio>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
}

#include "board.h"
#include "epd_driver.h"
#include "gt911.h"
#include "sd_storage.h"
#include "settings_store.h"
#include "ui_core.h"
#include "ui_event.h"
#include "ui_font.h"
#include "page_boot.h"

// InkUI 头文件 — 阶段 0 编译验证
#include "ink_ui/InkUI.h"

static const char *TAG = "parchment";

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.3 (C++)");
    ESP_LOGI(TAG, "  M5Stack PaperS3 (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    // InkUI Geometry 验证
    ink::Rect testRect = {10, 20, 100, 50};
    ESP_LOGI(TAG, "InkUI Geometry OK: Rect(%d,%d,%d,%d) area=%d",
             testRect.x, testRect.y, testRect.w, testRect.h, testRect.area());

    // Canvas 验证将在字体初始化后执行（需要 framebuffer 和字体）

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

    // InkUI EpdDriver wrapper 验证
    auto& inkEpd = ink::EpdDriver::instance();
    ESP_LOGI(TAG, "InkUI EpdDriver OK: initialized=%d, fb=%p",
             inkEpd.isInitialized(), inkEpd.framebuffer());

    /* 3. GT911 触摸初始化 */
    ESP_LOGI(TAG, "[3/6] Touch init...");
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

    /* 4. SD 卡挂载 */
    ESP_LOGI(TAG, "[4/6] SD card mount...");
    sd_storage_config_t sd_cfg = {};
    sd_cfg.miso_gpio = BOARD_SD_MISO;
    sd_cfg.mosi_gpio = BOARD_SD_MOSI;
    sd_cfg.clk_gpio = BOARD_SD_CLK;
    sd_cfg.cs_gpio = BOARD_SD_CS;
    if (sd_storage_mount(&sd_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
    }

    /* 5. 字体子系统初始化（LittleFS 挂载 + 扫描阅读字体） */
    ESP_LOGI(TAG, "[5/6] Font init...");
    ui_font_init();

    // ═══════════════════════════════════════════════════════════════
    //  InkUI Canvas 验证 (阶段 1)
    // ═══════════════════════════════════════════════════════════════
    {
        uint8_t* fb = inkEpd.framebuffer();
        ESP_LOGI(TAG, "=== Canvas Test Begin ===");

        // --- 7.1 几何图形测试 ---
        // 全屏 Canvas
        ink::Canvas fullCanvas(fb, ink::Rect{0, 0, 540, 960});
        fullCanvas.clear(ink::Color::White);

        // 黑色填充矩形 (左上角)
        fullCanvas.fillRect({20, 20, 120, 60}, ink::Color::Black);

        // 灰色边框矩形 (右上角)
        fullCanvas.drawRect({400, 20, 120, 60}, ink::Color::Dark, 2);

        // 水平分隔线
        fullCanvas.drawHLine(20, 100, 500, ink::Color::Medium);

        // 垂直线
        fullCanvas.drawVLine(270, 110, 80, ink::Color::Dark);

        // 斜线
        fullCanvas.drawLine({20, 110}, {250, 190}, ink::Color::Black);

        ESP_LOGI(TAG, "Canvas geometry test drawn");

        // --- 7.2 裁剪测试 ---
        // 创建 200×100 的裁剪区域
        ink::Canvas clipCanvas(fb, ink::Rect{50, 220, 200, 100});
        // 用浅灰填充裁剪区域作为背景
        clipCanvas.clear(ink::Color::Light);
        // 画一个超出裁剪区域的大矩形 — 应该被裁剪
        clipCanvas.fillRect({-20, -20, 300, 200}, ink::Color::Medium);
        // 在裁剪区内画小黑色矩形，确认局部坐标
        clipCanvas.fillRect({10, 10, 40, 40}, ink::Color::Black);

        // 嵌套裁剪: 子 Canvas
        ink::Canvas subCanvas = clipCanvas.clipped({80, 20, 200, 200});
        // 子 Canvas 的 clip 应该是 {130, 240, 120, 80} (被父 clip 截断)
        subCanvas.fillRect({0, 0, 200, 200}, ink::Color::Dark);

        ESP_LOGI(TAG, "Canvas clip test drawn (check screen: clipped rects at y=220)");

        // --- 7.3 文字测试 ---
        const EpdFont* font24 = ui_font_get(24);
        const EpdFont* font16 = ui_font_get(16);

        if (font24) {
            ink::Canvas textCanvas(fb, ink::Rect{0, 350, 540, 200});
            textCanvas.clear(ink::Color::White);

            // 中文文字
            textCanvas.drawText(font24, "你好世界 Hello!", 20, 40, ink::Color::Black);

            // 英文 + 度量
            int w = textCanvas.measureText(font24, "Canvas OK");
            ESP_LOGI(TAG, "measureText('Canvas OK') = %d px", w);
            textCanvas.drawText(font24, "Canvas OK", 20, 80, ink::Color::Dark);

            // drawTextN: 只画前 6 字节 ("你好" = 6 bytes UTF-8)
            textCanvas.drawText(font24, "drawTextN:", 20, 120, ink::Color::Medium);
            textCanvas.drawTextN(font24, "你好世界", 6, 200, 120, ink::Color::Black);

            ESP_LOGI(TAG, "Canvas text test drawn");
        } else {
            ESP_LOGW(TAG, "No 24px font available, skipping text test");
        }

        if (font16) {
            ink::Canvas labelCanvas(fb, ink::Rect{0, 560, 540, 40});
            labelCanvas.clear(ink::Color::White);
            labelCanvas.drawText(font16, "--- InkUI Canvas 验证完成 ---", 100, 28, ink::Color::Dark);
        }

        // 刷新屏幕显示测试结果
        inkEpd.updateScreen();
        ESP_LOGI(TAG, "=== Canvas Test End (screen updated) ===");

        // 等待 5 秒查看结果，然后继续启动旧 ui_core
        vTaskDelay(pdMS_TO_TICKS(5000));
    }

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
