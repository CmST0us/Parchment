/**
 * @file Platform.h
 * @brief 操作系统抽象 HAL 接口 -- 队列、任务、定时器、时间。
 *
 * 封装 FreeRTOS / POSIX 等平台差异，ESP32 和 SDL 模拟器分别提供实现。
 */

#pragma once

#include <cstdint>

namespace ink {

/// 队列句柄（类型擦除）
using QueueHandle = void*;

/// 任务句柄（类型擦除）
using TaskHandle = void*;

/// 定时器回调函数类型
using TimerCallback = void(*)(void*);

/// 平台抽象接口
class Platform {
public:
    virtual ~Platform() = default;

    /// 创建消息队列
    virtual QueueHandle createQueue(int maxItems, int itemSize) = 0;

    /// 向队列发送数据（超时单位 ms，0 表示不等待）
    virtual bool queueSend(QueueHandle queue, const void* item, uint32_t timeout_ms) = 0;

    /// 从队列接收数据（超时单位 ms）
    virtual bool queueReceive(QueueHandle queue, void* item, uint32_t timeout_ms) = 0;

    /// 创建任务
    virtual TaskHandle createTask(void(*entry)(void*), const char* name,
                                  int stackSize, void* arg, int priority) = 0;

    /// 启动一次性定时器（延迟 ms 后调用 callback）
    virtual bool startOneShotTimer(uint32_t delayMs, TimerCallback callback, void* arg) = 0;

    /// 获取系统时间（微秒）
    virtual int64_t getTimeUs() = 0;

    /// 延迟指定毫秒
    virtual void delayMs(uint32_t ms) = 0;
};

} // namespace ink
