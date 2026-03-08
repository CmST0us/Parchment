/**
 * @file SheetView.h
 * @brief iOS 16 风格底部 Sheet 视图组件。
 *
 * 全宽卡片从屏幕底部弹出，顶部带黑色边框线和抓手 Handle。
 * 提供配置式 API 添加标题和可点击设置项，内部延迟构建 View 树。
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ink_ui/core/View.h"

struct EpdFont;

namespace ink {

/// iOS 16 风格底部 Sheet 视图
class SheetView : public View {
public:
    SheetView();
    ~SheetView() override = default;

    /// 设置标题文字
    void setTitle(const std::string& title);

    /// 设置列表项字体
    void setFont(const EpdFont* font);

    /// 设置标题字体（可选，不设置则使用基础字体）
    void setTitleFont(const EpdFont* titleFont);

    /// 添加可点击的设置项
    void addItem(const std::string& label, std::function<void()> onTap);

    // ── View 覆写 ──

    void onDraw(Canvas& canvas) override;
    void onLayout() override;
    Size intrinsicSize() const override;

private:
    /// 顶部边框线宽度
    static constexpr int kTopBorderWidth = 1;
    /// 抓手 Handle 尺寸和边距
    static constexpr int kHandleWidth = 40;
    static constexpr int kHandleHeight = 4;
    static constexpr int kHandleTopMargin = 12;
    static constexpr int kHandleBottomMargin = 8;
    /// 内容区 padding
    static constexpr int kContentPadLeft = 24;
    static constexpr int kContentPadRight = 24;
    static constexpr int kContentPadBottom = 24;
    /// 设置项高度
    static constexpr int kItemHeight = 48;
    /// 屏幕宽度
    static constexpr int kScreenWidth = 540;

    const EpdFont* font_ = nullptr;
    const EpdFont* titleFont_ = nullptr;
    std::string title_;

    /// 设置项配置
    struct ItemInfo {
        std::string label;
        std::function<void()> onTap;
    };
    std::vector<ItemInfo> items_;

    mutable bool needsRebuild_ = true;

    /// 清除子 View 并根据当前配置重建内部 View 树
    void rebuild();

    /// 计算内容区域起始 Y（阴影 + Handle 区域之后）
    int contentStartY() const;

    /// 计算总高度
    int totalHeight() const;
};

} // namespace ink
