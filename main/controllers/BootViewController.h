/**
 * @file BootViewController.h
 * @brief 启动画面控制器 — 显示应用名称和加载状态，自动跳转到书库。
 */

#pragma once

#include "ink_ui/InkUI.h"

/// 启动画面控制器
class BootViewController : public ink::ViewController {
public:
    explicit BootViewController(ink::Application& app);

    void viewDidLoad() override;
    void viewDidAppear() override;
    void handleEvent(const ink::Event& event) override;

private:
    ink::Application& app_;
    ink::TextLabel* statusLabel_ = nullptr;
    ink::ProgressBarView* progressBar_ = nullptr;

    static constexpr int kTimerId = 1;
    static constexpr int kDelayMs = 2000;
};
