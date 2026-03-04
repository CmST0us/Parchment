/**
 * @file SheetView.cpp
 * @brief iOS 16 风格底部 Sheet 视图实现。
 *
 * onDraw() 绘制顺序:
 * 1. 白色填充卡片区域（阴影下方）
 * 2. 顶部 Bayer 抖动阴影渐变（向上扩散）
 * 3. Handle 灰色矩形居中
 *
 * rebuild() 构建 View 树:
 * SheetView
 * ├─ TextLabel title (titleFont, Black)
 * ├─ SeparatorView
 * ├─ TappableView item0 (kItemHeight)
 * │  └─ TextLabel (font, Black)
 * ├─ SeparatorView
 * ├─ TappableView item1
 * │  └─ TextLabel (font, Black)
 * └─ ...
 */

#include "ink_ui/views/SheetView.h"

#include "ink_ui/core/FlexLayout.h"
#include "ink_ui/views/SeparatorView.h"
#include "ink_ui/views/TextLabel.h"

namespace {

/// 4×4 Bayer 有序抖动矩阵（阈值 0-15）
static constexpr int kBayer4x4[4][4] = {
    { 0,  8,  2, 10},
    {12,  4, 14,  6},
    { 3, 11,  1,  9},
    {15,  7, 13,  5},
};

/// 支持 tap 回调的简单 View（设置项专用）
class TappableView : public ink::View {
public:
    std::function<void()> onTap_;

    bool onTouchEvent(const ink::TouchEvent& event) override {
        if (event.type == ink::TouchType::Tap && onTap_) {
            onTap_();
            return true;
        }
        return false;
    }
};

} // anonymous namespace

namespace ink {

SheetView::SheetView() {
    setBackgroundColor(Color::Clear);  // 阴影区域需要透明
    setOpaque(true);
}

void SheetView::setTitle(const std::string& title) {
    title_ = title;
    needsRebuild_ = true;
}

void SheetView::setFont(const EpdFont* font) {
    font_ = font;
    needsRebuild_ = true;
}

void SheetView::setTitleFont(const EpdFont* titleFont) {
    titleFont_ = titleFont;
    needsRebuild_ = true;
}

void SheetView::addItem(const std::string& label, std::function<void()> onTap) {
    items_.push_back({label, std::move(onTap)});
    needsRebuild_ = true;
}

int SheetView::contentStartY() const {
    return kTopShadowSpread + kHandleTopMargin + kHandleHeight + kHandleBottomMargin;
}

int SheetView::totalHeight() const {
    int h = contentStartY();
    // 标题
    if (!title_.empty()) {
        h += 32;  // 标题行高
    }
    // 分隔线 + 设置项
    for (size_t i = 0; i < items_.size(); i++) {
        h += 1;  // 分隔线
        h += kItemHeight;
    }
    h += kContentPadBottom;
    return h;
}

void SheetView::onDraw(Canvas& canvas) {
    const Rect b = bounds();

    // 1. 白色填充卡片区域（阴影下方）
    Rect cardArea = {0, kTopShadowSpread, b.w, b.h - kTopShadowSpread};
    canvas.fillRect(cardArea, Color::White);

    // 2. 顶部阴影渐变（Bayer 抖动，向上扩散）
    for (int d = 1; d <= kTopShadowSpread; d++) {
        // d=1 最靠近卡片（最暗），d=kTopShadowSpread 最远（最淡）
        float t = static_cast<float>(d - 1) / static_cast<float>(kTopShadowSpread);
        float darkness = (1.0f - t) * (1.0f - t) * kMaxShadowLevels;

        if (darkness < 0.05f) break;

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

        float fracScaled = levelFrac * 16.0f;

        // 水平条带：从卡片顶部向上
        int y = kTopShadowSpread - d;
        for (int x = 0; x < b.w; x++) {
            uint8_t gray = (fracScaled > kBayer4x4[y & 3][x & 3])
                         ? grayDark : grayLight;
            if (gray != 0xF0) canvas.drawPixel(x, y, gray);
        }
    }

    // 3. 抓手 Handle（灰色矩形居中）
    int handleX = (b.w - kHandleWidth) / 2;
    int handleY = kTopShadowSpread + kHandleTopMargin;
    canvas.fillRect({handleX, handleY, kHandleWidth, kHandleHeight}, Color::Light);
}

void SheetView::onLayout() {
    if (needsRebuild_) {
        rebuild();
    }

    const Rect b = bounds();
    int startY = contentStartY();
    int innerW = b.w - kContentPadLeft - kContentPadRight;
    int cursor = startY;

    for (auto& child : subviews()) {
        if (child->isHidden()) continue;

        Size intrinsic = child->intrinsicSize();
        int childW = (intrinsic.w > 0) ? intrinsic.w : innerW;
        int childH = (intrinsic.h > 0) ? intrinsic.h
                   : (child->flexBasis_ > 0) ? child->flexBasis_ : 0;

        if (childW > innerW) childW = innerW;

        child->setFrame({kContentPadLeft, cursor, childW, childH});
        child->onLayout();

        cursor += childH;
    }
}

Size SheetView::intrinsicSize() const {
    if (needsRebuild_) {
        const_cast<SheetView*>(this)->rebuild();
    }
    return {kScreenWidth, totalHeight()};
}

void SheetView::rebuild() {
    needsRebuild_ = false;
    clearSubviews();

    // ── 标题 ──
    if (!title_.empty()) {
        auto title = std::make_unique<TextLabel>();
        title->setFont(titleFont_ ? titleFont_ : font_);
        title->setText(title_);
        title->setColor(Color::Black);
        title->flexBasis_ = 32;
        addSubview(std::move(title));
    }

    // ── 设置项列表 ──
    for (size_t i = 0; i < items_.size(); i++) {
        // 分隔线
        auto sep = std::make_unique<SeparatorView>();
        sep->flexBasis_ = 1;
        addSubview(std::move(sep));

        // 可点击设置项
        const auto& itemInfo = items_[i];
        auto item = std::make_unique<TappableView>();
        item->flexBasis_ = kItemHeight;
        item->setBackgroundColor(Color::White);
        item->flexStyle_.direction = FlexDirection::Row;
        item->flexStyle_.alignItems = Align::Center;

        auto label = std::make_unique<TextLabel>();
        label->setFont(font_);
        label->setText(itemInfo.label);
        label->setColor(Color::Black);
        label->flexGrow_ = 1;
        item->addSubview(std::move(label));

        item->onTap_ = itemInfo.onTap;
        addSubview(std::move(item));
    }
}

} // namespace ink
