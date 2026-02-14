/**
 * @file GestureRecognizer.cpp
 * @brief 触摸手势识别器实现。
 *
 * 基于中断唤醒 + 轮询追踪的架构，识别 Tap / LongPress / Swipe 手势。
 */

#include "ink_ui/core/GestureRecognizer.h"

extern "C" {
#include "gt911.h"
#include "driver/gpio.h"
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

    // 创建信号量
    if (!touchSem_) {
        touchSem_ = xSemaphoreCreateBinary();
        if (!touchSem_) {
            ESP_LOGE(TAG, "Failed to create semaphore");
            return false;
        }
    }

    // 配置 GT911 INT 引脚下降沿中断
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << kIntGpio);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    gpio_config(&io_conf);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(static_cast<gpio_num_t>(kIntGpio), isrHandler, this);

    // 创建触摸任务
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

    ESP_LOGI(TAG, "Started (INT=GPIO%d)", kIntGpio);
    return true;
}

void GestureRecognizer::stop() {
    if (!running_) return;

    running_ = false;

    // 释放信号量使任务退出等待
    if (touchSem_) {
        xSemaphoreGive(touchSem_);
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    // 禁用中断
    gpio_isr_handler_remove(static_cast<gpio_num_t>(kIntGpio));
    gpio_intr_disable(static_cast<gpio_num_t>(kIntGpio));

    taskHandle_ = nullptr;
    ESP_LOGI(TAG, "Stopped");
}

// ── 中断处理 ──

void IRAM_ATTR GestureRecognizer::isrHandler(void* arg) {
    auto* self = static_cast<GestureRecognizer*>(arg);
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(self->touchSem_, &woken);
    if (woken) {
        portYIELD_FROM_ISR();
    }
}

// ── 触摸任务 ──

void GestureRecognizer::taskEntry(void* arg) {
    static_cast<GestureRecognizer*>(arg)->taskLoop();
    vTaskDelete(nullptr);
}

void GestureRecognizer::taskLoop() {
    gt911_touch_point_t point;
    bool wasPressed = false;

    while (running_) {
        if (state_ == GestureState::Idle) {
            // 空闲：阻塞等待 INT 中断信号量
            xSemaphoreTake(touchSem_, pdMS_TO_TICKS(1000));
        } else {
            // 追踪中：20ms 轮询
            vTaskDelay(pdMS_TO_TICKS(kPollIntervalMs));
        }

        if (!running_) break;

        // 读取触摸数据
        if (gt911_read(&point) != ESP_OK) continue;

        if (point.pressed) {
            int sx, sy;
            mapCoords(point.x, point.y, sx, sy);

            if (!wasPressed) {
                // Down 原始事件
                TouchEvent te = {TouchType::Down, sx, sy, sx, sy};
                sendEvent(Event::makeTouch(te));
                onPress(sx, sy);
            } else {
                // Move 原始事件
                TouchEvent te = {TouchType::Move, sx, sy, startX_, startY_};
                sendEvent(Event::makeTouch(te));
                onMove(sx, sy);
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

// ── 坐标映射 ──

void GestureRecognizer::mapCoords(uint16_t tx, uint16_t ty, int& sx, int& sy) {
    // GT911 直通，无需翻转
    sx = static_cast<int>(tx);
    sy = static_cast<int>(ty);

    // 裁剪到有效范围
    if (sx < 0) sx = 0;
    if (sx >= 540) sx = 539;
    if (sy < 0) sy = 0;
    if (sy >= 960) sy = 959;
}

int GestureRecognizer::distanceSq(int x0, int y0, int x1, int y1) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    return dx * dx + dy * dy;
}

// ── 手势状态机 ──

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
        // 长按已触发，释放后不产生其他手势
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
        // 滑动：根据主方向判断
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
        // 点击
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
