/**
 * @file Application.cpp
 * @brief InkUI 应用主循环实现 (M5GFX 后端)。
 */

#include "ink_ui/core/Application.h"
#include "ink_ui/core/Canvas.h"
#include "ink_ui/views/StatusBarView.h"

#include <M5Unified.h>

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "font_engine/font_engine.h"
}

#include <ctime>

static const char* TAG = "ink::App";

namespace ink {

// ── esp_timer 回调：延迟投递事件 ──

struct DelayedEventCtx {
    QueueHandle_t queue;
    Event event;
};

static void delayedTimerCallback(void* arg) {
    auto* ctx = static_cast<DelayedEventCtx*>(arg);
    xQueueSend(ctx->queue, &ctx->event, 0);
    delete ctx;
}

// ── Window View 树构建 ──

void Application::buildWindowTree() {
    // windowRoot_: 全屏 Column 容器
    windowRoot_ = std::make_unique<View>();
    windowRoot_->setFrame({0, 0, M5.Display.width(), M5.Display.height()});
    windowRoot_->setBackgroundColor(Color::White);
    windowRoot_->flexStyle_.direction = FlexDirection::Column;
    windowRoot_->flexStyle_.alignItems = Align::Stretch;

    // statusBar_: 20px 高，由 Application 管理
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

    ESP_LOGI(TAG, "Window tree built: statusBar(20px) + contentArea");
}

// ── VC View 挂载 ──

void Application::mountViewController(ViewController* vc) {
    if (!contentArea_) return;

    // 1. 归还旧 VC 的 View 所有权
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

    ESP_LOGI(TAG, "Mounted VC: '%s', statusBar=%s",
             vc->title_.c_str(),
             (statusBar_ && !statusBar_->isHidden()) ? "visible" : "hidden");
}

// ── 初始化 ──

bool Application::init() {
    ESP_LOGI(TAG, "Initializing InkUI Application...");

    // 1. M5Unified 初始化 (显示 + 触摸 + 电源)
    auto cfg = M5.config();
    cfg.clear_display = true;
    M5.begin(cfg);
    M5.Display.setAutoDisplay(false);
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    ESP_LOGI(TAG, "M5.begin() done, display=%dx%d",
             M5.Display.width(), M5.Display.height());

    // 2. 字体引擎初始化
    static font_engine_t engine;
    if (font_engine_load(&engine, "/littlefs/reading_font.bin")) {
        font_cache_init_common(&engine);
        font_cache_init_recycle_pool(&engine);
        fontEngine_ = &engine;
        ESP_LOGI(TAG, "Font engine loaded: %s", engine.header.family_name);
    } else {
        ESP_LOGW(TAG, "Font engine load failed, text rendering unavailable");
    }

    // 3. 创建事件队列
    eventQueue_ = xQueueCreate(kEventQueueSize, sizeof(Event));
    if (!eventQueue_) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return false;
    }

    // 4. 创建 RenderEngine (绑定 &M5.Display)
    renderEngine_ = std::make_unique<RenderEngine>(&M5.Display);

    // 5. 创建 GestureRecognizer
    gesture_ = std::make_unique<GestureRecognizer>(eventQueue_);

    // 6. 启动触摸任务
    if (!gesture_->start()) {
        ESP_LOGW(TAG, "GestureRecognizer start failed, touch unavailable");
    }

    // 7. 构建 Window View 树
    buildWindowTree();

    // 8. 设置 NavigationController VC 切换回调
    navigator_.setOnViewControllerChange(
        [this](ViewController* vc) { mountViewController(vc); });

    ESP_LOGI(TAG, "InkUI Application initialized");
    return true;
}

// ── 主事件循环 ──

void Application::run() {
    ESP_LOGI(TAG, "Entering main event loop");

    while (true) {
        Event event;
        BaseType_t received = xQueueReceive(
            eventQueue_, &event,
            pdMS_TO_TICKS(kQueueTimeoutMs));

        if (received == pdTRUE) {
            dispatchEvent(event);
        }

        // 检查时间更新（分钟级粒度）
        if (statusBar_ && !statusBar_->isHidden()) {
            time_t now;
            time(&now);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);
            if (timeinfo.tm_min != lastMinute_) {
                lastMinute_ = timeinfo.tm_min;
                statusBar_->updateTime();
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
            if (te.type == TouchType::Down || te.type == TouchType::Move ||
                te.type == TouchType::Up) {
                dispatchTouch(te);
            }
            break;
        }
        case EventType::Swipe: {
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

    View* target = windowRoot_->hitTest(touch.x, touch.y);
    if (!target) return;

    View* v = target;
    while (v) {
        if (v->onTouchEvent(touch)) {
            break;
        }
        v = v->parent();
    }
}

// ── 事件投递 ──

void Application::postEvent(const Event& event) {
    if (eventQueue_) {
        xQueueSend(eventQueue_, &event, pdMS_TO_TICKS(10));
    }
}

void Application::postDelayed(const Event& event, int delayMs) {
    if (!eventQueue_) return;

    auto* ctx = new DelayedEventCtx{eventQueue_, event};

    esp_timer_create_args_t args = {};
    args.callback = delayedTimerCallback;
    args.arg = ctx;
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = "ink_delayed";

    esp_timer_handle_t timer;
    if (esp_timer_create(&args, &timer) == ESP_OK) {
        esp_timer_start_once(timer, static_cast<uint64_t>(delayMs) * 1000);
    } else {
        delete ctx;
        ESP_LOGE(TAG, "Failed to create delayed timer");
    }
}

} // namespace ink
