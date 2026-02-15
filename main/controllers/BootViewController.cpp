/**
 * @file BootViewController.cpp
 * @brief 启动画面控制器实现。
 */

#include "controllers/BootViewController.h"
#include "controllers/LibraryViewController.h"

extern "C" {
#include "esp_log.h"
#include "book_store.h"
#include "ui_font.h"
}

static const char* TAG = "BootVC";

BootViewController::BootViewController(ink::Application& app)
    : app_(app) {
    title_ = "Boot";
}

void BootViewController::viewDidLoad() {
    ESP_LOGI(TAG, "viewDidLoad");

    const EpdFont* fontLarge = ui_font_get(28);
    const EpdFont* fontSmall = ui_font_get(20);

    // 根 View: 全屏, FlexBox Column, 垂直居中
    view_ = std::make_unique<ink::View>();
    view_->setFrame({0, 0, ink::kScreenWidth, ink::kScreenHeight});
    view_->setBackgroundColor(ink::Color::White);
    view_->flexStyle_.direction = ink::FlexDirection::Column;
    view_->flexStyle_.alignItems = ink::Align::Stretch;

    // 上部弹性空间
    auto topSpacer = std::make_unique<ink::View>();
    topSpacer->flexGrow_ = 1;
    topSpacer->setBackgroundColor(ink::Color::White);
    view_->addSubview(std::move(topSpacer));

    // 标题 "Parchment"
    auto title = std::make_unique<ink::TextLabel>();
    title->setFont(fontLarge);
    title->setText("Parchment");
    title->setColor(ink::Color::Black);
    title->setAlignment(ink::Align::Center);
    title->flexBasis_ = 40;
    view_->addSubview(std::move(title));

    // 副标题 "E-Ink Reader"
    auto subtitle = std::make_unique<ink::TextLabel>();
    subtitle->setFont(fontSmall);
    subtitle->setText("E-Ink Reader");
    subtitle->setColor(ink::Color::Dark);
    subtitle->setAlignment(ink::Align::Center);
    subtitle->flexBasis_ = 32;
    view_->addSubview(std::move(subtitle));

    // 下部弹性空间
    auto bottomSpacer = std::make_unique<ink::View>();
    bottomSpacer->flexGrow_ = 1;
    bottomSpacer->setBackgroundColor(ink::Color::White);
    view_->addSubview(std::move(bottomSpacer));

    // 底部状态文字
    auto status = std::make_unique<ink::TextLabel>();
    status->setFont(fontSmall);
    status->setText("Initializing...");
    status->setColor(ink::Color::Medium);
    status->setAlignment(ink::Align::Center);
    status->flexBasis_ = 48;
    statusLabel_ = status.get();
    view_->addSubview(std::move(status));

    // 扫描 SD 卡书籍
    esp_err_t err = book_store_scan();
    if (err == ESP_OK) {
        size_t count = book_store_get_count();
        char buf[64];
        snprintf(buf, sizeof(buf), "Found %zu books", count);
        statusLabel_->setText(buf);
    } else {
        statusLabel_->setText("SD card not available");
    }
}

void BootViewController::viewDidAppear() {
    ESP_LOGI(TAG, "viewDidAppear — posting delayed timer (%d ms)", kDelayMs);
    app_.postDelayed(ink::Event::makeTimer(kTimerId), kDelayMs);
}

void BootViewController::handleEvent(const ink::Event& event) {
    if (event.type == ink::EventType::Timer && event.timer.timerId == kTimerId) {
        ESP_LOGI(TAG, "Timer fired — navigating to Library");
        auto libraryVC = std::make_unique<LibraryViewController>(app_);
        app_.navigator().replace(std::move(libraryVC));
    }
}
