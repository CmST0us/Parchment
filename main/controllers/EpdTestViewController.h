/**
 * @file EpdTestViewController.h
 * @brief EPD 刷新测试页面 — 用于测试不同墨水屏刷新模式。
 */

#pragma once

#include "ink_ui/InkUI.h"

/// EPD 刷新模式测试页面
class EpdTestViewController : public ink::ViewController {
public:
    explicit EpdTestViewController(ink::Application& app);

    void loadView() override;
    void viewDidLoad() override;
    bool prefersStatusBarHidden() const override { return false; }

private:
    ink::Application& app_;

    /// 双图案测试 View（A/B 切换，观察不同刷新模式的过渡效果）
    class TestPatternView : public ink::View {
    public:
        bool patternB = false;  ///< false=图案A, true=图案B
        void onDraw(ink::Canvas& canvas) override;
    };

    TestPatternView* testView_ = nullptr;
    ink::TextLabel* infoLabel_ = nullptr;

    /// 切换图案并用指定模式刷新测试区域
    void refreshTestArea(int epdiyMode, const char* modeName);

    /// 快速 GL16: 15 相位自定义波形，灰度版 DU
    void fastGL16Refresh();
};
