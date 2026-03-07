/**
 * @file EpdTestViewController.cpp
 * @brief EPD 刷新模式测试页面实现。
 */

#include "controllers/EpdTestViewController.h"

#include <algorithm>
#include <string.h>

#include "esp_log.h"
#include "epd_driver.h"

#include "ui_font.h"
#include "ui_icon.h"

static const char* TAG = "EpdTestVC";

// ── 正文样例 ──
static const char* kSampleTextA =
    "天地玄黄，宇宙洪荒。日月盈昃，辰宿列张。";
static const char* kSampleTextB =
    "云腾致雨，露结为霜。金生丽水，玉出昆冈。";

// ── TestPatternView ──

void EpdTestViewController::TestPatternView::onDraw(ink::Canvas& canvas) {
    auto b = bounds();
    int gradientH = b.h / 3;
    int textH = b.h / 3;

    const EpdFont* font = ui_font_get(24);

    if (!patternB) {
        // 上 1/3: 灰度渐变 (黑→白)
        constexpr int kSteps = 16;
        int stepW = b.w / kSteps;
        for (int i = 0; i < kSteps; i++) {
            uint8_t gray = (uint8_t)(i * 0x10);
            int x = i * stepW;
            int w = (i == kSteps - 1) ? (b.w - x) : stepW;
            canvas.fillRect({x, 0, w, gradientH}, gray);
        }

        // 中 1/3: 正文样例
        canvas.fillRect({0, gradientH, b.w, textH}, ink::Color::White);
        if (font) {
            int ty = gradientH + 4 + font->ascender;
            canvas.drawText(font, kSampleTextA, 8, ty, ink::Color::Black);
        }

        // 下 1/3: 棋盘格
        int cellSize = 20;
        int yStart = gradientH + textH;
        for (int cy = yStart; cy < b.h; cy += cellSize) {
            for (int cx = 0; cx < b.w; cx += cellSize) {
                int row = (cy - yStart) / cellSize;
                int col = cx / cellSize;
                uint8_t color = ((row + col) % 2 == 0) ? ink::Color::Black
                                                         : ink::Color::White;
                int cw = std::min(cellSize, b.w - cx);
                int ch = std::min(cellSize, b.h - cy);
                canvas.fillRect({cx, cy, cw, ch}, color);
            }
        }
    } else {
        // 上 1/3: 反向灰度渐变 (白→黑)
        constexpr int kSteps = 16;
        int stepW = b.w / kSteps;
        for (int i = 0; i < kSteps; i++) {
            uint8_t gray = (uint8_t)((15 - i) * 0x10);
            int x = i * stepW;
            int w = (i == kSteps - 1) ? (b.w - x) : stepW;
            canvas.fillRect({x, 0, w, gradientH}, gray);
        }

        // 中 1/3: 不同正文样例
        canvas.fillRect({0, gradientH, b.w, textH}, ink::Color::White);
        if (font) {
            int ty = gradientH + 4 + font->ascender;
            canvas.drawText(font, kSampleTextB, 8, ty, ink::Color::Black);
        }

        // 下 1/3: 水平条纹
        int stripeH = 10;
        int yStart = gradientH + textH;
        for (int cy = yStart; cy < b.h; cy += stripeH) {
            int row = (cy - yStart) / stripeH;
            uint8_t color = (row % 2 == 0) ? ink::Color::Black
                                            : ink::Color::White;
            int ch = std::min(stripeH, b.h - cy);
            canvas.fillRect({0, cy, b.w, ch}, color);
        }
    }
}

// ── EpdTestViewController ──

EpdTestViewController::EpdTestViewController(ink::Application& app)
    : app_(app) {
    title_ = "EPD Test";
}

