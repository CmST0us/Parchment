/**
 * @file AlertView.h
 * @brief AlertView — iOS 风格 Alert 弹窗组件。
 *
 * 封装 ShadowCardView 容器 + 标题 + 消息 + 分隔线 + 按钮行的完整构建逻辑，
 * 提供简洁的配置式 API，避免每次弹 Alert 都重复构建代码。
 */

#pragma once

#include <functional>
#include <string>
#include <vector>

#include "ink_ui/views/ShadowCardView.h"

struct EpdFont;

namespace ink {

/// iOS 风格 Alert 弹窗，带标题、消息和操作按钮
class AlertView : public ShadowCardView {
public:
    AlertView();
    ~AlertView() override = default;

    /// 设置标题文字
    void setTitle(const std::string& title);

    /// 设置消息文字
    void setMessage(const std::string& message);

    /// 设置基础字体（消息和按钮共用）
    void setFont(const EpdFont* font);

    /// 设置标题字体（可选，不设置则使用基础字体）
    void setTitleFont(const EpdFont* titleFont);

    /// 添加按钮（按添加顺序从左到右排列）
    /// isPrimary=true 时文字为黑色（确定），false 为灰色（取消）
    void addButton(const std::string& label, std::function<void()> onTap,
                   bool isPrimary = false);

    // ── View 覆写 ──

    Size intrinsicSize() const override;
    void onLayout() override;

private:
    const EpdFont* font_ = nullptr;       ///< 消息/按钮字体
    const EpdFont* titleFont_ = nullptr;  ///< 标题字体
    std::string title_;
    std::string message_;

    /// 按钮配置信息
    struct ButtonInfo {
        std::string label;
        std::function<void()> onTap;
        bool isPrimary;
    };
    std::vector<ButtonInfo> buttons_;

    mutable bool needsRebuild_ = true;

    /// 清除子 View 并根据当前配置重建内部 View 树
    void rebuild();
};

} // namespace ink
