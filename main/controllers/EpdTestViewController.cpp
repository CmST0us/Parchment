/**
 * @file EpdTestViewController.cpp
 * @brief EPD 刷新测试页面实现。
 */

#include "controllers/EpdTestViewController.h"

#include <algorithm>

#include "esp_log.h"
#include "epd_driver.h"

extern "C" {
#include "epdiy.h"
}

#include "ui_font.h"
#include "ui_icon.h"

static const char* TAG = "EpdTestVC";

// ── TestPatternView ──

void EpdTestViewController::TestPatternView::onDraw(ink::Canvas& canvas) {
    auto b = bounds();

    if (!patternB) {
        // ── 图案 A: 灰度渐变 + 棋盘格 ──

        // 上半: 16 级灰度渐变条 (黑→白)
        constexpr int kSteps = 16;
        int stepW = b.w / kSteps;
        for (int i = 0; i < kSteps; i++) {
            uint8_t gray = (uint8_t)(i * 0x10);  // 0x00 ~ 0xF0
            int x = i * stepW;
            int w = (i == kSteps - 1) ? (b.w - x) : stepW;
            canvas.fillRect({x, 0, w, b.h / 2}, gray);
        }

        // 下半: 棋盘格
        int cellSize = 20;
        int yStart = b.h / 2;
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
        // ── 图案 B: 反向渐变 + 横条纹 ──

        // 上半: 反向灰度渐变 (白→黑)
        constexpr int kSteps = 16;
        int stepW = b.w / kSteps;
        for (int i = 0; i < kSteps; i++) {
            uint8_t gray = (uint8_t)((15 - i) * 0x10);  // 0xF0 ~ 0x00
            int x = i * stepW;
            int w = (i == kSteps - 1) ? (b.w - x) : stepW;
            canvas.fillRect({x, 0, w, b.h / 2}, gray);
        }

        // 下半: 水平条纹 (黑白交替, 每条 10px)
        int stripeH = 10;
        int yStart = b.h / 2;
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
    view_ = std::make_unique<ink::View>();
    view_->setBackgroundColor(ink::Color::White);
    view_->flexStyle_.direction = ink::FlexDirection::Column;
    view_->flexStyle_.gap = 8;
    view_->flexStyle_.padding = {8, 8, 8, 8};

    // ── Header ──
    auto header = std::make_unique<ink::HeaderView>();
    header->setFont(ui_font_get(24));
    header->setTitle("EPD Refresh Test");
    header->setLeftIcon(UI_ICON_ARROW_LEFT.data, [this]() {
        app_.navigator().pop();
    });
    view_->addSubview(std::move(header));

    // ── 信息标签 ──
    auto info = std::make_unique<ink::TextLabel>();
    info->setFont(ui_font_get(20));
    info->setText("Tap a button to refresh the pattern below");
    info->flexStyle_.padding = {4, 0, 4, 0};
    infoLabel_ = info.get();
    view_->addSubview(std::move(info));

    // ── 按钮行 ──
    auto buttonRow = std::make_unique<ink::View>();
    buttonRow->setBackgroundColor(ink::Color::Clear);
    buttonRow->flexStyle_.direction = ink::FlexDirection::Row;
    buttonRow->flexStyle_.gap = 8;
    buttonRow->flexBasis_ = 48;

    auto btnGL16 = std::make_unique<ink::ButtonView>();
    btnGL16->setLabel("GL16");
    btnGL16->setFont(ui_font_get(20));
    btnGL16->setStyle(ink::ButtonStyle::Secondary);
    btnGL16->setOnTap([this]() { refreshTestArea(MODE_GL16, "GL16"); });
    buttonRow->addSubview(std::move(btnGL16));

    auto btnDU = std::make_unique<ink::ButtonView>();
    btnDU->setLabel("DU");
    btnDU->setFont(ui_font_get(20));
    btnDU->setStyle(ink::ButtonStyle::Secondary);
    btnDU->setOnTap([this]() { refreshTestArea(MODE_DU, "DU"); });
    buttonRow->addSubview(std::move(btnDU));

    auto btnFastGL16 = std::make_unique<ink::ButtonView>();
    btnFastGL16->setLabel("Fast GL16");
    btnFastGL16->setFont(ui_font_get(20));
    btnFastGL16->setStyle(ink::ButtonStyle::Secondary);
    btnFastGL16->setOnTap([this]() { fastGL16Refresh(); });
    buttonRow->addSubview(std::move(btnFastGL16));

    auto btnClear = std::make_unique<ink::ButtonView>();
    btnClear->setLabel("Clear");
    btnClear->setFont(ui_font_get(20));
    btnClear->setStyle(ink::ButtonStyle::Primary);
    btnClear->setOnTap([this]() {
        ESP_LOGI(TAG, "Full clear");
        infoLabel_->setText("Full Clear...");
        epd_driver_clear();
        view()->setNeedsDisplay();
        view()->setNeedsLayout();
    });
    buttonRow->addSubview(std::move(btnClear));

    view_->addSubview(std::move(buttonRow));

    // ── 分隔线 ──
    auto sep = std::make_unique<ink::SeparatorView>();
    view_->addSubview(std::move(sep));

    // ── 测试图案区域 ──
    auto testPattern = std::make_unique<TestPatternView>();
    testPattern->setBackgroundColor(ink::Color::White);
    testPattern->flexGrow_ = 1;
    testView_ = testPattern.get();
    view_->addSubview(std::move(testPattern));
}

void EpdTestViewController::viewDidLoad() {
    ESP_LOGI(TAG, "EPD test page loaded");
}

void EpdTestViewController::refreshTestArea(int epdiyMode, const char* modeName) {
    if (!testView_) return;

    // 切换到另一个图案
    testView_->patternB = !testView_->patternB;
    const char* patName = testView_->patternB ? "B" : "A";

    // 将新图案绘制到 framebuffer（不触发屏幕刷新）
    ink::Rect sf = testView_->screenFrame();
    ink::Canvas canvas(epd_driver_get_framebuffer(), sf);
    canvas.clear(ink::Color::White);
    testView_->onDraw(canvas);

    ESP_LOGI(TAG, "Pattern %s -> refresh with %s, logical(%d,%d %dx%d)",
             patName, modeName, sf.x, sf.y, sf.w, sf.h);

    // 逻辑 → 物理坐标转换
    int px = sf.y;
    int py = ink::kScreenWidth - sf.x - sf.w;
    int pw = sf.h;
    int ph = sf.w;

    // 直接调用底层驱动，用指定模式刷新该区域
    epd_driver_update_area_mode(px, py, pw, ph, epdiyMode);

    // 更新信息标签
    char buf[64];
    snprintf(buf, sizeof(buf), "Pattern %s | %s", patName, modeName);
    infoLabel_->setText(buf);
}

void EpdTestViewController::fastGL16Refresh() {
    if (!testView_) return;

    // 切换图案
    testView_->patternB = !testView_->patternB;
    const char* patName = testView_->patternB ? "B" : "A";

    // 将新图案绘制到 framebuffer
    ink::Rect sf = testView_->screenFrame();
    ink::Canvas canvas(epd_driver_get_framebuffer(), sf);
    canvas.clear(ink::Color::White);
    testView_->onDraw(canvas);

    ESP_LOGI(TAG, "Pattern %s -> Fast GL16", patName);

    // 快速 GL16 全屏刷新（走 highlevel 差分路径 + 自定义快速波形）
    epd_driver_update_screen_fast_gl16();

    char buf[64];
    snprintf(buf, sizeof(buf), "Pattern %s | Fast GL16", patName);
    infoLabel_->setText(buf);

    view()->setNeedsDisplay();
    view()->setNeedsLayout();
}
