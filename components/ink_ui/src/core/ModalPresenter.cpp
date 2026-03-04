/**
 * @file ModalPresenter.cpp
 * @brief 模态视图呈现器实现。
 *
 * 管理 Toast（非阻塞）和 Modal（阻塞）双通道模态系统。
 * Toast 自动消失，Modal 需显式 dismiss。
 * Modal 通道内支持优先级队列（Alert > ActionSheet > Loading）。
 */

#include "ink_ui/core/ModalPresenter.h"
#include "ink_ui/core/Application.h"
#include "ink_ui/core/RenderEngine.h"

#include <cstdio>

namespace {

/// 透明背景 View，捕获点击触发 dismiss（Sheet 外区域关闭用）
class BackdropView : public ink::View {
public:
    ink::ModalPresenter* presenter_ = nullptr;

    bool onTouchEvent(const ink::TouchEvent& event) override {
        if (event.type == ink::TouchType::Tap && presenter_) {
            presenter_->dismiss(ink::ModalChannel::Modal);
            return true;
        }
        return false;
    }
};

} // anonymous namespace

namespace ink {

ModalPresenter::ModalPresenter(View* overlayRoot, View* screenRoot,
                               RenderEngine& renderer, Application& app)
    : overlayRoot_(overlayRoot)
    , screenRoot_(screenRoot)
    , renderer_(renderer)
    , app_(app) {
}

// ── Toast 呈现 ──

void ModalPresenter::showToast(std::unique_ptr<View> content, int durationMs) {
    if (!content) return;

    if (activeToast_) {
        // Toast 通道正忙，入队等待
        ModalRequest req;
        req.view = std::move(content);
        req.priority = ModalPriority::Loading;  // Toast 不使用优先级
        req.durationMs = durationMs;
        toastQueue_.push_back(std::move(req));
        return;
    }

    // 定位并显示
    View* view = content.get();
    positionToast(view);
    activeToast_ = view;
    overlayRoot_->addSubview(std::move(content));
    updateOverlayVisibility();

    // 注册自动消失定时器（使用 generation 防止 stale timer）
    toastTimerId_ = kTimerIdBase + (++toastTimerGen_ & kTimerIdMask);
    Event timerEvent = Event::makeTimer(toastTimerId_);
    app_.postDelayed(timerEvent, durationMs);

    fprintf(stderr, "ink::ModalPresenter: Toast shown, duration=%dms\n", durationMs);
}

// ── Modal 呈现 ──

void ModalPresenter::showAlert(std::unique_ptr<View> content) {
    showModal(std::move(content), ModalPriority::Alert);
}

void ModalPresenter::showActionSheet(std::unique_ptr<View> content) {
    showModal(std::move(content), ModalPriority::ActionSheet);
}

void ModalPresenter::showLoading(std::unique_ptr<View> content) {
    showModal(std::move(content), ModalPriority::Loading);
}

void ModalPresenter::showSheet(std::unique_ptr<View> content) {
    if (!content) return;

    // 计算 Sheet 高度和位置
    Size sheetSize = content->intrinsicSize();
    int sheetH = (sheetSize.h > 0) ? sheetSize.h : 400;
    int sheetY = kScreenHeight - sheetH;

    // 设置 Sheet 的 frame
    content->setFrame({0, sheetY, kScreenWidth, sheetH});

    // 创建全屏 wrapper（透明，无布局）
    auto wrapper = std::make_unique<View>();
    wrapper->setFrame({0, 0, kScreenWidth, kScreenHeight});
    wrapper->setBackgroundColor(Color::Clear);
    wrapper->setOpaque(false);
    wrapper->flexStyle_.direction = FlexDirection::None;

    // BackdropView 覆盖 Sheet 上方区域，点击触发关闭
    auto backdrop = std::make_unique<BackdropView>();
    backdrop->setBackgroundColor(Color::Clear);
    backdrop->setOpaque(false);
    backdrop->setFrame({0, 0, kScreenWidth, sheetY});
    backdrop->presenter_ = this;
    wrapper->addSubview(std::move(backdrop));

    // Sheet 内容
    wrapper->addSubview(std::move(content));

    // 通过 showModal 呈现（ActionSheet 优先级）
    // wrapper 全屏，centerOnScreen 会定位到 {0, 0}
    showModal(std::move(wrapper), ModalPriority::ActionSheet);
}

void ModalPresenter::showModal(std::unique_ptr<View> content, ModalPriority priority) {
    if (!content) return;

    if (activeModal_) {
        if (priority > activeModalPriority_) {
            // 高优先级打断：暂存当前模态到队列前端
            Rect damageRect = activeModal_->screenFrame();
            auto owned = activeModal_->removeFromParent();
            if (owned) {
                ModalRequest suspended;
                suspended.view = std::move(owned);
                suspended.priority = activeModalPriority_;
                suspended.durationMs = 0;
                modalQueue_.push_front(std::move(suspended));
            }
            activeModal_ = nullptr;

            // 修复被暂存模态覆盖的区域
            renderer_.repairDamage(screenRoot_, damageRect);
        } else {
            // 同或低优先级：入队等待
            ModalRequest req;
            req.view = std::move(content);
            req.priority = priority;
            req.durationMs = 0;
            modalQueue_.push_back(std::move(req));
            return;
        }
    }

    // 定位并显示
    View* view = content.get();
    centerOnScreen(view);
    activeModal_ = view;
    activeModalPriority_ = priority;
    overlayRoot_->addSubview(std::move(content));
    updateOverlayVisibility();

    fprintf(stderr, "ink::ModalPresenter: Modal shown, priority=%d\n",
            static_cast<int>(priority));
}

// ── 关闭模态 ──

void ModalPresenter::dismiss(ModalChannel channel) {
    if (channel == ModalChannel::Toast) {
        if (!activeToast_) return;

        Rect damageRect = activeToast_->screenFrame();
        activeToast_->removeFromParent();
        activeToast_ = nullptr;

        // 显示队列中下一个 Toast
        if (!toastQueue_.empty()) {
            ModalRequest next = std::move(toastQueue_.front());
            toastQueue_.pop_front();

            View* view = next.view.get();
            positionToast(view);
            activeToast_ = view;
            overlayRoot_->addSubview(std::move(next.view));

            // 注册自动消失定时器（新 generation）
            toastTimerId_ = kTimerIdBase + (++toastTimerGen_ & kTimerIdMask);
            Event timerEvent = Event::makeTimer(toastTimerId_);
            app_.postDelayed(timerEvent, next.durationMs);
        }

        updateOverlayVisibility();
        renderer_.repairDamage(screenRoot_, damageRect);

        fprintf(stderr, "ink::ModalPresenter: Toast dismissed\n");

    } else {  // ModalChannel::Modal
        if (!activeModal_) return;

        Rect damageRect = activeModal_->screenFrame();
        activeModal_->removeFromParent();
        activeModal_ = nullptr;

        // 从队列中找到下一个要显示的模态（优先级最高的）
        if (!modalQueue_.empty()) {
            // 找到优先级最高的
            auto bestIt = modalQueue_.begin();
            for (auto it = modalQueue_.begin(); it != modalQueue_.end(); ++it) {
                if (it->priority > bestIt->priority) {
                    bestIt = it;
                }
            }

            ModalRequest next = std::move(*bestIt);
            modalQueue_.erase(bestIt);

            View* view = next.view.get();
            centerOnScreen(view);
            activeModal_ = view;
            activeModalPriority_ = next.priority;
            overlayRoot_->addSubview(std::move(next.view));
        }

        updateOverlayVisibility();
        renderer_.repairDamage(screenRoot_, damageRect);

        fprintf(stderr, "ink::ModalPresenter: Modal dismissed\n");
    }
}

// ── Timer 处理 ──

bool ModalPresenter::handleTimer(int timerId) {
    // 检查是否在 ModalPresenter 保留的 timer ID 范围内
    if (timerId < kTimerIdBase || timerId > kTimerIdBase + kTimerIdMask) return false;

    // 只处理当前 generation 的 timer，忽略 stale timer
    if (timerId != toastTimerId_) return true;  // 消费但不执行

    // Toast 自动消失
    if (activeToast_) {
        dismiss(ModalChannel::Toast);
        return true;
    }
    return true;  // 属于 ModalPresenter 的 timer，即使无 activeToast 也消费
}

// ── 状态查询 ──

bool ModalPresenter::isBlocking() const {
    return activeModal_ != nullptr;
}

// ── 定位辅助 ──

void ModalPresenter::centerOnScreen(View* view) {
    if (!view) return;

    Rect f = view->frame();
    Size size = view->intrinsicSize();

    // 优先使用显式设置的 frame 尺寸，回退到 intrinsicSize
    int w = (f.w > 0) ? f.w : (size.w > 0) ? size.w : 0;
    int h = (f.h > 0) ? f.h : (size.h > 0) ? size.h : 0;

    int x = (kScreenWidth - w) / 2;
    int y = (kScreenHeight - h) / 2;

    view->setFrame({x, y, w, h});
}

void ModalPresenter::positionToast(View* view) {
    if (!view) return;

    Rect f = view->frame();
    Size size = view->intrinsicSize();

    int w = (f.w > 0) ? f.w : (size.w > 0) ? size.w : 0;
    int h = (f.h > 0) ? f.h : (size.h > 0) ? size.h : 0;

    int x = (kScreenWidth - w) / 2;
    int y = kToastTopMargin;

    view->setFrame({x, y, w, h});
}

void ModalPresenter::updateOverlayVisibility() {
    if (!overlayRoot_) return;

    bool hasChildren = !overlayRoot_->subviews().empty();
    overlayRoot_->setHidden(!hasChildren);

    if (hasChildren) {
        overlayRoot_->setNeedsDisplay();
    }
}

} // namespace ink
