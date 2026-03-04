/**
 * @file ShadowCardView.cpp
 * @brief 带投影阴影的卡片容器 View 实现。
 *
 * 阴影绘制算法（模拟 Figma drop shadow: blur=24, spread=4, opacity=30%）：
 * 1. 白色填充整个 frame（清除底层残留）
 * 2. 逐像素绘制阴影，使用 4×4 Bayer 有序抖动模拟平滑灰度渐变
 * 3. 白色卡片矩形覆盖阴影内部
 *
 * 4bpp 灰度仅 16 级，通过有序抖动在相邻灰度级间插值，
 * 将有效灰度级从 3 级提升到 ~50 级，实现柔和的 Gaussian 模糊效果。
 */

#include "ink_ui/views/ShadowCardView.h"
#include "ink_ui/core/FlexLayout.h"

namespace ink {

/// 4×4 Bayer 有序抖动矩阵（阈值 0-15）
static constexpr int kBayer4x4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5},
};

ShadowCardView::ShadowCardView() {
    setBackgroundColor(Color::White);
    setOpaque(true);
}

Rect ShadowCardView::cardRect() const {
    return {kShadowSpread, kShadowSpread,
            bounds().w - 2 * kShadowSpread,
            bounds().h - 2 * kShadowSpread};
}

void ShadowCardView::onDraw(Canvas& canvas) {
    const Rect b = bounds();

    // 1. 白色填充整个 frame（清除底层残留像素）
    canvas.fillRect(b, Color::White);

    // 2. 逐像素绘制阴影渐变（Bayer 有序抖动）
    const Rect card = cardRect();
    const int cx = card.x + kShadowOffsetX;
    const int cy = card.y + kShadowOffsetY;

    for (int d = 1; d <= kShadowSpread; d++) {
        // 二次衰减曲线: 靠近卡片暗，远离卡片渐淡
        float t = static_cast<float>(d - 1) / static_cast<float>(kShadowSpread);
        float darkness = (1.0f - t) * (1.0f - t) * kMaxShadowLevels;

        if (darkness < 0.05f) break;

        // 分解为整数级和小数部分，用于两级间抖动
        int levelInt = static_cast<int>(darkness);
        float levelFrac = darkness - levelInt;

        uint8_t grayLight, grayDark;
        if (levelInt >= kMaxShadowLevels) {
            grayLight = grayDark = 0xF0 - kMaxShadowLevels * 0x10;
            levelFrac = 0;
        } else {
            grayLight = 0xF0 - levelInt * 0x10;
            grayDark  = 0xF0 - (levelInt + 1) * 0x10;
        }

        // 抖动阈值（0-16 范围，frac * 16 > threshold 时用深色）
        float fracScaled = levelFrac * 16.0f;

        // ── 顶部条带 ──
        int y = cy - d;
        for (int x = cx - d; x < cx + card.w + d; x++) {
            uint8_t gray = (fracScaled > kBayer4x4[y & 3][x & 3])
                         ? grayDark : grayLight;
            if (gray != 0xF0) canvas.drawPixel(x, y, gray);
        }

        // ── 底部条带 ──
        y = cy + card.h + d - 1;
        for (int x = cx - d; x < cx + card.w + d; x++) {
            uint8_t gray = (fracScaled > kBayer4x4[y & 3][x & 3])
                         ? grayDark : grayLight;
            if (gray != 0xF0) canvas.drawPixel(x, y, gray);
        }

        // ── 左侧条带（排除角落）──
        int x = cx - d;
        for (int yy = cy - d + 1; yy < cy + card.h + d - 1; yy++) {
            uint8_t gray = (fracScaled > kBayer4x4[yy & 3][x & 3])
                         ? grayDark : grayLight;
            if (gray != 0xF0) canvas.drawPixel(x, yy, gray);
        }

        // ── 右侧条带（排除角落）──
        x = cx + card.w + d - 1;
        for (int yy = cy - d + 1; yy < cy + card.h + d - 1; yy++) {
            uint8_t gray = (fracScaled > kBayer4x4[yy & 3][x & 3])
                         ? grayDark : grayLight;
            if (gray != 0xF0) canvas.drawPixel(x, yy, gray);
        }
    }

    // 3. 白色卡片矩形覆盖阴影内部
    canvas.fillRect(card, Color::White);
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

    return {contentW + 2 * kShadowSpread, contentH + 2 * kShadowSpread};
}

} // namespace ink
