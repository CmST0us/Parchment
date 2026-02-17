/**
 * @file BootLogoView.cpp
 * @brief 启动画面书本 Logo 绘制实现。
 *
 * 使用 Canvas 直线段绘制简化的打开书本图标:
 * - 书本外轮廓（倒 U 形 + 底边）
 * - 中间书脊竖线
 * - 左右各 4 条书页横线
 */

#include "views/BootLogoView.h"
#include "ink_ui/core/Canvas.h"

BootLogoView::BootLogoView() {
    setBackgroundColor(ink::Color::White);
}

void BootLogoView::onDraw(ink::Canvas& canvas) {
    // 绘制区域: 100×100, 书本占据中心区域
    // 参考 design/index.html SVG: 书本从 (20,15) 到 (80,80)
    // 按比例映射到 100×100

    const uint8_t black = ink::Color::Black;
    const uint8_t gray = ink::Color::Medium;

    // ── 书本外轮廓 ──
    // 左边竖线: 从顶部圆弧底部到底边
    canvas.drawVLine(20, 25, 55, black);   // 左边线 (20, 25) -> (20, 80)
    // 右边竖线
    canvas.drawVLine(80, 15, 65, black);   // 右边线 (80, 15) -> (80, 80)
    // 底边
    canvas.drawHLine(20, 80, 61, black);   // 底边 (20, 80) -> (80, 80)
    // 顶部: 左侧圆弧用折线近似
    canvas.drawHLine(30, 15, 50, black);   // 顶边 (30, 15) -> (80, 15)
    canvas.drawLine({20, 25}, {30, 15}, black); // 左上圆角近似

    // 加粗: 再画一条偏移线
    canvas.drawVLine(21, 25, 55, black);
    canvas.drawVLine(79, 15, 65, black);
    canvas.drawHLine(20, 79, 61, black);
    canvas.drawHLine(30, 16, 50, black);
    canvas.drawLine({21, 25}, {30, 16}, black);

    // ── 中间书脊线 ──
    canvas.drawVLine(50, 20, 55, black);

    // ── 左侧书页线条 ──
    canvas.drawHLine(28, 32, 16, gray);
    canvas.drawHLine(28, 40, 13, gray);
    canvas.drawHLine(28, 48, 15, gray);
    canvas.drawHLine(28, 56, 11, gray);

    // ── 右侧书页线条 ──
    canvas.drawHLine(56, 32, 16, gray);
    canvas.drawHLine(56, 40, 13, gray);
    canvas.drawHLine(56, 48, 15, gray);
    canvas.drawHLine(56, 56, 11, gray);
}

ink::Size BootLogoView::intrinsicSize() const {
    return {kLogoSize, kLogoSize};
}
