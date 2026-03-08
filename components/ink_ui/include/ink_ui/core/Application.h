/**
 * @file Application.h
 * @brief InkUI 应用主循环 -- 整合事件分发、触摸手势和渲染引擎。
 */

#pragma once

#include <memory>

#include "ink_ui/core/Event.h"
#include "ink_ui/core/RenderEngine.h"
#include "ink_ui/core/GestureRecognizer.h"
#include "ink_ui/core/ModalPresenter.h"
#include "ink_ui/core/NavigationController.h"
#include "ink_ui/hal/DisplayDriver.h"
#include "ink_ui/hal/TouchDriver.h"
#include "ink_ui/hal/Platform.h"
#include "ink_ui/hal/SystemInfo.h"

namespace ink {

class StatusBarView;

/// InkUI 应用主循环
class Application {
public:
    Application() = default;

    /// 初始化所有子系统（接受 HAL 依赖注入）
    bool init(DisplayDriver& display, TouchDriver& touch,
              Platform& platform, SystemInfo& system);

    /// 启动主事件循环（永不返回）
    [[noreturn]] void run();

    /// 发送事件到队列（线程安全）
    void postEvent(const Event& event);

    /// 延迟发送事件
    void postDelayed(const Event& event, int delayMs);

    /// 获取导航控制器
    NavigationController& navigator() { return navigator_; }

    /// 获取渲染引擎
    RenderEngine& renderer() { return *renderEngine_; }

    /// 获取状态栏 View（供外部设置字体等）
    StatusBarView* statusBar() { return statusBar_; }

    /// 获取模态呈现器
    ModalPresenter& modalPresenter() { return *modalPresenter_; }

    /// 请求全屏 W>B>GL 过渡刷新（标脏整个 window 树 + 设置过渡标志）
    void requestTransitionRefresh();

private:
    NavigationController navigator_;
    QueueHandle eventQueue_ = nullptr;
    std::unique_ptr<RenderEngine> renderEngine_;
    std::unique_ptr<GestureRecognizer> gesture_;

    // ── HAL 引用 ──
    DisplayDriver* display_ = nullptr;
    Platform* platform_ = nullptr;
    SystemInfo* system_ = nullptr;

    // ── Screen View 树 ──
    std::unique_ptr<View> screenRoot_;         ///< 全屏根 View（FlexDirection::None）
    View* windowRoot_ = nullptr;               ///< 非拥有，screenRoot_ 子树中
    View* overlayRoot_ = nullptr;              ///< 非拥有，screenRoot_ 子树中
    StatusBarView* statusBar_ = nullptr;       ///< 非拥有，windowRoot_ 子树中
    View* contentArea_ = nullptr;              ///< 非拥有，windowRoot_ 子树中
    ViewController* mountedVC_ = nullptr;      ///< 当前挂载的 VC
    std::unique_ptr<ModalPresenter> modalPresenter_;  ///< 模态呈现器
    int lastMinute_ = -1;                      ///< 时间更新追踪
    int64_t lastBatteryUpdateUs_ = 0;          ///< 上次电池更新时间 (us)

    /// 事件队列超时（30 秒）
    static constexpr int kQueueTimeoutMs = 30000;

    /// 事件队列容量
    static constexpr int kEventQueueSize = 16;

    /// 电池更新间隔 (30 秒, 单位 us)
    static constexpr int64_t kBatteryUpdateIntervalUs = 30 * 1000 * 1000LL;

    /// 构建 Screen View 树（screenRoot_ + windowRoot_ + overlayRoot_）
    void buildWindowTree();

    /// 判断 View 是否为 overlayRoot_ 的子孙节点
    bool isInOverlay(const View* view) const;

    /// 挂载 VC 的 root view 到 contentArea_
    void mountViewController(ViewController* vc);

    /// 分发事件
    void dispatchEvent(const Event& event);

    /// 分发触摸事件：hitTest -> onTouchEvent 冒泡
    void dispatchTouch(const TouchEvent& touch);
};

} // namespace ink
