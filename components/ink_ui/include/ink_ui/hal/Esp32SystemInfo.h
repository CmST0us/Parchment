/**
 * @file Esp32SystemInfo.h
 * @brief ESP32 系统信息实现。
 *
 * 通过 battery ADC 读取电量，通过 POSIX time API 获取时间。
 */

#pragma once

#include "ink_ui/hal/SystemInfo.h"

namespace ink {

/// ESP32 系统信息实现
class Esp32SystemInfo : public SystemInfo {
public:
    /// 获取电池电量百分比 (0-100)
    int batteryPercent() override;

    /// 获取当前时间字符串 ("HH:MM" 格式)
    void getTimeString(char* buf, int bufSize) override;
};

} // namespace ink
