/**
 * @file Canvas.h
 * @brief 带裁剪区域的绘图引擎。
 *
 * Canvas 封装 framebuffer 操作，提供局部坐标系和自动裁剪。
 * 所有绘图坐标相对于裁剪区域左上角，超出裁剪区域的像素被自动丢弃。
 * RenderEngine 为每个需要重绘的 View 创建 Canvas 实例。
 */

#pragma once

#include <cstdint>

extern "C" {
#include "epdiy.h"
}

#include "ink_ui/core/Geometry.h"

namespace ink {

/// 灰度调色板常量
namespace Color {
constexpr uint8_t Black  = 0x00;
constexpr uint8_t Dark   = 0x30;
constexpr uint8_t Medium = 0x80;
constexpr uint8_t Light  = 0xC0;
constexpr uint8_t White  = 0xF0;
constexpr uint8_t Clear  = 0x01;  ///< 哨兵值：透明，不绘制背景
} // namespace Color

/// 带裁剪区域的绘图引擎
class Canvas {
public:
    /**
     * @brief 构造 Canvas。
     * @param fb   4bpp framebuffer 指针（物理布局 960×540）。
     * @param clip 裁剪区域（屏幕绝对逻辑坐标，540×960 portrait）。
     */
    Canvas(uint8_t* fb, Rect clip);

    /**
     * @brief 创建子 Canvas，裁剪区域为当前 clip 与 subRect 的交集。
     * @param subRect 局部坐标系下的子区域。
     * @return 新的 Canvas，clip 为交集区域。
     */
    Canvas clipped(const Rect& subRect) const;

    // ── 清除 ──

    /// 将整个裁剪区域填充为指定灰度值
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

    /// 绘制 4bpp 灰度位图（局部坐标）
    void drawBitmap(const uint8_t* data, int x, int y, int w, int h);

    /// 以指定前景色绘制 4bpp alpha 位图（局部坐标）
    void drawBitmapFg(const uint8_t* data, int x, int y, int w, int h,
                      uint8_t fgColor);

    // ── 文字 ──

    /// 绘制单行 UTF-8 文字，y 为基线坐标（局部坐标）
    void drawText(const EpdFont* font, const char* text,
                  int x, int y, uint8_t color);

    /// 绘制指定最大字节数的 UTF-8 文字（局部坐标）
    void drawTextN(const EpdFont* font, const char* text, int maxBytes,
                   int x, int y, uint8_t color);

    /// 度量文字渲染后的像素宽度（不写入 framebuffer）
    int measureText(const EpdFont* font, const char* text) const;

    // ── 访问器 ──

    /// 获取裁剪区域（屏幕绝对坐标）
    Rect clipRect() const { return clip_; }

    /// 获取底层 framebuffer 指针
    uint8_t* framebuffer() const { return fb_; }

private:
    uint8_t* fb_;   ///< 4bpp framebuffer（物理布局 960×540）
    Rect clip_;     ///< 裁剪区域（屏幕绝对逻辑坐标）

    /// 物理 framebuffer 尺寸
    static constexpr int kFbPhysWidth  = 960;
    static constexpr int kFbPhysHeight = 540;

    /// 写入单个像素（屏幕绝对逻辑坐标，含裁剪检查和坐标变换）
    void setPixel(int absX, int absY, uint8_t gray);

    /// 读取单个像素（屏幕绝对逻辑坐标，不做裁剪检查）
    uint8_t getPixel(int absX, int absY) const;

    /// 内部绘制单个字符，前进 cursorX（屏幕绝对坐标）
    void drawChar(const EpdFont* font, uint32_t codepoint,
                  int* cursorX, int cursorY, uint8_t color);
};

} // namespace ink
