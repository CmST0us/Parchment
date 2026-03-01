/**
 * @file DisplayDriver.h
 * @brief 显示驱动 HAL 接口 -- 抽象墨水屏操作。
 *
 * 定义与具体硬件无关的显示驱动接口，ESP32 和 SDL 模拟器分别提供实现。
 */

#pragma once

#include <cstdint>

namespace ink {

/// 屏幕逻辑 / 物理尺寸常量
constexpr int kScreenWidth  = 540;   ///< 逻辑宽度 (portrait)
constexpr int kScreenHeight = 960;   ///< 逻辑高度 (portrait)
constexpr int kFbPhysWidth  = 960;   ///< 物理 framebuffer 宽度 (landscape)
constexpr int kFbPhysHeight = 540;   ///< 物理 framebuffer 高度 (landscape)

/// 刷新模式
enum class RefreshMode {
    Full,   ///< 灰度准确刷新，不闪黑 (对应 MODE_GL16)
    Fast,   ///< 快速单色刷新 (对应 MODE_DU)
    Clear,  ///< 闪黑全清，消残影 (对应 MODE_GC16)
};

/// 显示驱动抽象接口
class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;

    /// 初始化显示硬件
    virtual bool init() = 0;

    /// 获取帧缓冲区指针 (4bpp, 物理布局)
    virtual uint8_t* framebuffer() = 0;

    /// 局部区域刷新（物理坐标）
    virtual bool updateArea(int x, int y, int w, int h, RefreshMode mode) = 0;

    /// 全屏刷新
    virtual bool updateScreen() = 0;

    /// 全屏闪黑清除（消残影）
    virtual void fullClear() = 0;

    /// 将帧缓冲区全部设为白色（不刷新屏幕）
    virtual void setAllWhite() = 0;

    /// 物理 framebuffer 宽度
    virtual int width() const = 0;

    /// 物理 framebuffer 高度
    virtual int height() const = 0;
};

} // namespace ink
