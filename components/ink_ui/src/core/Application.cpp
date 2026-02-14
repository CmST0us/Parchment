/**
 * @file Application.cpp
 * @brief InkUI 应用主循环实现。
 */

#include "ink_ui/core/Application.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_timer.h"
}

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

// ── 初始化 ──

bool Application::init() {
    ESP_LOGI(TAG, "Initializing InkUI Application...");

    // 1. EpdDriver 初始化
    auto& epd = EpdDriver::instance();
    if (!epd.init()) {
        ESP_LOGE(TAG, "EpdDriver init failed");
        return false;
    }

    // 2. 创建事件队列
    eventQueue_ = xQueueCreate(kEventQueueSize, sizeof(Event));
    if (!eventQueue_) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return false;
    }

    // 3. 创建 RenderEngine
    renderEngine_ = std::make_unique<RenderEngine>(epd);

    // 4. 创建 GestureRecognizer
    gesture_ = std::make_unique<GestureRecognizer>(eventQueue_);

    // 5. 启动触摸任务
    if (!gesture_->start()) {
        ESP_LOGW(TAG, "GestureRecognizer start failed, touch unavailable");
    }

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

        // 每轮循环末尾执行 renderCycle
        auto* vc = navigator_.current();
        if (vc) {
            View* rootView = vc->getView();
            if (rootView) {
                renderEngine_->renderCycle(rootView);
            }
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
    auto* vc = navigator_.current();
    if (!vc) return;

    View* rootView = vc->getView();
    if (!rootView) return;

    // hitTest 找到目标 View
    View* target = rootView->hitTest(touch.x, touch.y);
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
