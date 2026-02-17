/**
 * @file BootViewController.cpp
 * @brief 启动画面控制器实现。
 *
 * 像素级还原 design/index.html 的 Boot 画面:
 * Logo(100px) → 标题 "Parchment" → 副标题 "墨水屏阅读器" →
 * 状态文字 → 进度条(200×3px)
 */

#include "controllers/BootViewController.h"
#include "controllers/LibraryViewController.h"
#include "views/BootLogoView.h"

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
    const EpdFont* fontMedium = ui_font_get(20);
    const EpdFont* fontSmall = ui_font_get(16);

    // 根 View: FlexBox Column, 居中对齐（由 contentArea_ 约束尺寸）
    view_ = std::make_unique<ink::View>();
    view_->setBackgroundColor(ink::Color::White);
    view_->flexStyle_.direction = ink::FlexDirection::Column;
    view_->flexStyle_.alignItems = ink::Align::Center;

    // ── 上部弹性空间 ──
    auto topSpacer = std::make_unique<ink::View>();
    topSpacer->flexGrow_ = 1;
    topSpacer->setBackgroundColor(ink::Color::White);
    view_->addSubview(std::move(topSpacer));

    // ── Logo 书本图标 (100×100px) ──
    auto logo = std::make_unique<BootLogoView>();
    logo->flexBasis_ = 100;
    view_->addSubview(std::move(logo));

    // ── Logo 与标题间距 24px ──
    auto logoGap = std::make_unique<ink::View>();
    logoGap->flexBasis_ = 24;
    logoGap->setBackgroundColor(ink::Color::White);
    view_->addSubview(std::move(logoGap));

    // ── 标题 "Parchment" (28px, Black, 居中) ──
    auto title = std::make_unique<ink::TextLabel>();
    title->setFont(fontLarge);
    title->setText("Parchment");
    title->setColor(ink::Color::Black);
    title->setAlignment(ink::Align::Center);
    title->flexBasis_ = 40;
    view_->addSubview(std::move(title));

    // ── 副标题 "墨水屏阅读器" (20px, Dark, 居中) ──
    auto subtitle = std::make_unique<ink::TextLabel>();
    subtitle->setFont(fontMedium);
    subtitle->setText("墨水屏阅读器");
    subtitle->setColor(ink::Color::Dark);
    subtitle->setAlignment(ink::Align::Center);
    subtitle->flexBasis_ = 28;
    view_->addSubview(std::move(subtitle));

    // ── 副标题与底部状态间弹性空间 ──
    auto midSpacer = std::make_unique<ink::View>();
    midSpacer->flexGrow_ = 1;
    midSpacer->setBackgroundColor(ink::Color::White);
    view_->addSubview(std::move(midSpacer));

    // ── 状态文字 (16px, Medium, 居中) ──
    auto status = std::make_unique<ink::TextLabel>();
    status->setFont(fontSmall);
    status->setText("正在初始化...");
    status->setColor(ink::Color::Medium);
    status->setAlignment(ink::Align::Center);
    status->flexBasis_ = 24;
    statusLabel_ = status.get();
    view_->addSubview(std::move(status));

    // ── 状态文字与进度条间距 16px ──
    auto statusGap = std::make_unique<ink::View>();
    statusGap->flexBasis_ = 16;
    statusGap->setBackgroundColor(ink::Color::White);
    view_->addSubview(std::move(statusGap));

    // ── 进度条容器 (200px 宽度约束, 居中) ──
    auto progressContainer = std::make_unique<ink::View>();
    progressContainer->flexBasis_ = 8;
    progressContainer->setBackgroundColor(ink::Color::White);
    progressContainer->flexStyle_.direction = ink::FlexDirection::Row;
    progressContainer->flexStyle_.justifyContent = ink::Justify::Center;

    auto progress = std::make_unique<ink::ProgressBarView>();
    progress->setTrackHeight(3);
    progress->setTrackColor(ink::Color::Light);
    progress->setFillColor(ink::Color::Dark);
    progress->setValue(0);
    progress->flexBasis_ = 200;  // 200px 宽度
    progressBar_ = progress.get();
    progressContainer->addSubview(std::move(progress));
    view_->addSubview(std::move(progressContainer));

    // ── 底部弹性空间 ──
    auto bottomSpacer = std::make_unique<ink::View>();
    bottomSpacer->flexGrow_ = 1;
    bottomSpacer->setBackgroundColor(ink::Color::White);
    view_->addSubview(std::move(bottomSpacer));

    // ── 扫描 SD 卡书籍 ──
    esp_err_t err = book_store_scan();
    if (err == ESP_OK) {
        size_t count = book_store_get_count();
        char buf[64];
        snprintf(buf, sizeof(buf), "发现 %zu 本书", count);
        statusLabel_->setText(buf);
        if (progressBar_) {
            progressBar_->setValue(100);
        }
    } else {
        statusLabel_->setText("SD 卡不可用");
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
