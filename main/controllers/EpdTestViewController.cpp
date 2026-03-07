/**
 * @file EpdTestViewController.cpp
 * @brief EPD 刷新调节测试页面实现。
 */

#include "controllers/EpdTestViewController.h"

#include <algorithm>
#include <string.h>

#include "esp_log.h"
#include "epd_driver.h"

extern "C" {
#include "epdiy.h"
}

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
    numPhases_ = epd_driver_get_fast_gl16_phases();
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
    info->setText("Adjust phases, then tap a refresh button");
    infoLabel_ = info.get();
    view_->addSubview(std::move(info));

    // ── Phase 数量调节行: "Phases: 15 (~120ms)  [-] [+]" ──
    auto phaseRow = std::make_unique<ink::View>();
    phaseRow->setBackgroundColor(ink::Color::Clear);
    phaseRow->flexStyle_.direction = ink::FlexDirection::Row;
    phaseRow->flexStyle_.gap = 8;
    phaseRow->flexBasis_ = 40;

    auto phaseLabel = std::make_unique<ink::TextLabel>();
    phaseLabel->setFont(fontSmall);
    phaseLabel->flexGrow_ = 1;
    phaseCountLabel_ = phaseLabel.get();
    phaseRow->addSubview(std::move(phaseLabel));

    auto btnMinus = std::make_unique<ink::ButtonView>();
    btnMinus->setLabel("-");
    btnMinus->setFont(fontLarge);
    btnMinus->setStyle(ink::ButtonStyle::Secondary);
    btnMinus->setOnTap([this]() { setPhaseCount(numPhases_ - 1); });
    phaseRow->addSubview(std::move(btnMinus));

    auto btnPlus = std::make_unique<ink::ButtonView>();
    btnPlus->setLabel("+");
    btnPlus->setFont(fontLarge);
    btnPlus->setStyle(ink::ButtonStyle::Secondary);
    btnPlus->setOnTap([this]() { setPhaseCount(numPhases_ + 1); });
    phaseRow->addSubview(std::move(btnPlus));

    view_->addSubview(std::move(phaseRow));

    // ── 快捷预设行 ──
    auto presetRow = std::make_unique<ink::View>();
    presetRow->setBackgroundColor(ink::Color::Clear);
    presetRow->flexStyle_.direction = ink::FlexDirection::Row;
    presetRow->flexStyle_.gap = 8;
    presetRow->flexBasis_ = 36;

    struct Preset { const char* label; int phases; };
    Preset presets[] = {
        {"3p ~24ms", 3},
        {"5p ~40ms", 5},
        {"8p ~64ms", 8},
        {"10p ~80ms", 10},
        {"15p ~120ms", 15},
    };
    for (auto& pr : presets) {
        auto btn = std::make_unique<ink::ButtonView>();
        btn->setLabel(pr.label);
        btn->setFont(fontSmall);
        btn->setStyle(ink::ButtonStyle::Secondary);
        int p = pr.phases;
        btn->setOnTap([this, p]() { setPhaseCount(p); });
        presetRow->addSubview(std::move(btn));
    }

    view_->addSubview(std::move(presetRow));

    // ── 分隔线 ──
    view_->addSubview(std::make_unique<ink::SeparatorView>());

    // ── 刷新按钮行 ──
    auto refreshRow = std::make_unique<ink::View>();
    refreshRow->setBackgroundColor(ink::Color::Clear);
    refreshRow->flexStyle_.direction = ink::FlexDirection::Row;
    refreshRow->flexStyle_.gap = 6;
    refreshRow->flexBasis_ = 40;

    auto btnFGL16 = std::make_unique<ink::ButtonView>();
    btnFGL16->setLabel("FastGL16");
    btnFGL16->setFont(fontSmall);
    btnFGL16->setStyle(ink::ButtonStyle::Secondary);
    btnFGL16->setOnTap([this]() { fastGL16Refresh(); });
    refreshRow->addSubview(std::move(btnFGL16));

    auto btnDUGL = std::make_unique<ink::ButtonView>();
    btnDUGL->setLabel("DU>GL");
    btnDUGL->setFont(fontSmall);
    btnDUGL->setStyle(ink::ButtonStyle::Secondary);
    btnDUGL->setOnTap([this]() { whiteDuThenGL16Refresh(); });
    refreshRow->addSubview(std::move(btnDUGL));

    auto btnGL16 = std::make_unique<ink::ButtonView>();
    btnGL16->setLabel("GL16");
    btnGL16->setFont(fontSmall);
    btnGL16->setStyle(ink::ButtonStyle::Secondary);
    btnGL16->setOnTap([this]() { refreshTestArea(MODE_GL16, "GL16"); });
    refreshRow->addSubview(std::move(btnGL16));

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
    updatePhaseCountDisplay();
}

