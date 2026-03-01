/**
 * @file SdlTouchDriver.h
 * @brief SDL2 模拟器触摸驱动 -- 将 SDL 鼠标事件转换为触摸输入。
 *
 * SDL 事件必须从主线程轮询，但 GestureRecognizer 从后台任务调用 waitForTouch()。
 * 通过线程安全队列连接两端：主循环调用 pushMouseEvent() 注入事件，
 * 后台任务调用 waitForTouch() / read() 消费事件。
 */

#pragma once

#include "ink_ui/hal/TouchDriver.h"

#include <condition_variable>
#include <deque>
#include <mutex>

/// SDL 鼠标事件（内部传递用）
struct MouseEvent {
    int x = 0;       ///< 窗口坐标 X
    int y = 0;       ///< 窗口坐标 Y
    bool pressed = false;  ///< 鼠标左键是否按下
};

/// SDL2 触摸驱动实现
class SdlTouchDriver : public ink::TouchDriver {
public:
    /**
     * @brief 构造 SDL 触摸驱动。
     * @param scale 窗口缩放倍数（用于坐标反缩放）
     */
    explicit SdlTouchDriver(int scale = 1);

    // -- TouchDriver 接口 --
    bool init() override;
    bool waitForTouch(uint32_t timeout_ms) override;
    ink::TouchPoint read() override;

    /**
     * @brief 从 SDL 事件泵线程注入鼠标事件。
     *
     * 线程安全，可从主循环调用。
     */
    void pushMouseEvent(int x, int y, bool pressed);

private:
    int scale_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<MouseEvent> events_;
    MouseEvent lastEvent_;
};
