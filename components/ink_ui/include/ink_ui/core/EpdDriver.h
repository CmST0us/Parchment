/**
 * @file EpdDriver.h
 * @brief EPD 显示驱动 C++ 封装 -- DisplayDriver 的 ESP32 实现。
 *
 * 对现有 C 实现 (epd_driver.c) 的薄 wrapper，通过 DisplayDriver HAL 接口
 * 提供给 InkUI 框架使用。
 */

#pragma once

extern "C" {
#include "epd_driver.h"
}

#include "ink_ui/core/Geometry.h"
#include "ink_ui/hal/DisplayDriver.h"

namespace ink {

/// 墨水屏刷新模式（ESP32 epdiy 专用值）
enum class EpdMode {
    DU   = 0x1,   ///< 快速单色直接更新 (~200ms, 不闪)
    GC16 = 0x2,   ///< 灰度全清 (~3s, 闪黑, 消残影)
    GL16 = 0x5,   ///< 灰度更新 (~1.5s, 不闪)
};

/// EPD 显示驱动封装（实现 DisplayDriver HAL 接口）
class EpdDriver : public DisplayDriver {
public:
    /// 获取单例
    static EpdDriver& instance();

    // ── DisplayDriver 接口实现 ──

    /// 初始化 EPD 硬件 (含全屏清除)
    bool init() override;

    /// 获取帧缓冲区指针 (4bpp, 物理布局 960x540)
    uint8_t* framebuffer() override;

    /// 局部刷新（物理坐标，RefreshMode 映射到 EpdMode）
    bool updateArea(int x, int y, int w, int h, RefreshMode mode) override;

    /// 全屏刷新 (MODE_GL16, 不闪)
    bool updateScreen() override;

    /// 全屏闪黑清除 (MODE_GC16, 消残影)
    void fullClear() override;

    /// 将帧缓冲区全部设为白色 (不刷新屏幕)
    void setAllWhite() override;

    /// 将帧缓冲区全部设为黑色 (不刷新屏幕)
    void setAllBlack() override;

    /// 物理 framebuffer 宽度
    int width() const override { return kFbPhysWidth; }

    /// 物理 framebuffer 高度
    int height() const override { return kFbPhysHeight; }

    // ── ESP32 专用接口 ──

    /// 局部刷新（Rect + EpdMode，保留向后兼容）
    bool updateArea(const Rect& physicalArea, EpdMode mode);

    /// 是否已初始化
    bool isInitialized() const { return initialized_; }

private:
    EpdDriver() = default;
    bool initialized_ = false;

    /// RefreshMode → EpdMode 映射
    static EpdMode toEpdMode(RefreshMode mode);
};

} // namespace ink
