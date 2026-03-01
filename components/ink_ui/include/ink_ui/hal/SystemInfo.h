/**
 * @file SystemInfo.h
 * @brief 系统信息 HAL 接口 -- 电池、时间等系统状态查询。
 *
 * 抽象平台相关的系统信息获取，ESP32 读取真实硬件，模拟器返回模拟值。
 */

#pragma once

namespace ink {

/// 系统信息抽象接口
class SystemInfo {
public:
    virtual ~SystemInfo() = default;

    /// 获取电池电量百分比 (0-100)
    virtual int batteryPercent() = 0;

    /// 获取当前时间字符串（格式由实现决定，如 "HH:MM"）
    virtual void getTimeString(char* buf, int bufSize) = 0;
};

} // namespace ink
