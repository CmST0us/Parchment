/**
 * @file ModalPresenter.h
 * @brief 模态视图呈现器 — 管理 Toast 和 Modal 双通道模态系统。
 */

#pragma once

#include <deque>
#include <memory>

#include "ink_ui/core/Geometry.h"
#include "ink_ui/core/View.h"

namespace ink {

class Application;
class RenderEngine;

/// 模态通道
enum class ModalChannel {
    Toast,      ///< 顶部非阻塞通道（自动消失）
    Modal,      ///< 居中阻塞通道（需显式 dismiss）
};

/// 模态优先级（仅 Modal 通道使用）
enum class ModalPriority {
    Loading = 0,        ///< 最低
    ActionSheet = 1,
    Alert = 2,          ///< 最高
};

/// 模态视图呈现器
class ModalPresenter {
public:
    /// 构造，接收依赖注入
    ModalPresenter(View* overlayRoot, View* screenRoot,
                   RenderEngine& renderer, Application& app);

    /// 显示 Toast（顶部居中，非阻塞，自动消失）
    void showToast(std::unique_ptr<View> content, int durationMs = 2000);

    /// 显示 Alert（居中，阻塞，优先级最高）
    void showAlert(std::unique_ptr<View> content);

    /// 显示 ActionSheet（居中，阻塞，中优先级）
    void showActionSheet(std::unique_ptr<View> content);

    /// 显示 Loading（居中，阻塞，最低优先级）
    void showLoading(std::unique_ptr<View> content);

    /// 显示底部 Sheet（全宽，从底部弹出，阻塞，中优先级）
    void showSheet(std::unique_ptr<View> content);

    /// 关闭指定通道的当前模态
    void dismiss(ModalChannel channel);

    /// Modal 通道是否有活跃模态（用于事件拦截）
    bool isBlocking() const;

    /// 处理 Timer 事件，如果 timerId 属于 ModalPresenter 范围则消费并返回 true
    bool handleTimer(int timerId);

private:
    View* overlayRoot_;             ///< 叠加层根 View（非拥有）
    View* screenRoot_;              ///< 顶层根 View（用于 repairDamage）
    RenderEngine& renderer_;        ///< 渲染引擎
    Application& app_;              ///< 应用实例（用于 postDelayed）

    /// ModalPresenter 保留的 Timer ID 起始值和范围掩码
    static constexpr int kTimerIdBase = 0x7F00;
    static constexpr int kTimerIdMask = 0xFF;  ///< 低 8 位用于 generation

    /// Toast 默认顶部 margin（StatusBar 高度 + 间距）
    static constexpr int kToastTopMargin = 36;

    /// 屏幕尺寸常量
    static constexpr int kScreenWidth = 540;
    static constexpr int kScreenHeight = 960;

    /// 模态请求（用于排队）
    struct ModalRequest {
        std::unique_ptr<View> view;
        ModalPriority priority;
        int durationMs;     ///< 仅 Toast 使用，0 表示不自动消失
    };

    // ── Toast 通道状态 ──
    View* activeToast_ = nullptr;               ///< 当前显示的 Toast（非拥有）
    std::deque<ModalRequest> toastQueue_;        ///< Toast 等待队列
    int toastTimerId_ = kTimerIdBase;            ///< 当前 Toast 的 timerId
    int toastTimerGen_ = 0;                     ///< Timer generation 防止 stale timer

    // ── Modal 通道状态 ──
    View* activeModal_ = nullptr;               ///< 当前显示的 Modal（非拥有）
    ModalPriority activeModalPriority_ = ModalPriority::Loading;
    std::deque<ModalRequest> modalQueue_;        ///< Modal 等待/暂存队列

    /// 显示 Modal 通道的模态（内部实现）
    void showModal(std::unique_ptr<View> content, ModalPriority priority);

    /// 将 View 定位到屏幕居中
    void centerOnScreen(View* view);

    /// 将 View 定位到顶部居中
    void positionToast(View* view);

    /// 设置 overlayRoot_ 的可见性（有子 View 则 visible，无则 hidden）
    void updateOverlayVisibility();
};

} // namespace ink
