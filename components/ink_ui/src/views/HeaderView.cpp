/**
 * @file HeaderView.cpp
 * @brief HeaderView 实现 — 页面顶部标题栏。
 */

#include "ink_ui/views/HeaderView.h"
#include "ink_ui/core/FlexLayout.h"

namespace ink {

HeaderView::HeaderView() {
    setRefreshHint(RefreshHint::Quality);

    // FlexBox Row 布局
    flexStyle_.direction = FlexDirection::Row;
    flexStyle_.alignItems = Align::Center;

    rebuild();
}

void HeaderView::setTitle(const std::string& title) {
    if (titleLabel_) {
        titleLabel_->setText(title);
    }
}

void HeaderView::setLeftIcon(const uint8_t* iconData, std::function<void()> onTap) {
    leftIconData_ = iconData;
    leftTap_ = std::move(onTap);
    rebuild();
}

void HeaderView::setRightIcon(const uint8_t* iconData, std::function<void()> onTap) {
    rightIconData_ = iconData;
    rightTap_ = std::move(onTap);
    rebuild();
}

void HeaderView::setFont(font_engine_t* engine, uint8_t fontSize) {
    engine_ = engine;
    fontSize_ = fontSize;
    if (titleLabel_) {
        titleLabel_->setFont(engine, fontSize);
    }
}

void HeaderView::rebuild() {
    // 保存当前标题
    std::string currentTitle;
    if (titleLabel_) {
        currentTitle = titleLabel_->text();
    }

    // 清除所有子 View（释放内存）
    clearSubviews();
    leftButton_ = nullptr;
    rightButton_ = nullptr;
    titleLabel_ = nullptr;

    // 按顺序重建：left button → title → right button

    // 左侧按钮
    if (leftIconData_) {
        auto btn = std::make_unique<ButtonView>();
        btn->setStyle(ButtonStyle::Icon);
        btn->setIcon(leftIconData_);
        btn->setOnTap(leftTap_);
        btn->flexBasis_ = kButtonSize;
        leftButton_ = btn.get();
        addSubview(std::move(btn));
    }

    // 标题 Label（始终存在，flexGrow=1 填充剩余空间）
    auto label = std::make_unique<TextLabel>();
    label->setColor(Color::Black);
    label->setAlignment(Align::Center);
    label->flexGrow_ = 1;
    if (engine_) label->setFont(engine_, fontSize_);
    if (!currentTitle.empty()) label->setText(currentTitle);
    titleLabel_ = label.get();
    addSubview(std::move(label));

    // 右侧按钮
    if (rightIconData_) {
        auto btn = std::make_unique<ButtonView>();
        btn->setStyle(ButtonStyle::Icon);
        btn->setIcon(rightIconData_);
        btn->setOnTap(rightTap_);
        btn->flexBasis_ = kButtonSize;
        rightButton_ = btn.get();
        addSubview(std::move(btn));
    }

    setNeedsLayout();
    setNeedsDisplay();
}

void HeaderView::onLayout() {
    flexLayout(this);
}

Size HeaderView::intrinsicSize() const {
    return {-1, kHeight};
}

} // namespace ink
