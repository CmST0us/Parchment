/**
 * @file ShadowCardView.cpp
 * @brief 带黑色边框的卡片容器 View 实现。
 *
 * 绘制顺序：
 * 1. 白色填充整个 frame（清除底层残留）
 * 2. 黑色边框矩形
 * 3. 白色卡片矩形填充边框内部
 */

#include "ink_ui/views/ShadowCardView.h"
#include "ink_ui/core/FlexLayout.h"

namespace ink {

ShadowCardView::ShadowCardView() {
    setBackgroundColor(Color::White);
    setOpaque(true);
}

Rect ShadowCardView::cardRect() const {
    return {kEdgeTotal, kEdgeTotal,
            bounds().w - 2 * kEdgeTotal,
            bounds().h - 2 * kEdgeTotal};
}

void ShadowCardView::onDraw(Canvas& canvas) {
    const Rect b = bounds();

    // 1. 白色填充整个 frame
    canvas.fillRect(b, Color::White);

    // 2. 黑色边框矩形
    Rect borderRect = {kBorderMargin, kBorderMargin,
                       b.w - 2 * kBorderMargin,
                       b.h - 2 * kBorderMargin};
    // 顶边
    canvas.fillRect({borderRect.x, borderRect.y, borderRect.w, kBorderWidth}, Color::Black);
    // 底边
    canvas.fillRect({borderRect.x, borderRect.y + borderRect.h - kBorderWidth, borderRect.w, kBorderWidth}, Color::Black);
    // 左边
    canvas.fillRect({borderRect.x, borderRect.y, kBorderWidth, borderRect.h}, Color::Black);
    // 右边
    canvas.fillRect({borderRect.x + borderRect.w - kBorderWidth, borderRect.y, kBorderWidth, borderRect.h}, Color::Black);

    // 3. 白色卡片矩形（边框内部）
    canvas.fillRect(cardRect(), Color::White);
}

void ShadowCardView::onLayout() {
    const Rect card = cardRect();
    if (card.w <= 0 || card.h <= 0) return;

    // 卡片内部区域 Column 布局（扣除 padding）
    int padLeft = flexStyle_.padding.left;
    int padTop = flexStyle_.padding.top;
    int innerW = card.w - flexStyle_.padding.horizontalTotal();
    int cursor = card.y + padTop;

    for (auto& child : subviews()) {
        if (child->isHidden()) continue;

        Size intrinsic = child->intrinsicSize();
        int childW = (intrinsic.w > 0) ? intrinsic.w : innerW;
        int childH = (intrinsic.h > 0) ? intrinsic.h
                   : (child->flexBasis_ > 0) ? child->flexBasis_ : 0;

        if (childW > innerW) childW = innerW;

        child->setFrame({card.x + padLeft, cursor, childW, childH});
        child->onLayout();

        cursor += childH + flexStyle_.gap;
    }
}

Size ShadowCardView::intrinsicSize() const {
    // 遍历子 View 计算内容尺寸
    int contentW = 0;
    int contentH = 0;

    for (auto& child : subviews()) {
        if (child->isHidden()) continue;
        Size childSize = child->intrinsicSize();
        if (childSize.w > contentW) contentW = childSize.w;
        int h = (childSize.h > 0) ? childSize.h
              : (child->flexBasis_ > 0) ? child->flexBasis_ : 0;
        contentH += h;
    }

    if (contentW <= 0 && contentH <= 0) {
        return {-1, -1};
    }

    // 加上间距
    int visibleCount = 0;
    for (auto& child : subviews()) {
        if (!child->isHidden()) visibleCount++;
    }
    if (visibleCount > 1) {
        contentH += (visibleCount - 1) * flexStyle_.gap;
    }
    contentH += flexStyle_.padding.verticalTotal();
    contentW += flexStyle_.padding.horizontalTotal();

    return {contentW + 2 * kEdgeTotal, contentH + 2 * kEdgeTotal};
}

} // namespace ink
