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
#include "epdiy.h"
}

/// 书籍封面缩略图视图
class BookCoverView : public ink::View {
public:
    BookCoverView();

    /// 设置格式标签（如 "TXT"）
    void setFormatTag(const std::string& tag);

    /// 设置绘制文字的字体
    void setFont(const EpdFont* font);

    void onDraw(ink::Canvas& canvas) override;
    ink::Size intrinsicSize() const override;

private:
    std::string formatTag_ = "TXT";
    const EpdFont* font_ = nullptr;

    static constexpr int kCoverWidth = 52;
    static constexpr int kCoverHeight = 72;
    static constexpr int kFoldSize = 10;  // 右上角折角尺寸
};
