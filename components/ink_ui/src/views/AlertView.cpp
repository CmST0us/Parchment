/**
 * @file AlertView.cpp
 * @brief AlertView 实现 — iOS 风格 Alert 弹窗。
 *
 * rebuild() 根据当前配置重建内部 View 树:
 * AlertView (ShadowCardView, padding={24,24,0,24}, gap=0, Column)
 * ├─ TextLabel title (titleFont, Black, Center)
 * ├─ View spacer (8px)
 * ├─ TextLabel message (font, Dark, Center)
 * ├─ View spacer (20px)
 * ├─ SeparatorView (horizontal, 1px)
 * └─ View btnRow (44px, Row, Stretch)
 *    ├─ TappableView btn0 (flexGrow=1, Center)
 *    │  └─ TextLabel (font, color by isPrimary)
 *    ├─ SeparatorView (vertical) [多按钮时]
 *    └─ TappableView btn1 (flexGrow=1, Center)
 *       └─ TextLabel (font, color by isPrimary)
 */

#include "ink_ui/views/AlertView.h"

#include "ink_ui/core/FlexLayout.h"
#include "ink_ui/views/SeparatorView.h"
#include "ink_ui/views/TextLabel.h"

namespace {

/// 支持 tap 回调的简单 View（Alert 按钮专用）
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

AlertView::AlertView() {
    flexStyle_.direction = FlexDirection::Column;
    flexStyle_.alignItems = Align::Stretch;
    flexStyle_.padding = Insets{12, 24, 12, 24};
    flexStyle_.gap = 0;
}

void AlertView::setTitle(const std::string& title) {
    title_ = title;
    needsRebuild_ = true;
}

void AlertView::setMessage(const std::string& message) {
    message_ = message;
    needsRebuild_ = true;
}

void AlertView::setFont(const EpdFont* font) {
    font_ = font;
    needsRebuild_ = true;
}

void AlertView::setTitleFont(const EpdFont* titleFont) {
    titleFont_ = titleFont;
    needsRebuild_ = true;
}

void AlertView::addButton(const std::string& label, std::function<void()> onTap,
                           bool isPrimary) {
    buttons_.push_back({label, std::move(onTap), isPrimary});
    needsRebuild_ = true;
}

Size AlertView::intrinsicSize() const {
    if (needsRebuild_) {
        const_cast<AlertView*>(this)->rebuild();
    }
    return ShadowCardView::intrinsicSize();
}

void AlertView::onLayout() {
    if (needsRebuild_) {
        rebuild();
    }
    ShadowCardView::onLayout();
}

void AlertView::rebuild() {
    needsRebuild_ = false;
    clearSubviews();

    // ── 标题 ──
    if (!title_.empty()) {
        auto title = std::make_unique<TextLabel>();
        title->setFont(titleFont_ ? titleFont_ : font_);
        title->setText(title_);
        title->setColor(Color::Black);
        title->setAlignment(Align::Center);
        addSubview(std::move(title));

        // 标题与消息间距
        auto spacer = std::make_unique<View>();
        spacer->setBackgroundColor(Color::White);
        spacer->flexBasis_ = 8;
        addSubview(std::move(spacer));
    }

    // ── 消息 ──
    if (!message_.empty()) {
        auto message = std::make_unique<TextLabel>();
        message->setFont(font_);
        message->setText(message_);
        message->setColor(Color::Dark);
        message->setAlignment(Align::Center);
        addSubview(std::move(message));
    }

    // ── 消息与分隔线间距 ──
    auto spacer2 = std::make_unique<View>();
    spacer2->setBackgroundColor(Color::White);
    spacer2->flexBasis_ = 20;
    addSubview(std::move(spacer2));

    // ── 水平分隔线 ──
    auto sep = std::make_unique<SeparatorView>();
    sep->flexBasis_ = 1;
    addSubview(std::move(sep));

    // ── 按钮行 ──
    if (!buttons_.empty()) {
        auto btnRow = std::make_unique<View>();
        btnRow->flexBasis_ = 44;
        btnRow->setBackgroundColor(Color::White);
        btnRow->flexStyle_.direction = FlexDirection::Row;
        btnRow->flexStyle_.alignItems = Align::Stretch;

        for (size_t i = 0; i < buttons_.size(); i++) {
            // 按钮间垂直分隔线
            if (i > 0) {
                auto vSep =
                    std::make_unique<SeparatorView>(Orientation::Vertical);
                vSep->flexBasis_ = 1;
                btnRow->addSubview(std::move(vSep));
            }

            const auto& btnInfo = buttons_[i];

            auto btn = std::make_unique<TappableView>();
            btn->flexGrow_ = 1;
            btn->setBackgroundColor(Color::White);
            btn->flexStyle_.direction = FlexDirection::Row;
            btn->flexStyle_.alignItems = Align::Center;

            auto label = std::make_unique<TextLabel>();
            label->setFont(font_);
            label->setText(btnInfo.label);
            label->setColor(btnInfo.isPrimary ? Color::Black : Color::Dark);
            label->setAlignment(Align::Center);
            label->flexGrow_ = 1;
            btn->addSubview(std::move(label));

            btn->onTap_ = btnInfo.onTap;
            btnRow->addSubview(std::move(btn));
        }

        addSubview(std::move(btnRow));
    }
}

} // namespace ink
