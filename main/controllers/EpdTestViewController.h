/**
 * @file EpdTestViewController.h
 * @brief EPD 刷新模式测试页面。
 */

#pragma once

#include "ink_ui/InkUI.h"
#include "epd_driver.h"

/// EPD 刷新模式测试页面
class EpdTestViewController : public ink::ViewController {
public:
    explicit EpdTestViewController(ink::Application& app);

    void loadView() override;
    void viewDidLoad() override;
    bool prefersStatusBarHidden() const override { return false; }

private:
    ink::Application& app_;

    /// 双图案测试 View（A/B 切换）
    class TestPatternView : public ink::View {
    public:
        bool patternB = false;
        void onDraw(ink::Canvas& canvas) override;
    };

    // ── View 引用（非持有） ──
    TestPatternView* testView_ = nullptr;
    ink::TextLabel* infoLabel_ = nullptr;

    // ── 刷新操作 ──
    void customRefresh(epd_refresh_mode_t mode, const char* modeName);
    void whiteBlackDuThenGL16Refresh();
};
