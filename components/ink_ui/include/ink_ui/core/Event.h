/**
 * @file Event.h
 * @brief 统一事件类型定义: TouchEvent, SwipeEvent, TimerEvent, Event。
 *
 * 使用 tagged union 模式，固定大小，可通过 FreeRTOS 队列按值传递。
 */

#pragma once

#include <cstdint>

namespace ink {

/// 触摸事件类型
enum class TouchType {
    Down,
    Move,
    Up,
    Tap,
    LongPress,
};

/// 触摸事件数据
struct TouchEvent {
    TouchType type;
    int x;          ///< 当前触摸坐标（逻辑坐标系）
    int y;
    int startX;     ///< 触摸起始坐标
    int startY;
};

/// 滑动方向
enum class SwipeDirection {
    Left,
    Right,
    Up,
    Down,
};

/// 滑动事件数据
struct SwipeEvent {
    SwipeDirection direction;
};

/// 定时器事件数据
struct TimerEvent {
    int timerId;
};

/// 事件大类型
enum class EventType {
    Touch,
    Swipe,
    Timer,
    Custom,
};

/// 统一事件（tagged union）
struct Event {
    EventType type;
    union {
        TouchEvent touch;
        SwipeEvent swipe;
        TimerEvent timer;
        int32_t customParam;
    };

    /// 创建触摸事件
    static Event makeTouch(const TouchEvent& te) {
        Event e;
        e.type = EventType::Touch;
        e.touch = te;
        return e;
    }

    /// 创建滑动事件
    static Event makeSwipe(SwipeDirection dir) {
        Event e;
        e.type = EventType::Swipe;
        e.swipe = {dir};
        return e;
    }

    /// 创建定时器事件
    static Event makeTimer(int timerId) {
        Event e;
        e.type = EventType::Timer;
        e.timer = {timerId};
        return e;
    }

    /// 创建自定义事件
    static Event makeCustom(int32_t param) {
        Event e;
        e.type = EventType::Custom;
        e.customParam = param;
        return e;
    }
};

} // namespace ink
