/**
 * @file GestureRecognizer.cpp
 * @brief 触摸手势识别器实现 (M5.Touch 后端)。
 *
 * 20ms 轮询 M5.Touch.getDetail() 获取触摸数据，
 * 识别 Tap / LongPress / Swipe 手势。
 */

#include "ink_ui/core/GestureRecognizer.h"

#include <M5Unified.h>

extern "C" {
#include "esp_log.h"
#include "esp_timer.h"
}

static const char* TAG = "ink::Gesture";

namespace ink {

GestureRecognizer::GestureRecognizer(QueueHandle_t eventQueue)
    : eventQueue_(eventQueue) {
}

GestureRecognizer::~GestureRecognizer() {
    stop();
}

bool GestureRecognizer::start() {
    if (running_) return true;

    // 创建触摸任务 (纯轮询，无 GPIO 中断)
    running_ = true;
    state_ = GestureState::Idle;
    BaseType_t ret = xTaskCreate(taskEntry, "ink_touch",
                                  kTaskStackSize, this,
                                  kTaskPriority, &taskHandle_);
    if (ret != pdPASS) {
        running_ = false;
        ESP_LOGE(TAG, "Failed to create touch task");
        return false;
    }

    ESP_LOGI(TAG, "Started (M5.Touch polling, %dms interval)", kPollIntervalMs);
    return true;
}

void GestureRecognizer::stop() {
    if (!running_) return;

    running_ = false;
    vTaskDelay(pdMS_TO_TICKS(100));

    taskHandle_ = nullptr;
    ESP_LOGI(TAG, "Stopped");
}

// ── 触摸任务 ──

void GestureRecognizer::taskEntry(void* arg) {
    static_cast<GestureRecognizer*>(arg)->taskLoop();
    vTaskDelete(nullptr);
}

void GestureRecognizer::taskLoop() {
    bool wasPressed = false;

    while (running_) {
        // 统一 20ms 轮询 (M5.Touch 内部已有中断优化，无触摸时开销极低)
        vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs));
        if (!running_) break;

        // 更新触摸状态并获取详情
        M5.update();
        auto detail = M5.Touch.getDetail();
        bool pressed = detail.isPressed();
        int x = detail.x;
        int y = detail.y;

        if (pressed) {
            // M5.Touch 已输出逻辑坐标 (x∈[0,539], y∈[0,959])，直接使用
            if (!wasPressed) {
                // Down 原始事件
                TouchEvent te = {TouchType::Down, x, y, x, y};
                sendEvent(Event::makeTouch(te));
                onPress(x, y);
            } else {
                // Move 原始事件
                TouchEvent te = {TouchType::Move, x, y, startX_, startY_};
                sendEvent(Event::makeTouch(te));
                onMove(x, y);
            }
            wasPressed = true;
        } else {
            if (wasPressed) {
                // Up 原始事件
                TouchEvent te = {TouchType::Up, currentX_, currentY_,
                                 startX_, startY_};
                sendEvent(Event::makeTouch(te));
                onRelease();
                wasPressed = false;
            }
        }
    }
}

int GestureRecognizer::distanceSq(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    return dx * dx + dy * dy;
}

// ── 手势状态机 (不变) ──

void GestureRecognizer::onPress(int x, int y) {
    state_ = GestureState::Tracking;
    startX_ = x;
    startY_ = y;
    currentX_ = x;
    currentY_ = y;
    startTimeUs_ = esp_timer_get_time();
}

void GestureRecognizer::onMove(int x, int y) {
    currentX_ = x;
    currentY_ = y;

    if (state_ == GestureState::Tracking) {
        // 检查长按
        int64_t elapsed = esp_timer_get_time() - startTimeUs_;
        int dSq = distanceSq(startX_, startY_, x, y);

        if (elapsed >= static_cast<int64_t>(kLongPressMs) * 1000 &&
            dSq < kLongPressDist * kLongPressDist) {
            // 触发 LongPress
            TouchEvent te = {TouchType::LongPress, startX_, startY_,
                             startX_, startY_};
            sendEvent(Event::makeTouch(te));
            state_ = GestureState::LongFired;
        }
    }
}

void GestureRecognizer::onRelease() {
    if (state_ == GestureState::LongFired) {
        state_ = GestureState::Idle;
        return;
    }

    if (state_ != GestureState::Tracking) {
        state_ = GestureState::Idle;
        return;
    }

    int64_t elapsed = esp_timer_get_time() - startTimeUs_;
    int elapsedMs = static_cast<int>(elapsed / 1000);
    int dx = currentX_ - startX_;
    int dy = currentY_ - startY_;
    int dSq = dx * dx + dy * dy;

    if (dSq >= kSwipeMinDist * kSwipeMinDist) {
        int absDx = dx < 0 ? -dx : dx;
        int absDy = dy < 0 ? -dy : dy;

        SwipeDirection dir;
        if (absDx >= absDy) {
            dir = dx < 0 ? SwipeDirection::Left : SwipeDirection::Right;
        } else {
            dir = dy < 0 ? SwipeDirection::Up : SwipeDirection::Down;
        }
        sendEvent(Event::makeSwipe(dir));
    } else if (dSq < kTapMaxDist * kTapMaxDist &&
               elapsedMs < kTapMaxTimeMs) {
        TouchEvent te = {TouchType::Tap, startX_, startY_,
                         startX_, startY_};
        sendEvent(Event::makeTouch(te));
    }

    state_ = GestureState::Idle;
}

// ── 事件发送 ──

void GestureRecognizer::sendEvent(const Event& event) {
    xQueueSend(eventQueue_, &event, pdMS_TO_TICKS(10));
}

} // namespace ink
