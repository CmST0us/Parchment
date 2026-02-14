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

static const char* TAG = "parchment";

// ═══════════════════════════════════════════════════════════════
//  验证 ViewController — 显示灰色矩形 + 响应触摸
// ═══════════════════════════════════════════════════════════════

/// 触摸响应 View：点击时切换颜色
class TouchBox : public ink::View {
public:
    explicit TouchBox(uint8_t color) : color_(color) {
        setBackgroundColor(ink::Color::White);
    }

    void onDraw(ink::Canvas& canvas) override {
        canvas.fillRect({4, 4, frame().w - 8, frame().h - 8}, color_);
    }

    bool onTouchEvent(const ink::TouchEvent& event) override {
        if (event.type == ink::TouchType::Tap) {
            // 切换颜色
            color_ = (color_ == ink::Color::Black) ?
                     ink::Color::Medium : ink::Color::Black;
            setNeedsDisplay();
            setRefreshHint(ink::RefreshHint::Fast);
            ESP_LOGI("TouchBox", "Tapped! color=%02x", color_);
            return true;
        }
        return false;
    }

private:
    uint8_t color_;
};

/// 第二个验证页面（用于测试 push/pop）
class SecondVC : public ink::ViewController {
public:
    SecondVC() { title_ = "Second"; }

    void viewDidLoad() override {
        ESP_LOGI("SecondVC", "viewDidLoad");
        view_ = std::make_unique<ink::View>();
        view_->setFrame({0, 0, ink::kScreenWidth, ink::kScreenHeight});
        view_->setBackgroundColor(ink::Color::Light);

        auto label = std::make_unique<ink::View>();
        label->setFrame({0, 0, ink::kScreenWidth, 60});
        label->setBackgroundColor(ink::Color::Dark);
        view_->addSubview(std::move(label));
    }

    void handleEvent(const ink::Event& event) override {
        if (event.type == ink::EventType::Swipe &&
            event.swipe.direction == ink::SwipeDirection::Right) {
            ESP_LOGI("SecondVC", "Swipe right → pop");
            // 注意: 这里需要 Application 引用来 pop
            // 实际使用时通过全局 app 或回调
        }
    }

    void viewDidAppear() override {
        ESP_LOGI("SecondVC", "viewDidAppear");
    }

    void viewDidDisappear() override {
        ESP_LOGI("SecondVC", "viewDidDisappear");
    }
};

/// 主验证 ViewController — FlexBox 布局 + 触摸响应
class DemoVC : public ink::ViewController {
public:
    DemoVC() { title_ = "Demo"; }

    void viewDidLoad() override {
        ESP_LOGI("DemoVC", "viewDidLoad");

        // 根 View
        view_ = std::make_unique<ink::View>();
        view_->setFrame({0, 0, ink::kScreenWidth, ink::kScreenHeight});
        view_->setBackgroundColor(ink::Color::White);

        // FlexBox Column 布局
        view_->flexStyle_.direction = ink::FlexDirection::Column;
        view_->flexStyle_.padding = ink::Insets{20, 16, 20, 16};
        view_->flexStyle_.gap = 12;
        view_->flexStyle_.alignItems = ink::Align::Stretch;

        // Header: 固定高度 60px
        auto header = std::make_unique<ink::View>();
        header->flexBasis_ = 60;
        header->setBackgroundColor(ink::Color::Dark);
        view_->addSubview(std::move(header));

        // 3 个 TouchBox
        for (int i = 0; i < 3; i++) {
            uint8_t colors[] = {ink::Color::Black, ink::Color::Medium, ink::Color::Dark};
            auto box = std::make_unique<TouchBox>(colors[i]);
            box->flexBasis_ = 100;
            view_->addSubview(std::move(box));
        }

        // 弹性填充区
        auto spacer = std::make_unique<ink::View>();
        spacer->flexGrow_ = 1;
        spacer->setBackgroundColor(ink::Color::Light);
        view_->addSubview(std::move(spacer));

        // Footer: 固定高度 40px
        auto footer = std::make_unique<ink::View>();
        footer->flexBasis_ = 40;
        footer->setBackgroundColor(ink::Color::Medium);
        view_->addSubview(std::move(footer));
    }

    void viewDidAppear() override {
        ESP_LOGI("DemoVC", "viewDidAppear");
    }

    void handleEvent(const ink::Event& event) override {
        if (event.type == ink::EventType::Swipe) {
            ESP_LOGI("DemoVC", "Swipe: dir=%d",
                     static_cast<int>(event.swipe.direction));
        }
    }
};

// ═══════════════════════════════════════════════════════════════

static ink::Application app;

extern "C" void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.4 (InkUI)");
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

    // 推入主页面
    auto demoVC = std::make_unique<DemoVC>();
    app.navigator().push(std::move(demoVC));

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  InkUI running");
    ESP_LOGI(TAG, "========================================");

    // 进入主事件循环（永不返回）
    app.run();
}
