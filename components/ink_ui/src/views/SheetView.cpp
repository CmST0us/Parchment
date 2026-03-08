/**
 * @file SheetView.cpp
 * @brief iOS 16 风格底部 Sheet 视图实现。
 *
 * onDraw() 绘制顺序:
 * 1. 白色填充卡片区域
 * 2. 顶部黑色边框线
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
    return kTopBorderWidth + kHandleTopMargin + kHandleHeight + kHandleBottomMargin;
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

    // 1. 白色填充卡片区域
    canvas.fillRect({0, 0, b.w, b.h}, Color::White);

    // 2. 顶部黑色边框线
    canvas.fillRect({0, 0, b.w, kTopBorderWidth}, Color::Black);

    // 3. 抓手 Handle（灰色矩形居中）
    int handleX = (b.w - kHandleWidth) / 2;
    int handleY = kTopBorderWidth + kHandleTopMargin;
    canvas.fillRect({handleX, handleY, kHandleWidth, kHandleHeight}, Color::Black);
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
