/**
 * @file StatusBarView.h
 * @brief 系统状态栏 View — 20px 高，显示时间。
 */

#pragma once

#include "ink_ui/core/View.h"

extern "C" {
#include "epdiy.h"
}

namespace ink {

/// 系统状态栏 (20px 高)
class StatusBarView : public View {
public:
    StatusBarView();

    /// 设置字体
    void setFont(const EpdFont* font);

    /// 更新时间显示（由外部定时调用）
    void updateTime();

    /// 更新电池电量（0-100），值变化时触发重绘
    void updateBattery(int percent);

    void onDraw(Canvas& canvas) override;
    Size intrinsicSize() const override;

private:
    const EpdFont* font_ = nullptr;
    char timeStr_[8] = "00:00";
    int batteryPercent_ = -1;  ///< 电池百分比 (0-100), -1 表示未初始化

    static constexpr int kHeight = 20;

    /// 电池图标尺寸
    static constexpr int kBatWidth = 18;   ///< 外框宽度（不含正极凸起）
    static constexpr int kBatHeight = 10;  ///< 外框高度
    static constexpr int kBatCapWidth = 2; ///< 正极凸起宽度
    static constexpr int kBatCapHeight = 4;///< 正极凸起高度
    static constexpr int kBatPadding = 2;  ///< 填充区域内边距
    static constexpr int kRightMargin = 12;///< 右边距

    /// 绘制电池图标
    void drawBatteryIcon(Canvas& canvas);
};

} // namespace ink
