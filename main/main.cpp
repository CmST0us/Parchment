/**
 * @file main.cpp
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 初始化板级硬件，通过 InkUI Application 启动事件循环。
 * 当前使用 Boot 页面验证所有 widget 组件。
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

static const char* TAG = "parchment";

// ═══════════════════════════════════════════════════════════════
//  Boot 页面 ViewController — 验证所有 widget
// ═══════════════════════════════════════════════════════════════

class BootVC : public ink::ViewController {
public:
    BootVC() { title_ = "Boot"; }

    void viewDidLoad() override {
        ESP_LOGI("BootVC", "viewDidLoad");

        // 获取字体
        const EpdFont* fontSmall = ui_font_get(20);
        const EpdFont* fontLarge = ui_font_get(28);

        // 根 View
        view_ = std::make_unique<ink::View>();
        view_->setFrame({0, 0, ink::kScreenWidth, ink::kScreenHeight});
        view_->setBackgroundColor(ink::Color::White);

        // FlexBox Column 布局
        view_->flexStyle_.direction = ink::FlexDirection::Column;
        view_->flexStyle_.padding = ink::Insets{0, 0, 0, 0};
        view_->flexStyle_.gap = 0;
        view_->flexStyle_.alignItems = ink::Align::Stretch;

        // ── HeaderView: 页面标题栏 ──
        auto header = std::make_unique<ink::HeaderView>();
        header->setFont(fontLarge);
        header->setTitle("Parchment");
        header->flexBasis_ = 48;
        view_->addSubview(std::move(header));

        // ── 内容区域容器（带 padding） ──
        auto content = std::make_unique<ink::View>();
        content->flexGrow_ = 1;
        content->setBackgroundColor(ink::Color::White);
        content->flexStyle_.direction = ink::FlexDirection::Column;
        content->flexStyle_.padding = ink::Insets{16, 16, 16, 16};
        content->flexStyle_.gap = 12;
        content->flexStyle_.alignItems = ink::Align::Stretch;

        // TextLabel: 欢迎文字
        auto welcome = std::make_unique<ink::TextLabel>();
        welcome->setFont(fontLarge);
        welcome->setText("Welcome");
        welcome->setAlignment(ink::Align::Center);
        welcome->flexBasis_ = 40;
        content->addSubview(std::move(welcome));

        // TextLabel: 版本信息
        auto version = std::make_unique<ink::TextLabel>();
        version->setFont(fontSmall);
        version->setText("v0.5 InkUI Widgets");
        version->setColor(ink::Color::Medium);
        version->setAlignment(ink::Align::Center);
        version->flexBasis_ = 28;
        content->addSubview(std::move(version));

        // SeparatorView
        auto sep1 = std::make_unique<ink::SeparatorView>();
        sep1->flexBasis_ = 1;
        content->addSubview(std::move(sep1));

        // ProgressBarView: 加载进度
        auto progress = std::make_unique<ink::ProgressBarView>();
        progress->setValue(65);
        progress->setTrackHeight(8);
        progress->flexBasis_ = 24;
        progressBar_ = progress.get();
        content->addSubview(std::move(progress));

        // TextLabel: 进度说明
        auto progressLabel = std::make_unique<ink::TextLabel>();
        progressLabel->setFont(fontSmall);
        progressLabel->setText("Loading...");
        progressLabel->setColor(ink::Color::Dark);
        progressLabel->setAlignment(ink::Align::Center);
        progressLabel->flexBasis_ = 28;
        content->addSubview(std::move(progressLabel));

        // SeparatorView
        auto sep2 = std::make_unique<ink::SeparatorView>();
        sep2->flexBasis_ = 1;
        content->addSubview(std::move(sep2));

        // ButtonView: Primary 按钮
        auto btnPrimary = std::make_unique<ink::ButtonView>();
        btnPrimary->setFont(fontSmall);
        btnPrimary->setLabel("Start Reading");
        btnPrimary->setStyle(ink::ButtonStyle::Primary);
        btnPrimary->flexBasis_ = 48;
        btnPrimary->setOnTap([this]() {
            ESP_LOGI("BootVC", "Primary button tapped!");
            if (progressBar_) {
                int v = progressBar_->value() + 10;
                if (v > 100) v = 0;
                progressBar_->setValue(v);
            }
        });
        content->addSubview(std::move(btnPrimary));

        // ButtonView: Secondary 按钮
        auto btnSecondary = std::make_unique<ink::ButtonView>();
        btnSecondary->setFont(fontSmall);
        btnSecondary->setLabel("Settings");
        btnSecondary->setStyle(ink::ButtonStyle::Secondary);
        btnSecondary->flexBasis_ = 48;
        btnSecondary->setOnTap([]() {
            ESP_LOGI("BootVC", "Secondary button tapped!");
        });
        content->addSubview(std::move(btnSecondary));

        // 弹性填充区
        auto spacer = std::make_unique<ink::View>();
        spacer->flexGrow_ = 1;
        spacer->setBackgroundColor(ink::Color::White);
        content->addSubview(std::move(spacer));

        // SeparatorView
        auto sep3 = std::make_unique<ink::SeparatorView>();
        sep3->flexBasis_ = 1;
        content->addSubview(std::move(sep3));

        // PageIndicatorView
        auto pageInd = std::make_unique<ink::PageIndicatorView>();
        pageInd->setFont(fontSmall);
        pageInd->setPage(0, 5);
        pageInd->setOnPageChange([](int page) {
            ESP_LOGI("BootVC", "Page changed to %d", page);
        });
        pageInd->flexBasis_ = 48;
        content->addSubview(std::move(pageInd));

        view_->addSubview(std::move(content));
    }

    void viewDidAppear() override {
        ESP_LOGI("BootVC", "viewDidAppear");
    }

    void handleEvent(const ink::Event& event) override {
        if (event.type == ink::EventType::Swipe) {
            ESP_LOGI("BootVC", "Swipe: dir=%d",
                     static_cast<int>(event.swipe.direction));
        }
    }

private:
    ink::ProgressBarView* progressBar_ = nullptr;
};

// ═══════════════════════════════════════════════════════════════

static ink::Application app;

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.5 (InkUI)");
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

    // 推入 Boot 页面
    auto bootVC = std::make_unique<BootVC>();
    app.navigator().push(std::move(bootVC));

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  InkUI running");
    ESP_LOGI(TAG, "========================================");

    // 进入主事件循环（永不返回）
    app.run();
}