// ── Phase 数量调节 ──

void EpdTestViewController::setPhaseCount(int count) {
    if (count < 1) count = 1;
    if (count > 15) count = 15;
    numPhases_ = count;
    epd_driver_set_fast_gl16_phases(count);
    updatePhaseCountDisplay();
}

void EpdTestViewController::updatePhaseCountDisplay() {
    if (!phaseCountLabel_) return;
    char buf[48];
    snprintf(buf, sizeof(buf), "Phases: %d (~%dms)", numPhases_, numPhases_ * 8);
    phaseCountLabel_->setText(buf);
}

// ── 刷新操作 ──

void EpdTestViewController::refreshTestArea(int epdiyMode, const char* modeName) {
    if (!testView_) return;

    testView_->patternB = !testView_->patternB;
    const char* patName = testView_->patternB ? "B" : "A";

    ink::Rect sf = testView_->screenFrame();
    ink::Canvas canvas(epd_driver_get_framebuffer(), sf);
    canvas.clear(ink::Color::White);
    testView_->onDraw(canvas);

    ESP_LOGI(TAG, "Pattern %s -> %s", patName, modeName);

    int px = sf.y;
    int py = ink::kScreenWidth - sf.x - sf.w;
    int pw = sf.h;
    int ph = sf.w;

    epd_driver_update_area_mode(px, py, pw, ph, epdiyMode);

    char buf[64];
    snprintf(buf, sizeof(buf), "Pattern %s | %s", patName, modeName);
    infoLabel_->setText(buf);
}

void EpdTestViewController::fastGL16Refresh() {
    if (!testView_) return;

    testView_->patternB = !testView_->patternB;
    const char* patName = testView_->patternB ? "B" : "A";

    ink::Rect sf = testView_->screenFrame();
    ink::Canvas canvas(epd_driver_get_framebuffer(), sf);
    canvas.clear(ink::Color::White);
    testView_->onDraw(canvas);

    ESP_LOGI(TAG, "Pattern %s -> FastGL16 (%d phases)", patName, numPhases_);

    epd_driver_update_screen_fast_gl16();

    char buf[64];
    snprintf(buf, sizeof(buf), "%s | FastGL16 %dp", patName, numPhases_);
    infoLabel_->setText(buf);

    view()->setNeedsDisplay();
    view()->setNeedsLayout();
}

void EpdTestViewController::whiteDuThenGL16Refresh() {
    if (!testView_) return;

    testView_->patternB = !testView_->patternB;
    const char* patName = testView_->patternB ? "B" : "A";

    ink::Rect sf = testView_->screenFrame();
    ink::Canvas canvas(epd_driver_get_framebuffer(), sf);
    canvas.clear(ink::Color::White);
    testView_->onDraw(canvas);

    ESP_LOGI(TAG, "Pattern %s -> White DU then GL16", patName);

    epd_driver_white_du_then_gl16();

    char buf[64];
    snprintf(buf, sizeof(buf), "%s | DU>GL16", patName);
    infoLabel_->setText(buf);

    view()->setNeedsDisplay();
    view()->setNeedsLayout();
}
