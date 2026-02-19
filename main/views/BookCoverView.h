/**
 * @file BookCoverView.h
 * @brief 书籍封面缩略图 View。
 *
 * 52×72px 固定尺寸，绘制边框、格式标签和右上角折角装饰。
 */

#pragma once

#include <string>
#include "ink_ui/core/View.h"

extern "C" {
struct font_engine_t;
}

/// 书籍封面缩略图视图
class BookCoverView : public ink::View {
public:
    BookCoverView();

    /// 设置格式标签（如 "TXT"）
    void setFormatTag(const std::string& tag);

    /// 设置字体引擎和字号
    void setFont(font_engine_t* engine, uint8_t fontSize);

    void onDraw(ink::Canvas& canvas) override;
    ink::Size intrinsicSize() const override;

private:
    std::string formatTag_ = "TXT";
    font_engine_t* engine_ = nullptr;
    uint8_t fontSize_ = 0;

    static constexpr int kCoverWidth = 52;
    static constexpr int kCoverHeight = 72;
    static constexpr int kFoldSize = 10;  // 右上角折角尺寸
};
