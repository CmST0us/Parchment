/**
 * @file SdlTouchDriver.cpp
 * @brief SDL2 触摸驱动实现。
 *
 * 鼠标窗口坐标除以 scale 转换为逻辑触摸坐标。
 */

#include "SdlTouchDriver.h"

#include <chrono>

SdlTouchDriver::SdlTouchDriver(int scale)
    : scale_(scale), lastEvent_{0, 0, false} {}

bool SdlTouchDriver::init() {
    return true;
}

void SdlTouchDriver::pushMouseEvent(int x, int y, bool pressed) {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.push_back({x, y, pressed});
    cv_.notify_one();
}

bool SdlTouchDriver::waitForTouch(uint32_t timeout_ms) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!events_.empty()) {
        return true;
    }
    return cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                        [this] { return !events_.empty(); });
}

ink::TouchPoint SdlTouchDriver::read() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!events_.empty()) {
        lastEvent_ = events_.front();
        events_.pop_front();
    }
    return ink::TouchPoint{
        lastEvent_.x / scale_,
        lastEvent_.y / scale_,
        lastEvent_.pressed
    };
}
