/**
 * @file EpdDriver.h
 * @brief EPD 显示驱动 C++ 封装。
 *
 * 对现有 C 实现 (epd_driver.c) 的薄 wrapper，提供类型安全的 C++ 接口。
 */

#pragma once

extern "C" {
#include "epd_driver.h"
}

#include "ink_ui/core/Geometry.h"

namespace ink {

/// 墨水屏刷新模式
enum class EpdMode {
    DU   = 0x1,   ///< 快速单色直接更新 (~200ms, 不闪)
    GC16 = 0x2,   ///< 灰度全清 (~3s, 闪黑, 消残影)
    GL16 = 0x5,   ///< 灰度更新 (~1.5s, 不闪)
};

/// 屏幕常量
constexpr int kScreenWidth  = 540;   ///< 逻辑宽度 (portrait)
constexpr int kScreenHeight = 960;   ///< 逻辑高度 (portrait)
constexpr int kFbPhysWidth  = 960;   ///< 物理 framebuffer 宽度 (landscape)
constexpr int kFbPhysHeight = 540;   ///< 物理 framebuffer 高度 (landscape)

/// EPD 显示驱动封装
class EpdDriver {
public:
    /// 获取单例
    static EpdDriver& instance();

    /// 初始化 EPD 硬件 (含全屏清除)
    bool init();

    /// 获取帧缓冲区指针 (4bpp, 物理布局 960x540)
    uint8_t* framebuffer() const;

    /// 全屏刷新 (MODE_GL16, 不闪)
    bool updateScreen();

    /// 局部刷新 (指定物理坐标区域和模式)
    bool updateArea(const Rect& physicalArea, EpdMode mode);

    /// 全屏闪黑清除 (MODE_GC16, 消残影)
    void fullClear();

    /// 将帧缓冲区全部设为白色 (不刷新屏幕)
    void setAllWhite();

    /// 是否已初始化
    bool isInitialized() const { return initialized_; }

private:
    EpdDriver() = default;
    bool initialized_ = false;
};

} // namespace ink
