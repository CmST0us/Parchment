/**
 * @file StatusBarView.h
 * @brief 系统状态栏 View — 20px 高，显示时间。
 */

#pragma once

#include "ink_ui/core/View.h"

extern "C" {
#include "epdiy.h"
}

/// 系统状态栏 (20px 高)
class StatusBarView : public ink::View {
public:
    StatusBarView();

    /// 设置字体
    void setFont(const EpdFont* font);

    /// 更新时间显示（由外部定时调用或在 viewWillAppear 中调用）
    void updateTime();

    void onDraw(ink::Canvas& canvas) override;
    ink::Size intrinsicSize() const override;

private:
    const EpdFont* font_ = nullptr;
    char timeStr_[8] = "00:00";

    static constexpr int kHeight = 20;
};
