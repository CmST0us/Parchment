/**
 * @file Esp32Platform.h
 * @brief FreeRTOS 平台抽象的 ESP32 实现。
 *
 * 封装 FreeRTOS 队列、任务、延迟和 esp_timer 定时器，实现 Platform HAL 接口。
 */

#pragma once

#include "ink_ui/hal/Platform.h"

namespace ink {

/// FreeRTOS 平台的 ESP32 实现
class Esp32Platform : public Platform {
public:
    /// 创建 FreeRTOS 消息队列
    QueueHandle createQueue(int maxItems, int itemSize) override;

    /// 向队列发送数据
    bool queueSend(QueueHandle queue, const void* item, uint32_t timeout_ms) override;

    /// 从队列接收数据
    bool queueReceive(QueueHandle queue, void* item, uint32_t timeout_ms) override;

    /// 创建 FreeRTOS 任务
    TaskHandle createTask(void(*entry)(void*), const char* name,
                          int stackSize, void* arg, int priority) override;

    /// 启动 esp_timer 一次性定时器
    bool startOneShotTimer(uint32_t delayMs, TimerCallback callback, void* arg) override;

    /// 获取系统时间（微秒，基于 esp_timer）
    int64_t getTimeUs() override;

    /// FreeRTOS 延迟
    void delayMs(uint32_t ms) override;
};

} // namespace ink
