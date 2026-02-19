/**
 * @file Canvas.cpp
 * @brief Canvas 裁剪绘图引擎实现 (M5GFX 后端)。
 *
 * 所有绘图通过 M5GFX API 执行，坐标变换由 M5GFX 内部处理。
 * 文字渲染通过 font_engine 获取 glyph bitmap 后用 pushImage 输出。
 */

#include "ink_ui/core/Canvas.h"

#include <cstdlib>
#include <cstring>

#include <M5GFX.h>

extern "C" {
#include "font_engine/font_engine.h"
}

namespace ink {

// ============================================================================
//  构造与裁剪
// ============================================================================

Canvas::Canvas(M5GFX* display, Rect clip)
    : display_(display), clip_(clip) {
    if (display_) {
        display_->setClipRect(clip_.x, clip_.y, clip_.w, clip_.h);
    }
}

Canvas::~Canvas() {
    if (display_) {
        display_->clearClipRect();
    }
}

Canvas Canvas::clipped(const Rect& subRect) const {
    // subRect 是局部坐标，转为屏幕绝对坐标
    Rect absRect = {clip_.x + subRect.x, clip_.y + subRect.y,
                    subRect.w, subRect.h};
    // 取交集
    Rect newClip = clip_.intersection(absRect);
    return Canvas(display_, newClip);
}

// ============================================================================
//  清除
// ============================================================================

void Canvas::clear(uint8_t gray) {
    if (clip_.isEmpty() || !display_) return;
    display_->fillRect(clip_.x, clip_.y, clip_.w, clip_.h, gray);
}

// ============================================================================
//  几何图形
// ============================================================================

void Canvas::fillRect(const Rect& rect, uint8_t gray) {
    if (clip_.isEmpty() || !display_) return;
    // 局部坐标 → 屏幕绝对坐标
    int ax = clip_.x + rect.x;
    int ay = clip_.y + rect.y;
    display_->fillRect(ax, ay, rect.w, rect.h, gray);
}

void Canvas::drawRect(const Rect& rect, uint8_t gray, int thickness) {
    if (rect.w <= 0 || rect.h <= 0 || thickness <= 0) return;
    // 上边
    fillRect({rect.x, rect.y, rect.w, thickness}, gray);
    // 下边
    fillRect({rect.x, rect.y + rect.h - thickness, rect.w, thickness}, gray);
    // 左边
    fillRect({rect.x, rect.y + thickness, thickness, rect.h - 2 * thickness}, gray);
    // 右边
    fillRect({rect.x + rect.w - thickness, rect.y + thickness, thickness, rect.h - 2 * thickness}, gray);
}

void Canvas::drawHLine(int x, int y, int width, uint8_t gray) {
    if (clip_.isEmpty() || !display_) return;
    display_->drawFastHLine(clip_.x + x, clip_.y + y, width, gray);
}

void Canvas::drawVLine(int x, int y, int height, uint8_t gray) {
    if (clip_.isEmpty() || !display_) return;
    display_->drawFastVLine(clip_.x + x, clip_.y + y, height, gray);
}

void Canvas::drawLine(Point from, Point to, uint8_t gray) {
    if (clip_.isEmpty() || !display_) return;
    display_->drawLine(clip_.x + from.x, clip_.y + from.y,
                       clip_.x + to.x, clip_.y + to.y, gray);
}

// ============================================================================
//  位图绘制
// ============================================================================

void Canvas::drawBitmap(const uint8_t* data, int x, int y, int w, int h) {
    if (!display_ || !data || w <= 0 || h <= 0 || clip_.isEmpty()) return;
    display_->pushImage(clip_.x + x, clip_.y + y, w, h, data);
}

void Canvas::drawBitmapFg(const uint8_t* data, int x, int y, int w, int h,
                           uint8_t fgColor) {
    if (!display_ || !data || w <= 0 || h <= 0 || clip_.isEmpty()) return;

    // 8bpp alpha 混合：创建临时 buffer 进行 alpha 混合后 pushImage
    // fgColor 是 8bpp 灰度 (0x00=黑, 0xFF=白)
    int size = w * h;
    auto* mixed = static_cast<uint8_t*>(malloc(size));
    if (!mixed) return;

    for (int i = 0; i < size; i++) {
        uint8_t alpha = data[i];
        if (alpha == 0) {
            mixed[i] = 0xFF;  // 透明 → 白色背景
        } else if (alpha == 0xFF) {
            mixed[i] = fgColor;
        } else {
            // 线性插值: result = white + alpha * (fg - white) / 255
            mixed[i] = static_cast<uint8_t>(
                0xFF + static_cast<int>(alpha) *
                (static_cast<int>(fgColor) - 0xFF) / 255);
        }
    }

    display_->pushImage(clip_.x + x, clip_.y + y, w, h, mixed);
    free(mixed);
}

// ============================================================================
//  UTF-8 解码 (内部 static)
// ============================================================================

/// 返回 UTF-8 首字节对应的字节长度
static int utf8ByteLen(uint8_t ch) {
    if ((ch & 0x80) == 0x00) return 1;
    if ((ch & 0xE0) == 0xC0) return 2;
    if ((ch & 0xF0) == 0xE0) return 3;
    if ((ch & 0xF8) == 0xF0) return 4;
    return 1;  // malformed, skip one byte
}

/// 读取下一个 Unicode code point，并前进指针
static uint32_t nextCodepoint(const char** str) {
    const auto* s = reinterpret_cast<const uint8_t*>(*str);
    if (*s == 0) return 0;

    int len = utf8ByteLen(*s);

    // 验证续字节存在
    for (int i = 1; i < len; i++) {
        if (s[i] == 0) {
            *str += 1;
            return 0xFFFD;
        }
    }

    uint32_t cp = 0;
    switch (len) {
        case 1: cp = s[0]; break;
        case 2: cp = (s[0] & 0x1F) << 6  | (s[1] & 0x3F); break;
        case 3: cp = (s[0] & 0x0F) << 12 | (s[1] & 0x3F) << 6  | (s[2] & 0x3F); break;
        case 4: cp = (s[0] & 0x07) << 18 | (s[1] & 0x3F) << 12 | (s[2] & 0x3F) << 6 | (s[3] & 0x3F); break;
        default: cp = 0xFFFD; break;
    }

    *str += len;
    return cp;
}

// ============================================================================
//  文字渲染 (通过 font_engine)
// ============================================================================

void Canvas::drawText(font_engine_t* engine, const char* text,
                      int x, int y, uint8_t fontSize, uint8_t color) {
    if (!display_ || !engine || !text || *text == '\0' || clip_.isEmpty()) return;

    // 局部坐标 → 屏幕绝对坐标
    int cursorX = clip_.x + x;
    int cursorY = clip_.y + y;

    const char* ptr = text;
    uint32_t cp;
    while ((cp = nextCodepoint(&ptr)) != 0) {
        if (cp == '\n') break;

        font_scaled_glyph_t scaled;
        if (!font_engine_get_scaled_glyph(engine, cp, fontSize, &scaled)) {
            continue;
        }

        if (scaled.bitmap && scaled.width > 0 && scaled.height > 0) {
            // glyph 基线定位: drawX = cursor + x_offset, drawY = cursor(baseline) - y_offset
            int drawX = cursorX + scaled.x_offset;
            int drawY = cursorY - scaled.y_offset;

            if (color == Color::Black) {
                // 黑色文字：bitmap 中 0x00=黑(前景), 0xFF=白(背景)
                // 使用 alpha 混合：对白色背景上绘制文字
                // bitmap 值越小越黑，作为 alpha 的反转
                int size = scaled.width * scaled.height;
                auto* blended = static_cast<uint8_t*>(malloc(size));
                if (blended) {
                    for (int i = 0; i < size; i++) {
                        // glyph bitmap: 0x00=全黑(前景), 0xFF=全白(背景)
                        // 我们需要: alpha = 255 - bitmap_value
                        // result = bg + alpha * (fg - bg) / 255
                        // 简化为: 对白色背景上绘制 -> result = bitmap_value (直接)
                        // 但如果 color != black, 需要混合
                        blended[i] = scaled.bitmap[i];  // glyph 本身就是灰度
                    }
                    display_->pushImage(drawX, drawY, scaled.width, scaled.height, blended);
                    free(blended);
                }
            } else {
                // 非黑色文字：需要 alpha 混合
                int size = scaled.width * scaled.height;
                auto* blended = static_cast<uint8_t*>(malloc(size));
                if (blended) {
                    for (int i = 0; i < size; i++) {
                        // alpha = 255 - bitmap (bitmap 0x00=前景=不透明, 0xFF=背景=透明)
                        uint8_t alpha = 255 - scaled.bitmap[i];
                        if (alpha == 0) {
                            blended[i] = 0xFF;  // 透明
                        } else {
                            // 在白色背景上混合
                            blended[i] = static_cast<uint8_t>(
                                0xFF + static_cast<int>(alpha) *
                                (static_cast<int>(color) - 0xFF) / 255);
                        }
                    }
                    display_->pushImage(drawX, drawY, scaled.width, scaled.height, blended);
                    free(blended);
                }
            }

            // 释放缩放后的 bitmap
            if (scaled.bitmap) {
                free(scaled.bitmap);
            }
        }

        cursorX += scaled.advance_x;
    }
}

int Canvas::measureText(font_engine_t* engine, const char* text, uint8_t fontSize) const {
    if (!engine || !text || *text == '\0') return 0;

    int width = 0;
    const char* ptr = text;
    uint32_t cp;
    while ((cp = nextCodepoint(&ptr)) != 0) {
        if (cp == '\n') break;
        font_scaled_glyph_t scaled;
        if (font_engine_get_scaled_glyph(engine, cp, fontSize, &scaled)) {
            width += scaled.advance_x;
            // 释放 bitmap（measureText 不需要 bitmap，但 API 会分配）
            if (scaled.bitmap) {
                free(scaled.bitmap);
            }
        }
    }
    return width;
}

} // namespace ink
