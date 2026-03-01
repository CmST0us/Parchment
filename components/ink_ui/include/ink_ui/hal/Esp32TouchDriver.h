/**
 * @file Esp32TouchDriver.h
 * @brief GT911 触摸驱动的 ESP32 实现。
 *
 * 封装 GT911 I2C 触摸控制器和 GPIO 中断，实现 TouchDriver HAL 接口。
 * 使用 FreeRTOS 二值信号量等待触摸中断。
 */

#pragma once

#include "ink_ui/hal/TouchDriver.h"

extern "C" {
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
}

namespace ink {

/// GT911 触摸驱动的 ESP32 实现
class Esp32TouchDriver : public TouchDriver {
public:
    /// 初始化 GPIO 中断和信号量（GT911 I2C 须已由 main 初始化）
    bool init() override;

    /// 阻塞等待触摸中断（超时返回 false）
    bool waitForTouch(uint32_t timeout_ms) override;

    /// 读取当前触摸状态
    TouchPoint read() override;

private:
    SemaphoreHandle_t touchSem_ = nullptr;

    /// GPIO 中断服务例程（IRAM 常驻）
    static void IRAM_ATTR isrHandler(void* arg);
};

} // namespace ink
