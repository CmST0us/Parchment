/**
 * @file PageIndicatorView.h
 * @brief PageIndicatorView — 翻页导航指示器。
 */

#pragma once

#include <functional>

#include "ink_ui/core/Canvas.h"
#include "ink_ui/core/View.h"
#include "ink_ui/views/ButtonView.h"
#include "ink_ui/views/TextLabel.h"

extern "C" {
struct font_engine_t;
}

namespace ink {

/// PageIndicatorView: 翻页指示器（"< 2/5 >"）
class PageIndicatorView : public View {
public:
    PageIndicatorView();
    ~PageIndicatorView() override = default;

    /// 设置当前页码和总页数
    void setPage(int page, int total);

    /// 设置字体引擎和字号
    void setFont(font_engine_t* engine, uint8_t fontSize);

    /// 设置页码变化回调
    void setOnPageChange(std::function<void(int)> callback);

    int currentPage() const { return currentPage_; }
    int totalPages() const { return totalPages_; }

    // ── View 覆写 ──

    void onLayout() override;
    Size intrinsicSize() const override;

private:
    static constexpr int kHeight = 48;
    static constexpr int kButtonSize = 48;

    int currentPage_ = 0;
    int totalPages_ = 1;
    std::function<void(int)> onPageChange_;

    ButtonView* prevButton_ = nullptr;   ///< 非拥有指针
    TextLabel* pageLabel_ = nullptr;     ///< 非拥有指针
    ButtonView* nextButton_ = nullptr;   ///< 非拥有指针

    void updateDisplay();
};

} // namespace ink
