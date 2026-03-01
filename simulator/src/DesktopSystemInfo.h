/**
 * @file DesktopSystemInfo.h
 * @brief 桌面系统信息实现 -- 返回模拟电池电量和系统时间。
 */

#pragma once

#include "ink_ui/hal/SystemInfo.h"

/// 桌面系统信息实现
class DesktopSystemInfo : public ink::SystemInfo {
public:
    /// 始终返回 100%（模拟器无电池）
    int batteryPercent() override { return 100; }

    /// 返回当前系统时间，格式 "HH:MM"
    void getTimeString(char* buf, int bufSize) override;
};
