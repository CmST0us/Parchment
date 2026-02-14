/**
 * @file main.cpp
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 初始化板级硬件（EPD、触摸、SD 卡）和 ui_core 框架，启动事件循环。
 * 阶段 1: Canvas 绘图引擎验证，保持旧 ui_core 运行。
 */

#include <cstdio>
#include <memory>

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

    // ═══════════════════════════════════════════════════════════════
    //  InkUI View 验证 (阶段 2)
    // ═══════════════════════════════════════════════════════════════
    {
        ESP_LOGI(TAG, "=== View Test Begin ===");

        // --- 7.1 树操作测试 ---
        auto root = std::make_unique<ink::View>();
        root->setFrame({0, 0, 540, 960});

        auto childA = std::make_unique<ink::View>();
        childA->setFrame({10, 10, 200, 100});
        auto childB = std::make_unique<ink::View>();
        childB->setFrame({10, 120, 200, 100});
        auto childC = std::make_unique<ink::View>();
        childC->setFrame({10, 230, 200, 100});

        ink::View* ptrA = childA.get();
        ink::View* ptrB = childB.get();
        ink::View* ptrC = childC.get();

        root->addSubview(std::move(childA));
        root->addSubview(std::move(childB));
        root->addSubview(std::move(childC));

        bool treeOk = (root->subviews().size() == 3)
                    && (ptrA->parent() == root.get())
                    && (ptrB->parent() == root.get())
                    && (ptrC->parent() == root.get());
        ESP_LOGI(TAG, "7.1 Tree ops: %s (3 children, parent correct)",
                 treeOk ? "PASS" : "FAIL");

        // removeFromParent
        auto detachedB = ptrB->removeFromParent();
        bool removeOk = (root->subviews().size() == 2)
                      && (detachedB.get() == ptrB)
                      && (ptrB->parent() == nullptr);
        ESP_LOGI(TAG, "7.1 removeFromParent: %s", removeOk ? "PASS" : "FAIL");

        // re-add then clearSubviews
        root->addSubview(std::move(detachedB));
        root->clearSubviews();
        bool clearOk = root->subviews().empty();
        ESP_LOGI(TAG, "7.1 clearSubviews: %s", clearOk ? "PASS" : "FAIL");

        // --- 7.2 脏标记冒泡测试 ---
        auto root2 = std::make_unique<ink::View>();
        root2->setFrame({0, 0, 540, 960});
        root2->clearDirtyFlags();

        auto panel = std::make_unique<ink::View>();
        panel->setFrame({0, 0, 540, 480});
        panel->clearDirtyFlags();

        auto button = std::make_unique<ink::View>();
        button->setFrame({20, 20, 100, 40});
        button->clearDirtyFlags();

        ink::View* pPanel = panel.get();
        ink::View* pButton = button.get();

        panel->addSubview(std::move(button));
        root2->addSubview(std::move(panel));

        // 清除 addSubview 触发的脏标记
        root2->clearDirtyFlags();
        pPanel->clearDirtyFlags();
        pButton->clearDirtyFlags();

        // 子 View setNeedsDisplay
        pButton->setNeedsDisplay();
        bool dirtyOk = pButton->needsDisplay()
                     && pPanel->subtreeNeedsDisplay()
                     && root2->subtreeNeedsDisplay();
        ESP_LOGI(TAG, "7.2 Dirty propagation: %s", dirtyOk ? "PASS" : "FAIL");

        // 短路测试: 再次 setNeedsDisplay 不应 crash（已标记的祖先短路）
        pButton->setNeedsDisplay();
        ESP_LOGI(TAG, "7.2 Short-circuit: PASS (no crash)");

        // --- 7.3 hitTest 测试 ---
        // root2 -> panel(0,0,540,480) -> button(20,20,100,40)
        // 重置脏标记
        root2->clearDirtyFlags();
        pPanel->clearDirtyFlags();
        pButton->clearDirtyFlags();

        // 点击 button 范围内: (30, 30) 在 root 坐标系
        // root.hitTest(30,30) -> 转到 panel 坐标 (30,30) -> 转到 button 坐标 (10,10) -> hit
        ink::View* hitResult = root2->hitTest(30, 30);
        bool hitBtnOk = (hitResult == pButton);
        ESP_LOGI(TAG, "7.3 hitTest button: %s", hitBtnOk ? "PASS" : "FAIL");

        // 点击 panel 但不在 button: (300, 300)
        ink::View* hitPanel = root2->hitTest(300, 300);
        bool hitPanelOk = (hitPanel == pPanel);
        ESP_LOGI(TAG, "7.3 hitTest panel: %s", hitPanelOk ? "PASS" : "FAIL");

        // 点击 root 但不在 panel: (300, 600)
        ink::View* hitRoot = root2->hitTest(300, 600);
        bool hitRootOk = (hitRoot == root2.get());
        ESP_LOGI(TAG, "7.3 hitTest root: %s", hitRootOk ? "PASS" : "FAIL");

        // 隐藏 button 后 hitTest 跳过
        pButton->setHidden(true);
        ink::View* hitHidden = root2->hitTest(30, 30);
        bool hiddenOk = (hitHidden == pPanel);
        ESP_LOGI(TAG, "7.3 hitTest hidden skip: %s", hiddenOk ? "PASS" : "FAIL");
        pButton->setHidden(false);

        // --- 7.4 screenFrame 测试 ---
        ink::Rect btnScreen = pButton->screenFrame();
        // panel frame=(0,0,540,480), button frame=(20,20,100,40)
        // screenFrame = (0+20, 0+20, 100, 40) = (20, 20, 100, 40)
        bool sfOk = (btnScreen.x == 20 && btnScreen.y == 20
                  && btnScreen.w == 100 && btnScreen.h == 40);
        ESP_LOGI(TAG, "7.4 screenFrame: %s (%d,%d,%d,%d)",
                 sfOk ? "PASS" : "FAIL",
                 btnScreen.x, btnScreen.y, btnScreen.w, btnScreen.h);

        // 给 panel 一个偏移量再测
        pPanel->setFrame({50, 100, 400, 300});
        ink::Rect btnScreen2 = pButton->screenFrame();
        // screenFrame = (50+20, 100+20, 100, 40) = (70, 120, 100, 40)
        bool sf2Ok = (btnScreen2.x == 70 && btnScreen2.y == 120
                   && btnScreen2.w == 100 && btnScreen2.h == 40);
        ESP_LOGI(TAG, "7.4 screenFrame with offset: %s (%d,%d,%d,%d)",
                 sf2Ok ? "PASS" : "FAIL",
                 btnScreen2.x, btnScreen2.y, btnScreen2.w, btnScreen2.h);

        ESP_LOGI(TAG, "=== View Test End ===");
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
