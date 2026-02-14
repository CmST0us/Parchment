/**
 * @file PageIndicatorView.cpp
 * @brief PageIndicatorView 实现 — 翻页导航指示器。
 */

#include "ink_ui/views/PageIndicatorView.h"
#include "ink_ui/core/FlexLayout.h"

#include <cstdio>

namespace ink {

PageIndicatorView::PageIndicatorView() {
    setRefreshHint(RefreshHint::Fast);

    // FlexBox Row 布局
    flexStyle_.direction = FlexDirection::Row;
    flexStyle_.alignItems = Align::Center;

    // 上一页按钮
    auto prev = std::make_unique<ButtonView>();
    prev->setStyle(ButtonStyle::Secondary);
    prev->setLabel("<");
    prev->flexBasis_ = kButtonSize;
    prev->setOnTap([this]() {
        if (currentPage_ > 0) {
            currentPage_--;
            updateDisplay();
            if (onPageChange_) onPageChange_(currentPage_);
        }
    });
    prevButton_ = prev.get();
    addSubview(std::move(prev));

    // 页码标签
    auto label = std::make_unique<TextLabel>();
    label->setAlignment(Align::Center);
    label->setColor(Color::Black);
    label->flexGrow_ = 1;
    pageLabel_ = label.get();
    addSubview(std::move(label));

    // 下一页按钮
    auto next = std::make_unique<ButtonView>();
    next->setStyle(ButtonStyle::Secondary);
    next->setLabel(">");
    next->flexBasis_ = kButtonSize;
    next->setOnTap([this]() {
        if (currentPage_ < totalPages_ - 1) {
            currentPage_++;
            updateDisplay();
            if (onPageChange_) onPageChange_(currentPage_);
        }
    });
    nextButton_ = next.get();
    addSubview(std::move(next));

    updateDisplay();
}

void PageIndicatorView::setPage(int page, int total) {
    if (total < 1) total = 1;
    if (page < 0) page = 0;
    if (page >= total) page = total - 1;
    currentPage_ = page;
    totalPages_ = total;
    updateDisplay();
}

void PageIndicatorView::setFont(const EpdFont* font) {
    if (pageLabel_) pageLabel_->setFont(font);
    if (prevButton_) prevButton_->setFont(font);
    if (nextButton_) nextButton_->setFont(font);
}

void PageIndicatorView::setOnPageChange(std::function<void(int)> callback) {
    onPageChange_ = std::move(callback);
}

void PageIndicatorView::updateDisplay() {
    // 更新页码文字（1-based 显示）
    char buf[32];
    snprintf(buf, sizeof(buf), "%d/%d", currentPage_ + 1, totalPages_);
    if (pageLabel_) pageLabel_->setText(buf);

    // 更新按钮禁用状态
    if (prevButton_) prevButton_->setEnabled(currentPage_ > 0);
    if (nextButton_) nextButton_->setEnabled(currentPage_ < totalPages_ - 1);
}

void PageIndicatorView::onLayout() {
    flexLayout(this);
}

Size PageIndicatorView::intrinsicSize() const {
    return {-1, kHeight};
}

} // namespace ink
