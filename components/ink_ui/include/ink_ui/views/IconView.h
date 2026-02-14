/**
 * @file IconView.h
 * @brief IconView — 32x32 4bpp 图标显示 View。
 */

#pragma once

#include <cstdint>

#include "ink_ui/core/Canvas.h"
#include "ink_ui/core/View.h"

namespace ink {

/// IconView: 渲染 32x32 4bpp 位图图标
class IconView : public View {
public:
    IconView();
    ~IconView() override = default;

    /// 设置图标数据（32x32 4bpp，512 bytes）
    void setIcon(const uint8_t* data);

    /// 设置前景着色颜色
    void setTintColor(uint8_t color);

    const uint8_t* iconData() const { return iconData_; }
    uint8_t tintColor() const { return tintColor_; }

    // ── View 覆写 ──

    void onDraw(Canvas& canvas) override;
    Size intrinsicSize() const override;

private:
    static constexpr int kIconSize = 32;

    const uint8_t* iconData_ = nullptr;
    uint8_t tintColor_ = Color::Black;
};

} // namespace ink
