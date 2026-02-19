/**
 * @file HeaderView.h
 * @brief HeaderView — 页面顶部标题栏组件。
 */

#pragma once

#include <functional>
#include <string>

#include "ink_ui/core/Canvas.h"
#include "ink_ui/core/View.h"
#include "ink_ui/views/ButtonView.h"
#include "ink_ui/views/TextLabel.h"

extern "C" {
struct font_engine_t;
}

namespace ink {

/// HeaderView: 黑底白字标题栏（leftIcon + title + rightIcon）
class HeaderView : public View {
public:
    HeaderView();
    ~HeaderView() override = default;

    /// 设置标题文字
    void setTitle(const std::string& title);

    /// 设置左侧图标按钮
    void setLeftIcon(const uint8_t* iconData, std::function<void()> onTap);

    /// 设置右侧图标按钮
    void setRightIcon(const uint8_t* iconData, std::function<void()> onTap);

    /// 设置标题字体引擎和字号
    void setFont(font_engine_t* engine, uint8_t fontSize);

    // ── View 覆写 ──

    void onLayout() override;
    Size intrinsicSize() const override;

private:
    static constexpr int kHeight = 48;
    static constexpr int kButtonSize = 48;

    TextLabel* titleLabel_ = nullptr;     ///< 非拥有指针（子 View 持有）
    ButtonView* leftButton_ = nullptr;    ///< 非拥有指针
    ButtonView* rightButton_ = nullptr;   ///< 非拥有指针

    // 持久化按钮数据（rebuild 时使用）
    const uint8_t* leftIconData_ = nullptr;
    std::function<void()> leftTap_;
    const uint8_t* rightIconData_ = nullptr;
    std::function<void()> rightTap_;
    font_engine_t* engine_ = nullptr;
    uint8_t fontSize_ = 0;

    void rebuild();
};

} // namespace ink
