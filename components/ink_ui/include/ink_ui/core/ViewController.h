/**
 * @file ViewController.h
 * @brief 页面控制器基类 — 管理 View 树的创建和生命周期。
 *
 * 子类必须 override viewDidLoad() 以创建 View 树。
 * getView() 首次调用时触发 viewDidLoad（懒加载）。
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

    /// 获取根 View（首次调用触发 viewDidLoad）
    View* getView();

    // ── 生命周期回调（子类可选 override）──

    /// 创建 View 树（纯虚函数，只调用一次）
    virtual void viewDidLoad() = 0;

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

    // ── 属性 ──

    std::string title_;

protected:
    /// 根 View（子类在 viewDidLoad 中创建并赋值）
    std::unique_ptr<View> view_;

private:
    bool viewLoaded_ = false;
};

} // namespace ink
