/**
 * @file EpdTestViewController.cpp
 * @brief EPD 时序调节测试页面实现。
 */

#include "controllers/EpdTestViewController.h"

#include <algorithm>
#include <cstring>

#include "esp_log.h"
#include "epd_driver.h"

extern "C" {
#include "epdiy.h"
}

#include "ui_font.h"
#include "ui_icon.h"

static const char* TAG = "EpdTestVC";

// ── 预设时序 ──
static const int kPresetFast[FAST_GL16_PHASES] = {
    10, 10, 10, 10, 10, 15, 15, 20, 20, 30, 30, 50, 50, 80, 120
};
static const int kPresetNormal[FAST_GL16_PHASES] = {
    15, 15, 15, 15, 15, 20, 20, 30, 30, 40, 40, 60, 60, 100, 150
};
static const int kPresetSlow[FAST_GL16_PHASES] = {
    20, 20, 20, 20, 20, 30, 30, 40, 40, 60, 60, 80, 80, 120, 200
};

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
        // ── 图案 A ──
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
        // ── 图案 B ──
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
    // 初始化为 Fast 预设
    memcpy(currentTimes_, kPresetFast, sizeof(currentTimes_));
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
    header->setTitle("EPD Timing Tuner");
    header->setLeftIcon(UI_ICON_ARROW_LEFT.data, [this]() {
        app_.navigator().pop();
    });
    view_->addSubview(std::move(header));

    // ── 信息标签 ──
    auto info = std::make_unique<ink::TextLabel>();
    info->setFont(fontSmall);
    info->setText("Select preset, adjust phases, then refresh");
    infoLabel_ = info.get();
    view_->addSubview(std::move(info));

    // ── 预设选择行 ──
    auto presetRow = std::make_unique<ink::View>();
    presetRow->setBackgroundColor(ink::Color::Clear);
    presetRow->flexStyle_.direction = ink::FlexDirection::Row;
    presetRow->flexStyle_.gap = 8;
    presetRow->flexBasis_ = 40;

    auto btnFast = std::make_unique<ink::ButtonView>();
    btnFast->setLabel("Fast");
    btnFast->setFont(fontSmall);
    btnFast->setStyle(ink::ButtonStyle::Primary);
    btnFast->setOnTap([this]() { applyPreset(kPresetFast); });
    presetRow->addSubview(std::move(btnFast));

    auto btnNormal = std::make_unique<ink::ButtonView>();
    btnNormal->setLabel("Normal");
    btnNormal->setFont(fontSmall);
    btnNormal->setStyle(ink::ButtonStyle::Secondary);
    btnNormal->setOnTap([this]() { applyPreset(kPresetNormal); });
    presetRow->addSubview(std::move(btnNormal));

    auto btnSlow = std::make_unique<ink::ButtonView>();
    btnSlow->setLabel("Slow");
    btnSlow->setFont(fontSmall);
    btnSlow->setStyle(ink::ButtonStyle::Secondary);
    btnSlow->setOnTap([this]() { applyPreset(kPresetSlow); });
    presetRow->addSubview(std::move(btnSlow));

    view_->addSubview(std::move(presetRow));

    // ── Phase 分组选择行 ──
    auto groupRow = std::make_unique<ink::View>();
    groupRow->setBackgroundColor(ink::Color::Clear);
    groupRow->flexStyle_.direction = ink::FlexDirection::Row;
    groupRow->flexStyle_.gap = 8;
    groupRow->flexBasis_ = 36;

    const char* groupLabels[] = {"P0-4", "P5-9", "P10-14"};
    for (int g = 0; g < 3; g++) {
        auto btn = std::make_unique<ink::ButtonView>();
        btn->setLabel(groupLabels[g]);
        btn->setFont(fontSmall);
        btn->setStyle(g == 0 ? ink::ButtonStyle::Primary : ink::ButtonStyle::Secondary);
        btn->setOnTap([this, g]() { selectGroup(g); });
        groupBtns_[g] = btn.get();
        groupRow->addSubview(std::move(btn));
    }

    view_->addSubview(std::move(groupRow));

    // ── Phase 调节行（5 行，每行: "Pn: value  [+] [-]"）──
    for (int i = 0; i < 5; i++) {
        auto row = std::make_unique<ink::View>();
        row->setBackgroundColor(ink::Color::Clear);
        row->flexStyle_.direction = ink::FlexDirection::Row;
        row->flexStyle_.gap = 4;
        row->flexBasis_ = 32;

        // Phase 值标签
        auto label = std::make_unique<ink::TextLabel>();
        label->setFont(fontSmall);
        label->flexGrow_ = 1;
        phaseLabels_[i] = label.get();
        row->addSubview(std::move(label));

        // [+] 按钮
        auto btnPlus = std::make_unique<ink::ButtonView>();
        btnPlus->setLabel("+");
        btnPlus->setFont(fontSmall);
        btnPlus->setStyle(ink::ButtonStyle::Secondary);
        btnPlus->setOnTap([this, i]() { adjustPhase(i, 5); });
        row->addSubview(std::move(btnPlus));

        // [-] 按钮
        auto btnMinus = std::make_unique<ink::ButtonView>();
        btnMinus->setLabel("-");
        btnMinus->setFont(fontSmall);
        btnMinus->setStyle(ink::ButtonStyle::Secondary);
        btnMinus->setOnTap([this, i]() { adjustPhase(i, -5); });
        row->addSubview(std::move(btnMinus));

        view_->addSubview(std::move(row));
    }

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
    ESP_LOGI(TAG, "EPD timing tuner loaded");
    updatePhaseDisplay();
}

