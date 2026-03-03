/**
 * @file Application.cpp
 * @brief InkUI 应用主循环实现。
 */

#include "ink_ui/core/Application.h"
#include "ink_ui/core/Canvas.h"
#include "ink_ui/views/StatusBarView.h"

#include <cstdio>
#include <ctime>

namespace ink {

// ── 延迟事件上下文 ──

struct DelayedEventCtx {
    Platform* platform;
    QueueHandle queue;
    Event event;
};

static void delayedTimerCallback(void* arg) {
    auto* ctx = static_cast<DelayedEventCtx*>(arg);
    ctx->platform->queueSend(ctx->queue, &ctx->event, 0);
    delete ctx;
}

// ── Window View 树构建 ──

void Application::buildWindowTree() {
    // windowRoot_: 全屏 Column 容器
    windowRoot_ = std::make_unique<View>();
    windowRoot_->setFrame({0, 0, kScreenWidth, kScreenHeight});
    windowRoot_->setBackgroundColor(Color::White);
    windowRoot_->flexStyle_.direction = FlexDirection::Column;
    windowRoot_->flexStyle_.alignItems = Align::Stretch;

    // statusBar_: 20px 高，由 Application 管理（字体由 main.cpp 设置）
    auto sb = std::make_unique<StatusBarView>();
    sb->flexBasis_ = 20;
    statusBar_ = sb.get();
    windowRoot_->addSubview(std::move(sb));

    // contentArea_: 填满剩余空间，VC 的 root view 挂载于此
    auto ca = std::make_unique<View>();
    ca->flexGrow_ = 1;
    ca->flexStyle_.direction = FlexDirection::Column;
    ca->flexStyle_.alignItems = Align::Stretch;
    contentArea_ = ca.get();
    windowRoot_->addSubview(std::move(ca));

    fprintf(stderr, "ink::App: Window tree built: statusBar(20px) + contentArea\n");
}

// ── VC View 挂载 ──

void Application::mountViewController(ViewController* vc) {
    if (!contentArea_) return;

    // 1. 归还旧 VC 的 View 所有权（旧 VC 此时仍然存活）
    if (mountedVC_ && !contentArea_->subviews().empty()) {
        View* oldView = contentArea_->subviews().front().get();
        auto owned = oldView->removeFromParent();
        if (owned) {
            mountedVC_->returnView(std::move(owned));
        }
    }

    mountedVC_ = vc;
    if (!vc) return;

    // 2. 根据 VC 偏好设置状态栏可见性
    if (statusBar_) {
        bool hide = vc->prefersStatusBarHidden();
        statusBar_->setHidden(hide);
        if (!hide) {
            statusBar_->updateTime();
        }
    }

    // 3. 取出新 VC 的 View 所有权并挂载到 contentArea_
    View* vcView = vc->getView();
    if (vcView) {
        vcView->flexGrow_ = 1;
        auto owned = vc->takeView();
        if (owned) {
            contentArea_->addSubview(std::move(owned));
        }
    }

    // 4. 触发整个 Window 树重新布局
    windowRoot_->setNeedsLayout();
    windowRoot_->setNeedsDisplay();

    fprintf(stderr, "ink::App: Mounted VC: '%s', statusBar=%s\n",
            vc->title_.c_str(),
            (statusBar_ && !statusBar_->isHidden()) ? "visible" : "hidden");
}

// ── 初始化 ──

bool Application::init(DisplayDriver& display, TouchDriver& touch,
                       Platform& platform, SystemInfo& system) {
    fprintf(stderr, "ink::App: Initializing InkUI Application...\n");

    // 保存 HAL 引用
    display_ = &display;
    platform_ = &platform;
    system_ = &system;

    // 1. DisplayDriver 初始化
    if (!display.init()) {
        fprintf(stderr, "ink::App: DisplayDriver init failed\n");
        return false;
    }

    // 2. 创建事件队列
    eventQueue_ = platform.createQueue(kEventQueueSize, sizeof(Event));
    if (!eventQueue_) {
        fprintf(stderr, "ink::App: Failed to create event queue\n");
        return false;
    }

    // 3. 创建 RenderEngine
    renderEngine_ = std::make_unique<RenderEngine>(display);

    // 4. 创建 GestureRecognizer
    gesture_ = std::make_unique<GestureRecognizer>(touch, platform, eventQueue_);

    // 5. 启动触摸任务
    if (!gesture_->start()) {
        fprintf(stderr, "ink::App: GestureRecognizer start failed, touch unavailable\n");
    }

    // 6. 构建 Window View 树
    buildWindowTree();

    // 7. 设置 NavigationController VC 切换回调
    navigator_.setOnViewControllerChange(
        [this](ViewController* vc) { mountViewController(vc); });

    fprintf(stderr, "ink::App: InkUI Application initialized\n");
    return true;
}

// ── 主事件循环 ──

void Application::run() {
    fprintf(stderr, "ink::App: Entering main event loop\n");

    // 初始渲染：确保首屏立即显示，不需等待事件
    if (windowRoot_) {
        renderEngine_->renderCycle(windowRoot_.get());
    }

    while (true) {
        Event event;
        bool received = platform_->queueReceive(
            eventQueue_, &event, kQueueTimeoutMs);

        if (received) {
            dispatchEvent(event);
        }

        // 检查时间和电池更新
        if (statusBar_ && !statusBar_->isHidden()) {
            time_t now;
            time(&now);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            if (timeinfo.tm_min != lastMinute_) {
                lastMinute_ = timeinfo.tm_min;
                statusBar_->updateTime();
            }

            // 电池电量更新（>=30 秒间隔）
            int64_t nowUs = platform_->getTimeUs();
            if (nowUs - lastBatteryUpdateUs_ >= kBatteryUpdateIntervalUs) {
                lastBatteryUpdateUs_ = nowUs;
                statusBar_->updateBattery(system_->batteryPercent());
            }
        }

        // 每轮循环末尾执行 renderCycle（以 windowRoot_ 为根）
        if (windowRoot_) {
            renderEngine_->renderCycle(windowRoot_.get());
        }
    }
}

// ── 事件分发 ──

void Application::dispatchEvent(const Event& event) {
    switch (event.type) {
        case EventType::Touch: {
            const TouchEvent& te = event.touch;
            if (te.type == TouchType::Tap || te.type == TouchType::LongPress) {
                dispatchTouch(te);
            }
            // Down/Move/Up 原始事件也通过 hitTest 分发
            if (te.type == TouchType::Down || te.type == TouchType::Move ||
                te.type == TouchType::Up) {
                dispatchTouch(te);
            }
            break;
        }
        case EventType::Swipe: {
            // Swipe 直接传给当前 VC
            auto* vc = navigator_.current();
            if (vc) {
                vc->handleEvent(event);
            }
            break;
        }
        case EventType::Timer:
        case EventType::Custom: {
            auto* vc = navigator_.current();
            if (vc) {
                vc->handleEvent(event);
            }
            break;
        }
    }
}

void Application::dispatchTouch(const TouchEvent& touch) {
    if (!windowRoot_) return;

    // hitTest 从 windowRoot_ 开始
    View* target = windowRoot_->hitTest(touch.x, touch.y);
    if (!target) return;

    // 沿 parent 链冒泡直到被消费
    View* v = target;
    while (v) {
        if (v->onTouchEvent(touch)) {
            break;  // 事件已消费
        }
        v = v->parent();
    }
}

// ── 事件投递 ──

void Application::postEvent(const Event& event) {
    if (eventQueue_ && platform_) {
        platform_->queueSend(eventQueue_, &event, 10);
    }
}

void Application::postDelayed(const Event& event, int delayMs) {
    if (!eventQueue_ || !platform_) return;

    auto* ctx = new DelayedEventCtx{platform_, eventQueue_, event};

    if (!platform_->startOneShotTimer(delayMs, delayedTimerCallback, ctx)) {
        delete ctx;
        fprintf(stderr, "ink::App: Failed to create delayed timer\n");
    }
}

} // namespace ink
