/**
 * @file NavigationController.h
 * @brief 页面栈导航控制器 — 管理 ViewController 入栈/出栈和生命周期。
 */

#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "ink_ui/core/ViewController.h"

namespace ink {

/// 最大页面栈深度
constexpr int MAX_NAV_DEPTH = 8;

/// 页面栈导航控制器
class NavigationController {
public:
    NavigationController() = default;

    /// 将新 ViewController 推入栈顶
    void push(std::unique_ptr<ViewController> vc);

    /// 弹出栈顶 ViewController（栈只剩 1 个时拒绝）
    void pop();

    /// 替换栈顶 ViewController
    void replace(std::unique_ptr<ViewController> vc);

    /// 返回栈顶 ViewController（栈为空时返回 nullptr）
    ViewController* current() const;

    /// 返回当前栈深度
    int depth() const;

    /// 设置 VC 切换回调（在 getView() 之后、viewWillAppear() 之前触发）
    void setOnViewControllerChange(std::function<void(ViewController*)> callback);

private:
    std::vector<std::unique_ptr<ViewController>> stack_;
    std::function<void(ViewController*)> onVCChange_;
};

} // namespace ink
