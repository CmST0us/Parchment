/**
 * @file Canvas.h
 * @brief 带裁剪区域的绘图引擎 (M5GFX 后端)。
 *
 * Canvas 封装 M5GFX 绘图操作，提供局部坐标系和自动裁剪。
 * 所有绘图坐标相对于裁剪区域左上角，超出裁剪区域的像素被自动丢弃。
 * RenderEngine 为每个需要重绘的 View 创建 Canvas 实例。
 */

#pragma once

#include <cstdint>

#include "ink_ui/core/Geometry.h"

// 前向声明 M5GFX
namespace m5gfx { class M5GFX; }
using M5GFX = m5gfx::M5GFX;

// 前向声明字体引擎
extern "C" {
struct font_engine_t;
}

namespace ink {

/// 8bpp 灰度调色板常量 (0x00=黑, 0xFF=白)
namespace Color {
constexpr uint8_t Black  = 0x00;
constexpr uint8_t Dark   = 0x44;
constexpr uint8_t Medium = 0x88;
constexpr uint8_t Light  = 0xCC;
constexpr uint8_t White  = 0xFF;
constexpr uint8_t Clear  = 0x01;  ///< 哨兵值：透明，不绘制背景
} // namespace Color

/// 带裁剪区域的绘图引擎 (M5GFX 后端)
class Canvas {
public:
    /**
     * @brief 构造 Canvas。
     * @param display M5GFX 显示设备指针。
     * @param clip    裁剪区域（屏幕绝对逻辑坐标，540x960 portrait）。
     */
    Canvas(M5GFX* display, Rect clip);

    /// 析构 — 恢复 M5GFX 裁剪区域
    ~Canvas();

    /**
     * @brief 创建子 Canvas，裁剪区域为当前 clip 与 subRect 的交集。
     * @param subRect 局部坐标系下的子区域。
     * @return 新的 Canvas，clip 为交集区域。
     */
    Canvas clipped(const Rect& subRect) const;

    // ── 清除 ──

    /// 将整个裁剪区域填充为指定灰度值 (8bpp, 0x00=黑, 0xFF=白)
    void clear(uint8_t gray = 0xFF);

    // ── 几何图形 ──

    /// 填充矩形（局部坐标）
    void fillRect(const Rect& rect, uint8_t gray);

    /// 绘制矩形边框（局部坐标）
    void drawRect(const Rect& rect, uint8_t gray, int thickness = 1);

    /// 绘制水平线（局部坐标）
    void drawHLine(int x, int y, int width, uint8_t gray);

    /// 绘制垂直线（局部坐标）
    void drawVLine(int x, int y, int height, uint8_t gray);

    /// 绘制任意方向直线（局部坐标，Bresenham 算法）
    void drawLine(Point from, Point to, uint8_t gray);

    // ── 位图 ──

    /// 绘制 8bpp 灰度位图（局部坐标）
    void drawBitmap(const uint8_t* data, int x, int y, int w, int h);

    /// 以指定前景色绘制 8bpp alpha 位图（局部坐标）
    void drawBitmapFg(const uint8_t* data, int x, int y, int w, int h,
                      uint8_t fgColor);

    // ── 文字 ──

    /// 绘制单行 UTF-8 文字 (通过字体引擎)，y 为基线坐标（局部坐标）
    void drawText(font_engine_t* engine, const char* text,
                  int x, int y, uint8_t fontSize, uint8_t color);

    /// 度量文字渲染后的像素宽度（不绘制）
    int measureText(font_engine_t* engine, const char* text, uint8_t fontSize) const;

    // ── 访问器 ──

    /// 获取裁剪区域（屏幕绝对坐标）
    Rect clipRect() const { return clip_; }

    /// 获取 M5GFX display 指针
    M5GFX* display() const { return display_; }

private:
    M5GFX* display_;  ///< M5GFX 显示设备
    Rect clip_;       ///< 裁剪区域（屏幕绝对逻辑坐标）
};

} // namespace ink
