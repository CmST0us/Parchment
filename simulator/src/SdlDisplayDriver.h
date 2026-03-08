/**
 * @file SdlDisplayDriver.h
 * @brief SDL2 模拟器显示驱动 -- 将 4bpp framebuffer 渲染到 SDL 窗口。
 *
 * 将物理布局 (960x540 landscape) 的 4bpp 灰度帧缓冲区转换为
 * 逻辑布局 (540x960 portrait) 的 ARGB 像素，通过 SDL2 窗口显示。
 */

#pragma once

#include "ink_ui/hal/DisplayDriver.h"
#include <SDL.h>
#include <cstdint>
#include <atomic>
#include <mutex>

/// SDL2 显示驱动实现
class SdlDisplayDriver : public ink::DisplayDriver {
public:
    /**
     * @brief 构造 SDL 显示驱动。
     * @param scale 窗口缩放倍数（默认 1）
     */
    explicit SdlDisplayDriver(int scale = 1);
    ~SdlDisplayDriver() override;

    // -- DisplayDriver 接口 --
    bool init() override;
    uint8_t* framebuffer() override;
    bool updateArea(int x, int y, int w, int h, ink::RefreshMode mode) override;
    bool updateScreen() override;
    void fullClear() override;
    void setAllWhite() override;
    void setAllBlack() override;
    int width() const override;
    int height() const override;

private:
    static constexpr int kPhysW = 960;
    static constexpr int kPhysH = 540;
    static constexpr int kLogW = 540;
    static constexpr int kLogH = 960;
    static constexpr int kFbSize = kPhysW / 2 * kPhysH;  // 259200 bytes

    int scale_;
    uint8_t* fb_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    uint32_t* pixelBuf_ = nullptr;  ///< 540x960 ARGB 像素缓冲区

    std::atomic<bool> needsPresent_{false};
    std::mutex blitMutex_;  ///< 保护 pixelBuf_ 的并发读写

    /**
     * @brief 将物理区域的 4bpp 数据转换到 ARGB 像素缓冲区。
     *
     * 包含坐标变换：物理 (px, py) -> 逻辑 (lx = 539 - py, ly = px)
     */
    void blitAreaToTexture(int physX, int physY, int physW, int physH);

    /// 将像素缓冲区提交到 SDL 纹理并呈现（必须在 SDL 主线程调用）
    void present();

public:
    /**
     * @brief 由主线程 SDL 事件循环调用，检查是否需要刷新并提交到屏幕。
     *
     * SDL 渲染 API 必须在创建 renderer 的线程（主线程）调用。
     * 后台线程的 updateArea/updateScreen 只更新 pixelBuf_，
     * 由主线程在事件循环中调用此方法完成实际 SDL 呈现。
     */
    void presentIfNeeded();
};
