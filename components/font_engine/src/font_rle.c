/**
 * @file font_rle.c
 * @brief RLE 解码器 — 将 .bin 字体的 RLE 编码 glyph 解码为 4bpp packed bitmap
 */

#include "font_engine/font_engine.h"
#include <string.h>

int font_rle_decode(const uint8_t *rle_data, uint32_t rle_size,
                    uint8_t width, uint8_t height,
                    uint8_t *out_buf, uint32_t out_size)
{
    if (!rle_data || !out_buf || width == 0 || height == 0) {
        return 0;
    }

    // 4bpp packed: 每字节 2 像素
    uint32_t row_bytes = (width + 1) / 2;
    uint32_t needed = row_bytes * height;
    if (out_size < needed) {
        return 0;
    }

    memset(out_buf, 0, needed);

    uint32_t rle_pos = 0;
    int total_pixels = 0;

    for (uint8_t row = 0; row < height; row++) {
        uint8_t col = 0;
        uint32_t row_offset = row * row_bytes;

        while (rle_pos < rle_size) {
            uint8_t byte = rle_data[rle_pos++];
            uint8_t count = (byte >> 4) & 0x0F;
            uint8_t gray = byte & 0x0F;

            // 行结束标记: count == 0
            if (count == 0) {
                break;
            }

            for (uint8_t i = 0; i < count && col < width; i++, col++) {
                // 4bpp packed: 高 4 位 = 偶数列, 低 4 位 = 奇数列
                uint32_t byte_idx = row_offset + col / 2;
                if (col % 2 == 0) {
                    out_buf[byte_idx] = (out_buf[byte_idx] & 0x0F) | (gray << 4);
                } else {
                    out_buf[byte_idx] = (out_buf[byte_idx] & 0xF0) | gray;
                }
                total_pixels++;
            }
        }
    }

    return total_pixels;
}
