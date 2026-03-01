/**
 * @file DesktopSystemInfo.cpp
 * @brief 桌面系统信息实现。
 */

#include "DesktopSystemInfo.h"

#include <cstdio>
#include <ctime>

void DesktopSystemInfo::getTimeString(char* buf, int bufSize) {
    time_t now = time(nullptr);
    struct tm tm;
    localtime_r(&now, &tm);
    snprintf(buf, bufSize, "%02d:%02d", tm.tm_hour, tm.tm_min);
}