// ── 预设与调节 ──

void EpdTestViewController::applyPreset(const int preset[FAST_GL16_PHASES]) {
    memcpy(currentTimes_, preset, sizeof(currentTimes_));
    updatePhaseDisplay();

    char buf[64];
    int total = 0;
    for (int i = 0; i < FAST_GL16_PHASES; i++) total += currentTimes_[i];
    snprintf(buf, sizeof(buf), "Preset applied (total: %dus)", total);
    infoLabel_->setText(buf);
}

void EpdTestViewController::selectGroup(int group) {
    currentGroup_ = group;

    // 更新分组按钮样式
    for (int g = 0; g < 3; g++) {
        if (groupBtns_[g]) {
            groupBtns_[g]->setStyle(g == group ? ink::ButtonStyle::Primary
                                               : ink::ButtonStyle::Secondary);
        }
    }
    updatePhaseDisplay();
}

void EpdTestViewController::adjustPhase(int indexInGroup, int delta) {
    int phaseIdx = currentGroup_ * 5 + indexInGroup;
    if (phaseIdx >= FAST_GL16_PHASES) return;

    currentTimes_[phaseIdx] += delta;
    if (currentTimes_[phaseIdx] < 5) currentTimes_[phaseIdx] = 5;
    if (currentTimes_[phaseIdx] > 500) currentTimes_[phaseIdx] = 500;

    updatePhaseDisplay();
}

void EpdTestViewController::updatePhaseDisplay() {
    for (int i = 0; i < 5; i++) {
        int phaseIdx = currentGroup_ * 5 + i;
        if (phaseLabels_[i] == nullptr) continue;

        if (phaseIdx < FAST_GL16_PHASES) {
            char buf[32];
            snprintf(buf, sizeof(buf), "P%d: %d", phaseIdx, currentTimes_[phaseIdx]);
            phaseLabels_[i]->setText(buf);
        } else {
            phaseLabels_[i]->setText("--");
        }
    }
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

    // 逻辑 → 物理坐标转换
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

    // 下发当前时序到 driver
    epd_driver_set_fast_gl16_times(currentTimes_);

    testView_->patternB = !testView_->patternB;
    const char* patName = testView_->patternB ? "B" : "A";

    ink::Rect sf = testView_->screenFrame();
    ink::Canvas canvas(epd_driver_get_framebuffer(), sf);
    canvas.clear(ink::Color::White);
    testView_->onDraw(canvas);

    ESP_LOGI(TAG, "Pattern %s -> Fast GL16 (custom times)", patName);

    epd_driver_update_screen_fast_gl16();

    int total = 0;
    for (int i = 0; i < FAST_GL16_PHASES; i++) total += currentTimes_[i];

    char buf[64];
    snprintf(buf, sizeof(buf), "%s | FastGL16 %dus", patName, total);
    infoLabel_->setText(buf);

    view()->setNeedsDisplay();
    view()->setNeedsLayout();
}

void EpdTestViewController::whiteDuThenGL16Refresh() {
    if (!testView_) return;

    testView_->patternB = !testView_->patternB;
    const char* patName = testView_->patternB ? "B" : "A";

    // 先绘制目标图案到 framebuffer
    ink::Rect sf = testView_->screenFrame();
    ink::Canvas canvas(epd_driver_get_framebuffer(), sf);
    canvas.clear(ink::Color::White);
    testView_->onDraw(canvas);

    ESP_LOGI(TAG, "Pattern %s -> White DU then GL16", patName);

    // 执行 白DU → GL16 流程
    epd_driver_white_du_then_gl16();

    char buf[64];
    snprintf(buf, sizeof(buf), "%s | DU>GL16", patName);
    infoLabel_->setText(buf);

    view()->setNeedsDisplay();
    view()->setNeedsLayout();
}
