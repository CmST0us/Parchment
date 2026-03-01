/**
 * @file GestureRecognizer.cpp
 * @brief 触摸手势识别器实现。
 *
 * 基于 waitForTouch + 轮询追踪的架构，识别 Tap / LongPress / Swipe 手势。
 * 通过 HAL 接口访问触摸硬件和平台服务，不直接依赖 ESP-IDF。
 */

#include "ink_ui/core/GestureRecognizer.h"

#include <cstdio>

namespace ink {

GestureRecognizer::GestureRecognizer(TouchDriver& touch, Platform& platform,
                                     QueueHandle eventQueue)
    : touch_(touch)
    , platform_(platform)
    , eventQueue_(eventQueue) {
}

GestureRecognizer::~GestureRecognizer() {
    stop();
}

bool GestureRecognizer::start() {
    if (running_) return true;

    // 创建触摸任务
    running_ = true;
    state_ = GestureState::Idle;
    taskHandle_ = platform_.createTask(taskEntry, "ink_touch",
                                        kTaskStackSize, this,
                                        kTaskPriority);
    if (!taskHandle_) {
        running_ = false;
        fprintf(stderr, "GestureRecognizer: Failed to create touch task\n");
        return false;
    }

    return true;
}

void GestureRecognizer::stop() {
    if (!running_) return;
    running_ = false;
    // 等待任务退出（touch_.waitForTouch 会超时返回）
    platform_.delayMs(100);
    taskHandle_ = nullptr;
}

// ── 触摸任务 ──

void GestureRecognizer::taskEntry(void* arg) {
    static_cast<GestureRecognizer*>(arg)->taskLoop();
}

void GestureRecognizer::taskLoop() {
    bool wasPressed = false;

    while (running_) {
        if (state_ == GestureState::Idle) {
            // 空闲：阻塞等待触摸事件
            touch_.waitForTouch(kIdleTimeoutMs);
        } else {
            // 追踪中：20ms 轮询
            platform_.delayMs(kPollIntervalMs);
        }

        if (!running_) break;

        // 读取触摸数据
        TouchPoint tp = touch_.read();

        if (tp.valid) {
            int sx = tp.x;
            int sy = tp.y;
            clampCoords(sx, sy);

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

// ── 坐标裁剪 ──

void GestureRecognizer::clampCoords(int& sx, int& sy) {
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
    startTimeUs_ = platform_.getTimeUs();
}

void GestureRecognizer::onMove(int x, int y) {
    currentX_ = x;
    currentY_ = y;

    if (state_ == GestureState::Tracking) {
        // 检查长按
        int64_t elapsed = platform_.getTimeUs() - startTimeUs_;
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

    int64_t elapsed = platform_.getTimeUs() - startTimeUs_;
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
    platform_.queueSend(eventQueue_, &event, 10);
}

} // namespace ink
