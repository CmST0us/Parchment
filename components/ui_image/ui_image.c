/**
 * @file ui_image.c
 * @brief Image decoding and rendering for the Parchment e-reader.
 *
 * Uses stb_image for decoding, converts to grayscale, renders onto 4bpp fb.
 */

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_GIF
#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR

#include "stb/stb_image.h"
#include "ui_image.h"
#include "ui_canvas.h"

#include <stdlib.h>
#include <string.h>

ui_image_t *ui_image_decode(const uint8_t *data, size_t data_len) {
    if (!data || data_len == 0) return NULL;

    int w, h, channels;
    /* Decode to 8-bit grayscale (1 channel) */
    uint8_t *pixels = stbi_load_from_memory(data, (int)data_len,
                                             &w, &h, &channels, 1);
    if (!pixels) return NULL;

    ui_image_t *img = (ui_image_t *)malloc(sizeof(ui_image_t));
    if (!img) {
        stbi_image_free(pixels);
        return NULL;
    }

    img->pixels = pixels;
    img->width = w;
    img->height = h;
    return img;
}

void ui_image_free(ui_image_t *img) {
    if (!img) return;
    if (img->pixels) stbi_image_free(img->pixels);
    free(img);
}

int ui_image_get_height(const ui_image_t *img, int fit_width) {
    if (!img || img->width <= 0 || img->height <= 0) return 0;

    int dw = img->width;
    int dh = img->height;

    if (fit_width > 0 && fit_width < dw) {
        dh = (dh * fit_width) / dw;
        dw = fit_width;
    }

    return dh > 0 ? dh : 1;
}

int ui_image_draw(uint8_t *fb, const ui_image_t *img,
                  int x, int y, int fit_width) {
    if (!fb || !img || !img->pixels) return 0;

    int src_w = img->width;
    int src_h = img->height;
    int dst_w = src_w;
    int dst_h = src_h;

    /* Scale to fit_width, maintaining aspect ratio */
    if (fit_width > 0 && fit_width < dst_w) {
        dst_h = (dst_h * fit_width) / dst_w;
        dst_w = fit_width;
    }
    if (dst_w <= 0 || dst_h <= 0) return 0;

    /* Nearest-neighbor scaling + render to framebuffer */
    for (int dy = 0; dy < dst_h; dy++) {
        int sy = (dy * src_h) / dst_h;
        if (sy >= src_h) sy = src_h - 1;

        for (int dx = 0; dx < dst_w; dx++) {
            int sx = (dx * src_w) / dst_w;
            if (sx >= src_w) sx = src_w - 1;

            uint8_t gray = img->pixels[sy * src_w + sx];
            /* Quantize to 4-bit (high nibble) for e-ink */
            uint8_t gray4 = gray & 0xF0;

            ui_canvas_draw_pixel(fb, x + dx, y + dy, gray4);
        }
    }

    return dst_h;
}
