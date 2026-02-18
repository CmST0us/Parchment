/**
 * @file Canvas.cpp
 * @brief Canvas 裁剪绘图引擎实现。
 *
 * 从旧 ui_canvas.c 迁移核心像素操作、坐标变换、UTF-8 解码和 glyph 渲染逻辑，
 * 增加裁剪区域管理和局部坐标系支持。
 */

#include "ink_ui/core/Canvas.h"

#include <cstdlib>
#include <cstring>

extern "C" {
#include "epdiy.h"
#include <miniz.h>
}

namespace ink {

// ============================================================================
//  构造与裁剪
// ============================================================================

Canvas::Canvas(uint8_t* fb, Rect clip)
    : fb_(fb), clip_(clip) {}

Canvas Canvas::clipped(const Rect& subRect) const {
    // subRect 是局部坐标，转为屏幕绝对坐标
    Rect absRect = {clip_.x + subRect.x, clip_.y + subRect.y,
                    subRect.w, subRect.h};
    // 取交集
    Rect newClip = clip_.intersection(absRect);
    return Canvas(fb_, newClip);
}

// ============================================================================
//  像素操作 (private)
// ============================================================================

void Canvas::setPixel(int absX, int absY, uint8_t gray) {
    // 裁剪检查
    if (absX < clip_.x || absX >= clip_.right() ||
        absY < clip_.y || absY >= clip_.bottom()) {
        return;
    }
    // 屏幕边界检查
    if (absX < 0 || absX >= 540 || absY < 0 || absY >= 960) {
        return;
    }
    // 逻辑坐标 → 物理坐标: px = ly, py = 539 - lx
    int px = absY;
    int py = (kFbPhysHeight - 1) - absX;
    uint8_t* buf_ptr = &fb_[py * (kFbPhysWidth / 2) + px / 2];
    if (px & 1) {
        *buf_ptr = (*buf_ptr & 0x0F) | (gray & 0xF0);
    } else {
        *buf_ptr = (*buf_ptr & 0xF0) | (gray >> 4);
    }
}

uint8_t Canvas::getPixel(int absX, int absY) const {
    if (absX < 0 || absX >= 540 || absY < 0 || absY >= 960) {
        return 0xFF;
    }
    int px = absY;
    int py = (kFbPhysHeight - 1) - absX;
    uint8_t byte_val = fb_[py * (kFbPhysWidth / 2) + px / 2];
    if (px & 1) {
        return byte_val & 0xF0;
    } else {
        return (byte_val & 0x0F) << 4;
    }
}

// ============================================================================
//  清除
// ============================================================================

void Canvas::clear(uint8_t gray) {
    if (clip_.isEmpty() || !fb_) {
        return;
    }
    for (int ay = clip_.y; ay < clip_.bottom(); ay++) {
        for (int ax = clip_.x; ax < clip_.right(); ax++) {
            setPixel(ax, ay, gray);
        }
    }
}

// ============================================================================
//  几何图形
// ============================================================================

void Canvas::fillRect(const Rect& rect, uint8_t gray) {
    if (clip_.isEmpty() || !fb_) {
        return;
    }

    // 局部坐标 → 屏幕绝对坐标
    int ax0 = clip_.x + rect.x;
    int ay0 = clip_.y + rect.y;
    int ax1 = ax0 + rect.w;
    int ay1 = ay0 + rect.h;

    // 与 clip 取交集
    if (ax0 < clip_.x) ax0 = clip_.x;
    if (ay0 < clip_.y) ay0 = clip_.y;
    if (ax1 > clip_.right()) ax1 = clip_.right();
    if (ay1 > clip_.bottom()) ay1 = clip_.bottom();

    // 屏幕边界裁剪
    if (ax0 < 0) ax0 = 0;
    if (ay0 < 0) ay0 = 0;
    if (ax1 > 540) ax1 = 540;
    if (ay1 > 960) ay1 = 960;

    if (ax0 >= ax1 || ay0 >= ay1) {
        return;
    }

    for (int ay = ay0; ay < ay1; ay++) {
        for (int ax = ax0; ax < ax1; ax++) {
            // 直接写入，跳过 setPixel 的冗余检查
            int px = ay;
            int py = (kFbPhysHeight - 1) - ax;
            uint8_t* buf_ptr = &fb_[py * (kFbPhysWidth / 2) + px / 2];
            if (px & 1) {
                *buf_ptr = (*buf_ptr & 0x0F) | (gray & 0xF0);
            } else {
                *buf_ptr = (*buf_ptr & 0xF0) | (gray >> 4);
            }
        }
    }
}

void Canvas::drawRect(const Rect& rect, uint8_t gray, int thickness) {
    if (rect.w <= 0 || rect.h <= 0 || thickness <= 0) {
        return;
    }
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
    fillRect({x, y, width, 1}, gray);
}

void Canvas::drawVLine(int x, int y, int height, uint8_t gray) {
    fillRect({x, y, 1, height}, gray);
}

void Canvas::drawLine(Point from, Point to, uint8_t gray) {
    if (clip_.isEmpty() || !fb_) {
        return;
    }

    int x0 = from.x;
    int y0 = from.y;
    int x1 = to.x;
    int y1 = to.y;

    int dx = x1 > x0 ? x1 - x0 : x0 - x1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1;
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    while (true) {
        // setPixel 会处理局部→绝对转换和裁剪
        setPixel(clip_.x + x0, clip_.y + y0, gray);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// ============================================================================
//  位图绘制
// ============================================================================

void Canvas::drawBitmap(const uint8_t* data, int x, int y, int w, int h) {
    if (!fb_ || !data || w <= 0 || h <= 0 || clip_.isEmpty()) {
        return;
    }

    for (int by = 0; by < h; by++) {
        int absY = clip_.y + y + by;
        if (absY < clip_.y || absY >= clip_.bottom()) {
            continue;
        }

        for (int bx = 0; bx < w; bx++) {
            int absX = clip_.x + x + bx;
            if (absX < clip_.x || absX >= clip_.right()) {
                continue;
            }

            int bmp_idx = by * w + bx;
            uint8_t bmp_byte = data[bmp_idx / 2];
            uint8_t gray_val;
            if (bmp_idx & 1) {
                gray_val = bmp_byte & 0xF0;
            } else {
                gray_val = (bmp_byte & 0x0F) << 4;
            }

            setPixel(absX, absY, gray_val);
        }
    }
}

void Canvas::drawBitmapFg(const uint8_t* data, int x, int y, int w, int h,
                           uint8_t fgColor) {
    if (!fb_ || !data || w <= 0 || h <= 0 || clip_.isEmpty()) {
        return;
    }

    uint8_t fg4 = fgColor >> 4;  // 0-15

    for (int by = 0; by < h; by++) {
        int absY = clip_.y + y + by;
        if (absY < clip_.y || absY >= clip_.bottom()) {
            continue;
        }

        for (int bx = 0; bx < w; bx++) {
            int absX = clip_.x + x + bx;
            if (absX < clip_.x || absX >= clip_.right()) {
                continue;
            }

            // 提取 4bpp alpha 值
            int bmp_idx = by * w + bx;
            uint8_t bmp_byte = data[bmp_idx / 2];
            uint8_t alpha;
            if (bmp_idx & 1) {
                alpha = bmp_byte >> 4;
            } else {
                alpha = bmp_byte & 0x0F;
            }

            if (alpha == 0) {
                continue;  // 完全透明
            }

            if (alpha == 0x0F) {
                // 完全不透明
                setPixel(absX, absY, fgColor);
            } else {
                // 线性插值: result = bg + alpha * (fg - bg) / 15
                uint8_t bg4 = getPixel(absX, absY) >> 4;
                uint8_t result4 = static_cast<uint8_t>(
                    bg4 + static_cast<int>(alpha) *
                    (static_cast<int>(fg4) - static_cast<int>(bg4)) / 15);
                setPixel(absX, absY, result4 << 4);
            }
        }
    }
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
//  Glyph 解压 (内部 static)
// ============================================================================

/// 解压 zlib 压缩的 glyph bitmap
static uint8_t* decompressGlyph(const EpdFont* font, const EpdGlyph* glyph,
                                 size_t bitmapSize) {
    auto* buf = static_cast<uint8_t*>(malloc(bitmapSize));
    if (!buf) return nullptr;

    auto* decomp = static_cast<tinfl_decompressor*>(
        malloc(sizeof(tinfl_decompressor)));
    if (!decomp) {
        free(buf);
        return nullptr;
    }
    tinfl_init(decomp);

    size_t srcSize = glyph->compressed_size;
    size_t outSize = bitmapSize;
    tinfl_status status = tinfl_decompress(
        decomp,
        &font->bitmap[glyph->data_offset],
        &srcSize,
        buf, buf, &outSize,
        TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(decomp);

    if (status != TINFL_STATUS_DONE) {
        free(buf);
        return nullptr;
    }
    return buf;
}

// ============================================================================
//  字符渲染 (private)
// ============================================================================

void Canvas::drawChar(const EpdFont* font, uint32_t codepoint,
                      int* cursorX, int cursorY, uint8_t color) {
    const EpdGlyph* glyph = epd_get_glyph(font, codepoint);
    if (!glyph) return;

    uint16_t w = glyph->width;
    uint16_t h = glyph->height;
    int byteWidth = (w / 2 + w % 2);
    size_t bitmapSize = static_cast<size_t>(byteWidth) * h;

    const uint8_t* bitmap = nullptr;
    bool needFree = false;

    if (bitmapSize > 0) {
        if (font->compressed) {
            bitmap = decompressGlyph(font, glyph, bitmapSize);
            if (!bitmap) {
                *cursorX += glyph->advance_x;
                return;
            }
            needFree = true;
        } else {
            bitmap = &font->bitmap[glyph->data_offset];
        }
    }

    // cursorX/cursorY 是屏幕绝对坐标
    uint8_t fg4 = color >> 4;

    for (int by = 0; by < h; by++) {
        int absY = cursorY - glyph->top + by;
        if (absY < clip_.y || absY >= clip_.bottom()) continue;

        for (int bx = 0; bx < w; bx++) {
            int absX = *cursorX + glyph->left + bx;
            if (absX < clip_.x || absX >= clip_.right()) continue;

            // 提取 4bpp alpha
            uint8_t bm = bitmap[by * byteWidth + bx / 2];
            uint8_t alpha;
            if ((bx & 1) == 0) {
                alpha = bm & 0x0F;
            } else {
                alpha = bm >> 4;
            }

            if (alpha == 0) continue;

            if (alpha == 0x0F) {
                // 完全不透明，直接写入前景色
                setPixel(absX, absY, color);
            } else {
                // 从 framebuffer 读取实际背景色进行混合
                uint8_t bg4 = getPixel(absX, absY) >> 4;
                uint8_t color4 = static_cast<uint8_t>(
                    bg4 + static_cast<int>(alpha) *
                    (static_cast<int>(fg4) - static_cast<int>(bg4)) / 15);
                setPixel(absX, absY, color4 << 4);
            }
        }
    }

    if (needFree) {
        free(const_cast<uint8_t*>(bitmap));
    }
    *cursorX += glyph->advance_x;
}

// ============================================================================
//  文字渲染 (public)
// ============================================================================

void Canvas::drawText(const EpdFont* font, const char* text,
                      int x, int y, uint8_t color) {
    if (!fb_ || !font || !text || *text == '\0' || clip_.isEmpty()) return;

    // 局部坐标 → 屏幕绝对坐标
    int cursorX = clip_.x + x;
    int cursorY = clip_.y + y;

    uint32_t cp;
    while ((cp = nextCodepoint(&text)) != 0) {
        if (cp == '\n') break;
        drawChar(font, cp, &cursorX, cursorY, color);
    }
}

void Canvas::drawTextN(const EpdFont* font, const char* text, int maxBytes,
                        int x, int y, uint8_t color) {
    if (!fb_ || !font || !text || maxBytes <= 0 || clip_.isEmpty()) return;

    const char* end = text + maxBytes;
    int cursorX = clip_.x + x;
    int cursorY = clip_.y + y;

    while (text < end && *text != '\0') {
        int charLen = utf8ByteLen(static_cast<uint8_t>(*text));
        if (text + charLen > end) break;
        uint32_t cp = nextCodepoint(&text);
        if (cp == 0 || cp == '\n') break;
        drawChar(font, cp, &cursorX, cursorY, color);
    }
}

int Canvas::measureText(const EpdFont* font, const char* text) const {
    if (!font || !text || *text == '\0') return 0;

    int width = 0;
    uint32_t cp;
    while ((cp = nextCodepoint(&text)) != 0) {
        if (cp == '\n') break;
        const EpdGlyph* glyph = epd_get_glyph(font, cp);
        if (glyph) {
            width += glyph->advance_x;
        }
    }
    return width;
}

} // namespace ink
