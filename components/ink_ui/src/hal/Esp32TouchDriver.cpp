/**
 * @file Esp32TouchDriver.cpp
 * @brief GT911 触摸驱动的 ESP32 实现。
 */

#include "ink_ui/hal/Esp32TouchDriver.h"

#include <cstdio>

extern "C" {
#include "driver/gpio.h"
#include "gt911.h"
}

/// GT911 中断引脚 (M5PaperS3 板级定义)
static constexpr gpio_num_t kTouchIntGpio = GPIO_NUM_48;

namespace ink {

void IRAM_ATTR Esp32TouchDriver::isrHandler(void* arg) {
    auto* sem = static_cast<SemaphoreHandle_t>(arg);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sem, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

bool Esp32TouchDriver::init() {
    // 创建二值信号量
    touchSem_ = xSemaphoreCreateBinary();
    if (!touchSem_) {
        fprintf(stderr, "Esp32TouchDriver: Failed to create semaphore\n");
        return false;
    }

    // 配置 GPIO 48 为输入，下降沿中断
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << kTouchIntGpio);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    gpio_config(&io_conf);

    // 安装 GPIO ISR 服务（如果尚未安装，忽略已安装错误）
    gpio_install_isr_service(0);
    gpio_isr_handler_add(kTouchIntGpio, isrHandler, touchSem_);

    fprintf(stderr, "Esp32TouchDriver: Initialized (GPIO %d, falling edge)\n",
            kTouchIntGpio);
    return true;
}

bool Esp32TouchDriver::waitForTouch(uint32_t timeout_ms) {
    if (!touchSem_) return false;
    return xSemaphoreTake(touchSem_, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

TouchPoint Esp32TouchDriver::read() {
    gt911_touch_point_t point = {};
    esp_err_t err = gt911_read(&point);
    if (err != ESP_OK) {
        return TouchPoint{0, 0, false};
    }
    return TouchPoint{static_cast<int>(point.x),
                      static_cast<int>(point.y),
                      point.pressed};
}

} // namespace ink
