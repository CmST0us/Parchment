/**
 * @file EpdDriver.cpp
 * @brief EPD 显示驱动 C++ 封装实现。
 *
 * 委托给现有 C 函数 (epd_driver.h)。
 */

#include "ink_ui/core/EpdDriver.h"

extern "C" {
#include "epdiy.h"
#include "esp_log.h"
}

static const char* TAG = "ink::EpdDriver";

namespace ink {

EpdDriver& EpdDriver::instance() {
    static EpdDriver driver;
    return driver;
}

bool EpdDriver::init() {
    if (initialized_) {
        return true;
    }
    esp_err_t ret = epd_driver_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "epd_driver_init failed: %s", esp_err_to_name(ret));
        return false;
    }
    initialized_ = true;
    return true;
}

uint8_t* EpdDriver::framebuffer() const {
    return epd_driver_get_framebuffer();
}

bool EpdDriver::updateScreen() {
    return epd_driver_update_screen() == ESP_OK;
}

bool EpdDriver::updateArea(const Rect& physicalArea, EpdMode mode) {
    if (!initialized_) {
        return false;
    }

    int epdiyMode;
    switch (mode) {
        case EpdMode::DU:   epdiyMode = static_cast<int>(EpdMode::DU);   break;
        case EpdMode::GC16: epdiyMode = static_cast<int>(EpdMode::GC16); break;
        case EpdMode::GL16: epdiyMode = static_cast<int>(EpdMode::GL16); break;
        default:            epdiyMode = static_cast<int>(EpdMode::GL16); break;
    }

    return epd_driver_update_area_mode(physicalArea.x, physicalArea.y,
                                        physicalArea.w, physicalArea.h,
                                        epdiyMode) == ESP_OK;
}

void EpdDriver::fullClear() {
    epd_driver_clear();
}

void EpdDriver::setAllWhite() {
    epd_driver_set_all_white();
}

} // namespace ink
