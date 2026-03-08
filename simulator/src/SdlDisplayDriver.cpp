/**
 * @file SdlDisplayDriver.cpp
 * @brief SDL2 显示驱动实现。
 *
 * 4bpp framebuffer 像素格式（与 Canvas::setPixel 一致）:
 *   - 偶数 px: 低 nibble 存储灰度值 (gray >> 4)
 *   - 奇数 px: 高 nibble 存储灰度值 (gray & 0xF0)
 *
 * 坐标变换（Canvas::setPixel 的逆变换）:
 *   - 物理 (px, py) -> 逻辑 (lx, ly): lx = 539 - py, ly = px
 */

#include "SdlDisplayDriver.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

SdlDisplayDriver::SdlDisplayDriver(int scale)
    : scale_(scale) {}

SdlDisplayDriver::~SdlDisplayDriver() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    free(pixelBuf_);
    free(fb_);
}

bool SdlDisplayDriver::init() {
    // 分配 4bpp 物理帧缓冲区
    fb_ = static_cast<uint8_t*>(calloc(kFbSize, 1));
    if (!fb_) return false;

    // 分配 ARGB 逻辑像素缓冲区 (540 x 960)
    pixelBuf_ = static_cast<uint32_t*>(
        calloc(kLogW * kLogH, sizeof(uint32_t)));
    if (!pixelBuf_) return false;

    // 创建 SDL 窗口（逻辑尺寸 * 缩放）
    window_ = SDL_CreateWindow(
        "Parchment Simulator",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        kLogW * scale_, kLogH * scale_,
        SDL_WINDOW_SHOWN);
    if (!window_) return false;

    // 创建渲染器
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer_) return false;

    // 创建流式纹理（逻辑尺寸 540x960）
    texture_ = SDL_CreateTexture(
        renderer_,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        kLogW, kLogH);
    if (!texture_) return false;

    // 初始化为白色
    memset(fb_, 0xFF, kFbSize);
    blitAreaToTexture(0, 0, kPhysW, kPhysH);
    present();

    return true;
}

uint8_t* SdlDisplayDriver::framebuffer() {
    return fb_;
}

bool SdlDisplayDriver::updateArea(int x, int y, int w, int h,
                                   ink::RefreshMode /*mode*/) {
    {
        std::lock_guard<std::mutex> lock(blitMutex_);
        blitAreaToTexture(x, y, w, h);
    }
    needsPresent_.store(true, std::memory_order_release);
    return true;
}

bool SdlDisplayDriver::updateScreen() {
    {
        std::lock_guard<std::mutex> lock(blitMutex_);
        blitAreaToTexture(0, 0, kPhysW, kPhysH);
    }
    needsPresent_.store(true, std::memory_order_release);
    return true;
}

void SdlDisplayDriver::fullClear() {
    memset(fb_, 0xFF, kFbSize);
    {
        std::lock_guard<std::mutex> lock(blitMutex_);
        blitAreaToTexture(0, 0, kPhysW, kPhysH);
    }
    needsPresent_.store(true, std::memory_order_release);
}

void SdlDisplayDriver::setAllWhite() {
    memset(fb_, 0xFF, kFbSize);
}

void SdlDisplayDriver::setAllBlack() {
    memset(fb_, 0x00, kFbSize);
}

int SdlDisplayDriver::width() const {
    return kPhysW;
}

int SdlDisplayDriver::height() const {
    return kPhysH;
}

void SdlDisplayDriver::blitAreaToTexture(int physX, int physY,
                                          int physW, int physH) {
    // 遍历物理区域中的每个像素，转换到逻辑坐标写入 pixelBuf_
    for (int py = physY; py < physY + physH && py < kPhysH; py++) {
        for (int px = physX; px < physX + physW && px < kPhysW; px++) {
            // 从 4bpp framebuffer 提取灰度值
            int idx = py * (kPhysW / 2) + px / 2;
            uint8_t byte_val = fb_[idx];
            uint8_t gray4;
            if (px & 1) {
                // 奇数像素: 高 nibble
                gray4 = (byte_val >> 4) & 0x0F;
            } else {
                // 偶数像素: 低 nibble
                gray4 = byte_val & 0x0F;
            }
            // 4bit -> 8bit: 0x0 -> 0x00, 0xF -> 0xFF
            uint8_t gray8 = static_cast<uint8_t>(gray4 * 17);

            // 物理 -> 逻辑坐标变换（Canvas::setPixel 的逆变换）
            // Canvas: px = absY, py = 539 - absX
            // 逆: absX = 539 - py, absY = px => lx = 539 - py, ly = px
            int lx = (kPhysH - 1) - py;  // 539 - py
            int ly = px;

            pixelBuf_[ly * kLogW + lx] =
                0xFF000000u | (gray8 << 16) | (gray8 << 8) | gray8;
        }
    }
}

void SdlDisplayDriver::present() {
    SDL_UpdateTexture(texture_, nullptr, pixelBuf_,
                      kLogW * sizeof(uint32_t));
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

void SdlDisplayDriver::presentIfNeeded() {
    if (needsPresent_.exchange(false, std::memory_order_acq_rel)) {
        std::lock_guard<std::mutex> lock(blitMutex_);
        present();
    }
}
