/**
 * @file EpdTestViewController.h
 * @brief EPD 刷新调节测试页面。
 */

#pragma once

#include "ink_ui/InkUI.h"

/// EPD 刷新调节与测试页面
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
    ink::TextLabel* phaseCountLabel_ = nullptr;

    // ── 状态 ──
    int numPhases_ = 15;

    // ── 操作方法 ──
    void setPhaseCount(int count);
    void updatePhaseCountDisplay();

    void refreshTestArea(int epdiyMode, const char* modeName);
    void fastGL16Refresh();
    void whiteDuThenGL16Refresh();
};
