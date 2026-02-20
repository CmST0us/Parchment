/**
 * @file ViewController.h
 * @brief 页面控制器基类 — 管理 View 树的创建和生命周期。
 *
 * 子类可 override loadView() 创建自定义 View 树，
 * override viewDidLoad() 做加载后的额外配置。
 * view() / getView() 首次调用时触发懒加载（loadView → viewDidLoad）。
 */

#pragma once

#include <memory>
#include <string>

#include "ink_ui/core/View.h"
#include "ink_ui/core/Event.h"

namespace ink {

/// 页面控制器基类
class ViewController {
public:
    ViewController() = default;
    virtual ~ViewController() = default;

    // 不可拷贝
    ViewController(const ViewController&) = delete;
    ViewController& operator=(const ViewController&) = delete;

    // ── View 访问 ──

    /// 获取根 View（首次调用触发 loadView + viewDidLoad）
    View* view();

    /// 获取根 View（等价于 view()，保持向后兼容）
    View* getView();

    /// 查询 view 是否已加载
    bool isViewLoaded() const { return viewLoaded_; }

    /// 取出 View 所有权（用于挂载到 Window 树）
    std::unique_ptr<View> takeView();

    /// 归还 View 所有权（从 Window 树卸载后）
    void returnView(std::unique_ptr<View> view);

    // ── 生命周期回调（子类可选 override）──

    /// View 加载后的配置回调（子类可 override，默认空实现）
    virtual void viewDidLoad() {}

    /// 即将变为可见
    virtual void viewWillAppear() {}

    /// 已变为可见
    virtual void viewDidAppear() {}

    /// 即将变为不可见
    virtual void viewWillDisappear() {}

    /// 已变为不可见
    virtual void viewDidDisappear() {}

    // ── 事件处理 ──

    /// 处理非触摸事件（SwipeEvent、TimerEvent 等）
    virtual void handleEvent(const Event& event) { (void)event; }

    /// 是否隐藏状态栏（默认 false，子类可覆写）
    virtual bool prefersStatusBarHidden() const { return false; }

    // ── 属性 ──

    std::string title_;

protected:
    /// 创建 View 树（子类可 override，默认创建空白 View）
    virtual void loadView();

    /// 根 View（子类在 loadView 中创建并赋值）
    std::unique_ptr<View> view_;

private:
    bool viewLoaded_ = false;
    View* viewRawPtr_ = nullptr;  ///< 缓存 raw pointer，view_ 被 take 后仍有效
};

} // namespace ink
