/**
 * @file Application.h
 * @brief InkUI 应用主循环 — 整合事件分发、触摸手势和渲染引擎。
 */

#pragma once

#include <memory>

#include "ink_ui/core/Event.h"
#include "ink_ui/core/EpdDriver.h"
#include "ink_ui/core/RenderEngine.h"
#include "ink_ui/core/GestureRecognizer.h"
#include "ink_ui/core/NavigationController.h"

namespace ink {

class StatusBarView;

/// InkUI 应用主循环
class Application {
public:
    Application() = default;

    /// 初始化所有子系统
    bool init();

    /// 启动主事件循环（永不返回）
    [[noreturn]] void run();

    /// 发送事件到队列（线程安全）
    void postEvent(const Event& event);

    /// 延迟发送事件（使用 esp_timer）
    void postDelayed(const Event& event, int delayMs);

    /// 获取导航控制器
    NavigationController& navigator() { return navigator_; }

    /// 获取渲染引擎
    RenderEngine& renderer() { return *renderEngine_; }

    /// 获取状态栏 View（供外部设置字体等）
    StatusBarView* statusBar() { return statusBar_; }

private:
    NavigationController navigator_;
    QueueHandle_t eventQueue_ = nullptr;
    std::unique_ptr<RenderEngine> renderEngine_;
    std::unique_ptr<GestureRecognizer> gesture_;

    // ── Window View 树 ──
    std::unique_ptr<View> windowRoot_;
    StatusBarView* statusBar_ = nullptr;       ///< 非拥有，windowRoot_ 子树中
    View* contentArea_ = nullptr;              ///< 非拥有，windowRoot_ 子树中
    ViewController* mountedVC_ = nullptr;      ///< 当前挂载的 VC
    int lastMinute_ = -1;                      ///< 时间更新追踪

    /// 事件队列超时（30 秒）
    static constexpr int kQueueTimeoutMs = 30000;

    /// 事件队列容量
    static constexpr int kEventQueueSize = 16;

    /// 构建 Window View 树（windowRoot_ + statusBar_ + contentArea_）
    void buildWindowTree();

    /// 挂载 VC 的 root view 到 contentArea_
    void mountViewController(ViewController* vc);

    /// 分发事件
    void dispatchEvent(const Event& event);

    /// 分发触摸事件：hitTest → onTouchEvent 冒泡
    void dispatchTouch(const TouchEvent& touch);
};

} // namespace ink
