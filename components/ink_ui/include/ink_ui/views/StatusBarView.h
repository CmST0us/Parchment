/**
 * @file StatusBarView.h
 * @brief 系统状态栏 View — 20px 高，显示时间。
 */

#pragma once

#include "ink_ui/core/View.h"

extern "C" {
struct font_engine_t;
}

namespace ink {

/// 系统状态栏 (20px 高)
class StatusBarView : public View {
public:
    StatusBarView();

    /// 设置字体引擎和字号
    void setFont(font_engine_t* engine, uint8_t fontSize);

    /// 更新时间显示（由外部定时调用）
    void updateTime();

    void onDraw(Canvas& canvas) override;
    Size intrinsicSize() const override;

private:
    font_engine_t* engine_ = nullptr;
    uint8_t fontSize_ = 0;
    char timeStr_[8] = "00:00";

    static constexpr int kHeight = 20;
};

} // namespace ink
