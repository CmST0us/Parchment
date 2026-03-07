/**
 * @file EpdTestViewController.h
 * @brief EPD 时序调节测试页面。
 */

#pragma once

#include "ink_ui/InkUI.h"

#define FAST_GL16_PHASES 15

/// EPD 时序调节与刷新模式测试页面
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

    // Phase 值显示标签（当前组 5 个）
    ink::TextLabel* phaseLabels_[5] = {};

    // 分组按钮引用（用于高亮当前组）
    ink::ButtonView* groupBtns_[3] = {};

    // ── 状态 ──
    int currentTimes_[FAST_GL16_PHASES] = {};
    int currentGroup_ = 0;  ///< 当前显示的 phase 分组（0/1/2）

    // ── 操作方法 ──
    void applyPreset(const int preset[FAST_GL16_PHASES]);
    void selectGroup(int group);
    void adjustPhase(int indexInGroup, int delta);
    void updatePhaseDisplay();

    void refreshTestArea(int epdiyMode, const char* modeName);
    void fastGL16Refresh();
    void whiteDuThenGL16Refresh();
};
