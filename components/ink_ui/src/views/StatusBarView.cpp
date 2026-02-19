/**
 * @file StatusBarView.cpp
 * @brief 系统状态栏实现。
 *
 * 20px 高，左侧显示时间 (HH:MM)，底部 1px 分隔线。
 */

#include "ink_ui/views/StatusBarView.h"
#include "ink_ui/core/Canvas.h"

extern "C" {
#include "font_engine/font_engine.h"
}

#include <ctime>
#include <cstdio>

namespace ink {

StatusBarView::StatusBarView() {
}

void StatusBarView::setFont(font_engine_t* engine, uint8_t fontSize) {
    engine_ = engine;
    fontSize_ = fontSize;
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

void StatusBarView::onDraw(Canvas& canvas) {
    int h = frame().h;

    // 时间文字 (左侧, 垂直居中)
    if (engine_ && fontSize_ > 0) {
        int ascender = engine_->header.ascender * fontSize_ / engine_->header.font_height;
        int baselineY = (h - fontSize_) / 2 + ascender;
        canvas.drawText(engine_, timeStr_, 12, baselineY, fontSize_, Color::Dark);
    }
}

Size StatusBarView::intrinsicSize() const {
    return {-1, kHeight};
}

} // namespace ink
