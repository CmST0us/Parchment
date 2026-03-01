/**
 * @file DesktopPlatform.h
 * @brief 桌面平台 HAL 实现 -- 使用 std::thread / std::mutex 替代 FreeRTOS。
 *
 * 提供队列、任务、定时器等操作系统原语的 POSIX/C++17 实现。
 */

#pragma once

#include "ink_ui/hal/Platform.h"

/// 桌面平台实现
class DesktopPlatform : public ink::Platform {
public:
    // -- Platform 接口 --
    ink::QueueHandle createQueue(int maxItems, int itemSize) override;
    bool queueSend(ink::QueueHandle queue, const void* item,
                   uint32_t timeout_ms) override;
    bool queueReceive(ink::QueueHandle queue, void* item,
                      uint32_t timeout_ms) override;
    ink::TaskHandle createTask(void(*entry)(void*), const char* name,
                               int stackSize, void* arg, int priority) override;
    bool startOneShotTimer(uint32_t delayMs, ink::TimerCallback callback,
                           void* arg) override;
    int64_t getTimeUs() override;
    void delayMs(uint32_t ms) override;
};
