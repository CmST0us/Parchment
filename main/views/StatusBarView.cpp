/**
 * @file StatusBarView.cpp
 * @brief 系统状态栏实现。
 *
 * 20px 高，左侧显示时间 (HH:MM)，底部 1px 分隔线。
 */

#include "views/StatusBarView.h"
#include "ink_ui/core/Canvas.h"

#include <ctime>
#include <cstdio>

StatusBarView::StatusBarView() {
    setBackgroundColor(ink::Color::White);
}

void StatusBarView::setFont(const EpdFont* font) {
    font_ = font;
}

void StatusBarView::updateTime() {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    snprintf(timeStr_, sizeof(timeStr_), "%02d:%02d",
             timeinfo.tm_hour, timeinfo.tm_min);
    setNeedsDisplay();
}

void StatusBarView::onDraw(ink::Canvas& canvas) {
    int w = frame().w;
    int h = frame().h;

    // 时间文字 (左侧, 垂直居中)
    if (font_) {
        int baselineY = (h - font_->advance_y) / 2 + font_->ascender;
        canvas.drawText(font_, timeStr_, 12, baselineY, ink::Color::Dark);
    }

    // 底部 1px 分隔线
    canvas.drawHLine(0, h - 1, w, ink::Color::Light);
}

ink::Size StatusBarView::intrinsicSize() const {
    return {-1, kHeight};
}
