/**
 * @file TouchDriver.h
 * @brief 触摸输入 HAL 接口 -- 抽象触摸屏读取操作。
 *
 * 定义与具体硬件无关的触摸输入接口，ESP32 (GT911) 和 SDL 模拟器分别提供实现。
 */

#pragma once

#include <cstdint>

namespace ink {

/// 触摸点数据
struct TouchPoint {
    int x = 0;       ///< 触摸 X 坐标（逻辑坐标系）
    int y = 0;       ///< 触摸 Y 坐标（逻辑坐标系）
    bool valid = false;  ///< 是否有有效触摸
};

/// 触摸驱动抽象接口
class TouchDriver {
public:
    virtual ~TouchDriver() = default;

    /// 初始化触摸硬件
    virtual bool init() = 0;

    /// 阻塞等待触摸事件（超时返回 false）
    virtual bool waitForTouch(uint32_t timeout_ms) = 0;

    /// 读取当前触摸状态
    virtual TouchPoint read() = 0;
};

} // namespace ink
