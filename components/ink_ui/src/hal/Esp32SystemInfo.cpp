/**
 * @file Esp32SystemInfo.cpp
 * @brief ESP32 系统信息实现。
 */

#include "ink_ui/hal/Esp32SystemInfo.h"

#include <cstdio>
#include <ctime>

extern "C" {
#include "battery.h"
}

namespace ink {

int Esp32SystemInfo::batteryPercent() {
    return battery_get_percent();
}

void Esp32SystemInfo::getTimeString(char* buf, int bufSize) {
    time_t now;
    time(&now);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    snprintf(buf, bufSize, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
}

} // namespace ink