void EpdTestViewController::loadView() {
    const EpdFont* fontSmall = ui_font_get(16);
    const EpdFont* fontLarge = ui_font_get(24);

    view_ = std::make_unique<ink::View>();
    view_->setBackgroundColor(ink::Color::White);
    view_->flexStyle_.direction = ink::FlexDirection::Column;
    view_->flexStyle_.gap = 4;
    view_->flexStyle_.padding = {4, 4, 4, 4};

    // ── Header ──
    auto header = std::make_unique<ink::HeaderView>();
    header->setFont(fontLarge);
    header->setTitle("EPD Tuner");
    header->setLeftIcon(UI_ICON_ARROW_LEFT.data, [this]() {
        app_.navigator().pop();
    });
    view_->addSubview(std::move(header));

    // ── 信息标签 ──
    auto info = std::make_unique<ink::TextLabel>();
    info->setFont(fontSmall);
    info->setText("Tap a refresh button to test");
    infoLabel_ = info.get();
    view_->addSubview(std::move(info));

    // ── 刷新按钮行 ──
    auto refreshRow = std::make_unique<ink::View>();
    refreshRow->setBackgroundColor(ink::Color::Clear);
    refreshRow->flexStyle_.direction = ink::FlexDirection::Row;
    refreshRow->flexStyle_.gap = 6;
    refreshRow->flexBasis_ = 40;

    auto btnFast = std::make_unique<ink::ButtonView>();
    btnFast->setLabel("Fast");
    btnFast->setFont(fontSmall);
    btnFast->setStyle(ink::ButtonStyle::Secondary);
    btnFast->setOnTap([this]() { customRefresh(EPD_REFRESH_FAST, "Fast"); });
    refreshRow->addSubview(std::move(btnFast));

    auto btnStd = std::make_unique<ink::ButtonView>();
    btnStd->setLabel("Std");
    btnStd->setFont(fontSmall);
    btnStd->setStyle(ink::ButtonStyle::Secondary);
    btnStd->setOnTap([this]() { customRefresh(EPD_REFRESH_STANDARD, "Std"); });
    refreshRow->addSubview(std::move(btnStd));

    auto btnQuality = std::make_unique<ink::ButtonView>();
    btnQuality->setLabel("Quality");
    btnQuality->setFont(fontSmall);
    btnQuality->setStyle(ink::ButtonStyle::Secondary);
    btnQuality->setOnTap([this]() { customRefresh(EPD_REFRESH_QUALITY, "Quality"); });
    refreshRow->addSubview(std::move(btnQuality));

    auto btnWBGL = std::make_unique<ink::ButtonView>();
    btnWBGL->setLabel("W>B>GL");
    btnWBGL->setFont(fontSmall);
    btnWBGL->setStyle(ink::ButtonStyle::Secondary);
    btnWBGL->setOnTap([this]() { whiteBlackDuThenGL16Refresh(); });
    refreshRow->addSubview(std::move(btnWBGL));

    auto btnClear = std::make_unique<ink::ButtonView>();
    btnClear->setLabel("Clear");
    btnClear->setFont(fontSmall);
    btnClear->setStyle(ink::ButtonStyle::Primary);
    btnClear->setOnTap([this]() {
        infoLabel_->setText("Full Clear...");
        epd_driver_clear();
        view()->setNeedsDisplay();
        view()->setNeedsLayout();
    });
    refreshRow->addSubview(std::move(btnClear));

    view_->addSubview(std::move(refreshRow));

    // ── 分隔线 ──
    view_->addSubview(std::make_unique<ink::SeparatorView>());

    // ── 测试图案区域 ──
    auto testPattern = std::make_unique<TestPatternView>();
    testPattern->setBackgroundColor(ink::Color::White);
    testPattern->flexGrow_ = 1;
    testView_ = testPattern.get();
    view_->addSubview(std::move(testPattern));
}

void EpdTestViewController::viewDidLoad() {
    ESP_LOGI(TAG, "EPD tuner loaded");
}

// ── 刷新操作 ──

void EpdTestViewController::customRefresh(epd_refresh_mode_t mode, const char* modeName) {
    if (!testView_) return;

    testView_->patternB = !testView_->patternB;
    const char* patName = testView_->patternB ? "B" : "A";

    ink::Rect sf = testView_->screenFrame();
    ink::Canvas canvas(epd_driver_get_framebuffer(), sf);
    canvas.clear(ink::Color::White);
    testView_->onDraw(canvas);

    ESP_LOGI(TAG, "Pattern %s -> %s", patName, modeName);

    epd_driver_update_screen_custom(mode);

    char buf[64];
    snprintf(buf, sizeof(buf), "%s | %s", patName, modeName);
    infoLabel_->setText(buf);

    view()->setNeedsDisplay();
    view()->setNeedsLayout();
}

void EpdTestViewController::whiteBlackDuThenGL16Refresh() {
    if (!testView_) return;

    testView_->patternB = !testView_->patternB;
    const char* patName = testView_->patternB ? "B" : "A";

    ink::Rect sf = testView_->screenFrame();
    ink::Canvas canvas(epd_driver_get_framebuffer(), sf);
    canvas.clear(ink::Color::White);
    testView_->onDraw(canvas);

    ESP_LOGI(TAG, "Pattern %s -> W>B>GL16", patName);

    epd_driver_white_black_du_then_gl16();

    char buf[64];
    snprintf(buf, sizeof(buf), "%s | W>B>GL16", patName);
    infoLabel_->setText(buf);

    view()->setNeedsDisplay();
    view()->setNeedsLayout();
}
