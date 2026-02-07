/**
 * @file ui_image.h
 * @brief Image decoding and rendering for the Parchment e-reader.
 *
 * Decodes PNG/JPEG/BMP/GIF images from memory buffers using stb_image,
 * converts to grayscale, and renders onto the 4bpp framebuffer.
 *
 * Usage:
 *   ui_image_t *img = ui_image_decode(data, data_len);
 *   ui_image_draw(fb, img, x, y, fit_width);  // scale to fit_width, keep AR
 *   ui_image_free(img);
 */

#ifndef UI_IMAGE_H
#define UI_IMAGE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Decoded image (8-bit grayscale). */
typedef struct {
    uint8_t *pixels;   /**< Row-major grayscale pixels (1 byte per pixel). */
    int      width;    /**< Image width in pixels. */
    int      height;   /**< Image height in pixels. */
} ui_image_t;

/**
 * @brief Decode an image from a memory buffer.
 *
 * Supports PNG, JPEG, BMP, GIF (via stb_image).
 * The image is converted to 8-bit grayscale internally.
 *
 * @param data      Pointer to the compressed image data.
 * @param data_len  Length of the data buffer in bytes.
 * @return Decoded image, or NULL on failure. Caller must call ui_image_free().
 */
ui_image_t *ui_image_decode(const uint8_t *data, size_t data_len);

/**
 * @brief Free a decoded image.
 */
void ui_image_free(ui_image_t *img);

/**
 * @brief Draw an image onto the 4bpp framebuffer, scaled to a target width.
 *
 * Maintains aspect ratio. If fit_width <= 0, draws at original size.
 * Uses nearest-neighbor scaling (fast, suitable for e-ink).
 *
 * @param fb         Framebuffer pointer.
 * @param img        Decoded image.
 * @param x          Destination X (logical coordinates).
 * @param y          Destination Y (logical coordinates).
 * @param fit_width  Target width (0 = original size).
 * @return Rendered height in pixels (useful for layout).
 */
int ui_image_draw(uint8_t *fb, const ui_image_t *img,
                  int x, int y, int fit_width);

/**
 * @brief Compute the display height of an image when scaled to fit_width.
 *
 * Does not render anything.
 *
 * @param img        Decoded image.
 * @param fit_width  Target width (0 = original size).
 * @return Height in pixels.
 */
int ui_image_get_height(const ui_image_t *img, int fit_width);

#ifdef __cplusplus
}
#endif

#endif /* UI_IMAGE_H */
