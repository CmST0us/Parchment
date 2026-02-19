/**
 * @file BookCoverView.cpp
 * @brief 书籍封面缩略图绘制实现。
 *
 * 绘制 52×72px 的书籍封面:
 * - 浅灰背景
 * - 深色边框 (1px)
 * - 居中格式标签 "TXT"
 * - 右上角折角装饰
 */

#include "views/BookCoverView.h"
#include "ink_ui/core/Canvas.h"

BookCoverView::BookCoverView() {
    setBackgroundColor(ink::Color::White);
}

void BookCoverView::setFormatTag(const std::string& tag) {
    if (formatTag_ == tag) return;
    formatTag_ = tag;
    setNeedsDisplay();
}

void BookCoverView::setFont(font_engine_t* engine, uint8_t fontSize) {
    engine_ = engine;
    fontSize_ = fontSize;
}

void BookCoverView::onDraw(ink::Canvas& canvas) {
    int w = frame().w;
    int h = frame().h;

    // 浅灰背景
    canvas.fillRect({0, 0, w, h}, 0xE8);

    // 深色边框 (1px)
    canvas.drawRect({0, 0, w, h}, ink::Color::Dark, 1);

    // 右上角折角装饰: 用白色三角覆盖
    // 从 (w-kFoldSize, 0) 到 (w, kFoldSize) 的三角形
    for (int i = 0; i < kFoldSize; i++) {
        // 从右上角画白色水平线，逐行缩短
        int lineLen = kFoldSize - i;
        canvas.drawHLine(w - lineLen, i, lineLen, ink::Color::White);
    }
    // 折角斜线
    canvas.drawLine({w - kFoldSize, 0}, {w - 1, kFoldSize - 1},
                    ink::Color::Dark);

    // 居中格式标签
    if (engine_ && fontSize_ > 0 && !formatTag_.empty()) {
        int textW = canvas.measureText(engine_, formatTag_.c_str(), fontSize_);
        int textX = (w - textW) / 2;
        // 基线大约在垂直中心偏下
        int textY = h / 2 + 4;
        canvas.drawText(engine_, formatTag_.c_str(), textX, textY,
                        fontSize_, ink::Color::Dark);
    }
}

ink::Size BookCoverView::intrinsicSize() const {
    return {kCoverWidth, kCoverHeight};
}
