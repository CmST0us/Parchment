/**
 * @file StatusBarView.cpp
 * @brief 系统状态栏实现。
 *
 * 20px 高，左侧显示时间 (HH:MM)，底部 1px 分隔线。
 */

#include "ink_ui/views/StatusBarView.h"
#include "ink_ui/core/Canvas.h"

#include <ctime>
#include <cstdio>

namespace ink {

StatusBarView::StatusBarView() {
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

void StatusBarView::updateBattery(int percent) {
    // Clamp 到 0-100
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;

    if (percent != batteryPercent_) {
        batteryPercent_ = percent;
        setNeedsDisplay();
    }
}

void StatusBarView::onDraw(Canvas& canvas) {
    int w = frame().w;
    int h = frame().h;

    // 时间文字 (左侧, 垂直居中)
    if (font_) {
        int baselineY = (h - font_->advance_y) / 2 + font_->ascender;
        canvas.drawText(font_, timeStr_, 12, baselineY, Color::Dark);
    }

    // 电池图标 (右侧)
    if (batteryPercent_ >= 0) {
        drawBatteryIcon(canvas);
    }
}

void StatusBarView::drawBatteryIcon(Canvas& canvas) {
    int w = frame().w;
    int h = frame().h;

    // 图标位置: 右对齐, 垂直居中
    int totalWidth = kBatWidth + kBatCapWidth;
    int x0 = w - kRightMargin - totalWidth;
    int y0 = (h - kBatHeight) / 2;

    // 1. 电池外框
    canvas.drawRect({x0, y0, kBatWidth, kBatHeight}, Color::Dark);

    // 2. 正极凸起（右侧小矩形）
    int capX = x0 + kBatWidth;
    int capY = y0 + (kBatHeight - kBatCapHeight) / 2;
    canvas.fillRect({capX, capY, kBatCapWidth, kBatCapHeight}, Color::Dark);

    // 3. 内部填充（按百分比缩放宽度）
    int fillMaxW = kBatWidth - kBatPadding * 2;
    int fillW = fillMaxW * batteryPercent_ / 100;
    if (fillW > 0) {
        int fillX = x0 + kBatPadding;
        int fillY = y0 + kBatPadding;
        int fillH = kBatHeight - kBatPadding * 2;
        canvas.fillRect({fillX, fillY, fillW, fillH}, Color::Dark);
    }
}

Size StatusBarView::intrinsicSize() const {
    return {-1, kHeight};
}

} // namespace ink
