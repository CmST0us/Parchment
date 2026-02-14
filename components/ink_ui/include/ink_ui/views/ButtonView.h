/**
 * @file ButtonView.h
 * @brief ButtonView — 可点击按钮 View。
 */

#pragma once

#include <functional>
#include <string>

#include "ink_ui/core/Canvas.h"
#include "ink_ui/core/View.h"

namespace ink {

/// 按钮样式
enum class ButtonStyle {
    Primary,    ///< 黑底白字（主按钮）
    Secondary,  ///< 白底黑字黑边框（次按钮）
    Icon,       ///< 透明底 + 图标（图标按钮）
};

/// ButtonView: 可点击按钮，支持 icon + text、三种样式
class ButtonView : public View {
public:
    ButtonView();
    ~ButtonView() override = default;

    // ── Setters ──

    /// 设置按钮文字（相同文字不触发重绘）
    void setLabel(const std::string& label);

    /// 设置字体
    void setFont(const EpdFont* font);

    /// 设置图标数据（32x32 4bpp）
    void setIcon(const uint8_t* iconData);

    /// 设置按钮样式
    void setStyle(ButtonStyle style);

    /// 设置点击回调
    void setOnTap(std::function<void()> onTap);

    /// 设置启用/禁用状态
    void setEnabled(bool enabled);

    // ── Getters ──

    const std::string& label() const { return label_; }
    const EpdFont* font() const { return font_; }
    const uint8_t* icon() const { return icon_; }
    ButtonStyle style() const { return style_; }
    bool enabled() const { return enabled_; }

    // ── View 覆写 ──

    void onDraw(Canvas& canvas) override;
    bool onTouchEvent(const TouchEvent& event) override;
    Size intrinsicSize() const override;

private:
    static constexpr int kPaddingH = 16;   ///< 水平 padding（左右各 16px）
    static constexpr int kHeight = 48;     ///< 按钮高度
    static constexpr int kIconSize = 32;   ///< 图标尺寸

    std::string label_;
    const EpdFont* font_ = nullptr;
    const uint8_t* icon_ = nullptr;
    ButtonStyle style_ = ButtonStyle::Primary;
    std::function<void()> onTap_;
    bool enabled_ = true;
};

} // namespace ink
