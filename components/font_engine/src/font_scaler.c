/**
 * @file font_scaler.c
 * @brief 面积加权灰度缩放引擎 — 4bpp packed → 8bpp
 */

#include "font_engine/font_engine.h"
#include <math.h>
#include <string.h>

/** 从 4bpp packed bitmap 中读取单个像素 (0-15) */
static inline uint8_t get_4bpp_pixel(const uint8_t *src, uint8_t src_w, int x, int y)
{
    uint32_t row_bytes = (src_w + 1) / 2;
    uint32_t byte_idx = y * row_bytes + x / 2;
    if (x % 2 == 0) {
        return (src[byte_idx] >> 4) & 0x0F;
    } else {
        return src[byte_idx] & 0x0F;
    }
}

void font_scale_area_weighted(const uint8_t *src, uint8_t src_w, uint8_t src_h,
                              uint8_t *dst, uint8_t dst_w, uint8_t dst_h)
{
    if (!src || !dst || src_w == 0 || src_h == 0 || dst_w == 0 || dst_h == 0) {
        return;
    }

    float x_ratio = (float)src_w / dst_w;
    float y_ratio = (float)src_h / dst_h;

    for (int dy = 0; dy < dst_h; dy++) {
        float sy0 = dy * y_ratio;
        float sy1 = (dy + 1) * y_ratio;

        for (int dx = 0; dx < dst_w; dx++) {
            float sx0 = dx * x_ratio;
            float sx1 = (dx + 1) * x_ratio;

            float weighted_sum = 0.0f;
            float total_area = 0.0f;

            // 遍历与映射矩形重叠的所有源像素
            int iy_start = (int)sy0;
            int iy_end = (int)ceilf(sy1);
            int ix_start = (int)sx0;
            int ix_end = (int)ceilf(sx1);

            // 边界钳制
            if (iy_end > src_h) iy_end = src_h;
            if (ix_end > src_w) ix_end = src_w;

            for (int iy = iy_start; iy < iy_end; iy++) {
                // 计算 Y 方向的重叠
                float overlap_y0 = (iy > sy0) ? (float)iy : sy0;
                float overlap_y1 = ((iy + 1) < sy1) ? (float)(iy + 1) : sy1;
                float fy = overlap_y1 - overlap_y0;

                for (int ix = ix_start; ix < ix_end; ix++) {
                    // 计算 X 方向的重叠
                    float overlap_x0 = (ix > sx0) ? (float)ix : sx0;
                    float overlap_x1 = ((ix + 1) < sx1) ? (float)(ix + 1) : sx1;
                    float fx = overlap_x1 - overlap_x0;

                    float area = fx * fy;
                    uint8_t gray_4 = get_4bpp_pixel(src, src_w, ix, iy);
                    // 4bpp → 8bpp: gray_8 = gray_4 * 17
                    weighted_sum += (float)(gray_4 * 17) * area;
                    total_area += area;
                }
            }

            uint8_t result = 0;
            if (total_area > 0.0f) {
                float val = weighted_sum / total_area;
                if (val > 255.0f) val = 255.0f;
                if (val < 0.0f) val = 0.0f;
                result = (uint8_t)(val + 0.5f);
            }

            dst[dy * dst_w + dx] = result;
        }
    }
}
