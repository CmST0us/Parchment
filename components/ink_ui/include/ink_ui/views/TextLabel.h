/**
 * @file TextLabel.h
 * @brief TextLabel — 单行/多行文字显示 View。
 */

#pragma once

#include <string>

#include "ink_ui/core/Canvas.h"
#include "ink_ui/core/View.h"

namespace ink {

/// TextLabel: 显示单行或多行文字
class TextLabel : public View {
public:
    TextLabel();
    ~TextLabel() override = default;

    // ── Setters ──

    /// 设置文字内容（相同文字不触发重绘）
    void setText(const std::string& text);

    /// 设置字体
    void setFont(const EpdFont* font);

    /// 设置文字颜色
    void setColor(uint8_t color);

    /// 设置水平对齐方式
    void setAlignment(Align alignment);

    /// 设置最大行数（0 = 不限制，默认 1）
    void setMaxLines(int maxLines);

    // ── Getters ──

    const std::string& text() const { return text_; }
    const EpdFont* font() const { return font_; }
    uint8_t color() const { return color_; }
    Align alignment() const { return alignment_; }
    int maxLines() const { return maxLines_; }

    // ── View 覆写 ──

    void onDraw(Canvas& canvas) override;
    Size intrinsicSize() const override;

private:
    std::string text_;
    const EpdFont* font_ = nullptr;
    uint8_t color_ = Color::Black;
    Align alignment_ = Align::Start;
    int maxLines_ = 1;
};

} // namespace ink
