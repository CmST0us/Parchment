/**
 * @file ui_text.h
 * @brief Text rendering engine for Parchment.
 *
 * Provides TrueType font loading, glyph caching, and anti-aliased text rendering
 * onto the 4bpp grayscale framebuffer. Supports UTF-8 including CJK characters.
 *
 * Usage:
 *   1. Load a TTF font file into memory.
 *   2. Create a font handle with ui_font_create().
 *   3. Draw text with ui_text_draw() or ui_text_draw_wrapped().
 *   4. Destroy the font when no longer needed.
 *
 * The glyph cache (direct-mapped, 512 entries) avoids repeated rasterization
 * for frequently used characters, which is critical for scrolling text views.
 */

#ifndef UI_TEXT_H
#define UI_TEXT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Opaque font handle. */
typedef struct ui_font ui_font_t;

/** Text horizontal alignment. */
typedef enum {
    UI_TEXT_ALIGN_LEFT = 0,
    UI_TEXT_ALIGN_CENTER,
    UI_TEXT_ALIGN_RIGHT,
} ui_text_align_t;

/* ========================================================================== */
/*  Font lifecycle                                                             */
/* ========================================================================== */

/**
 * @brief Create a font from TrueType data in memory.
 *
 * The TTF data buffer must remain valid for the lifetime of the font handle
 * (stb_truetype accesses it on demand during glyph rasterization).
 *
 * @param ttf_data  Pointer to TTF file contents.
 * @param pixel_size  Desired font size in pixels (e.g. 24.0f for body text).
 * @return Font handle, or NULL on failure.
 */
ui_font_t *ui_font_create(const uint8_t *ttf_data, float pixel_size);

/**
 * @brief Destroy a font and free all cached glyph bitmaps.
 *
 * @param font  Font handle (NULL-safe).
 */
void ui_font_destroy(ui_font_t *font);

/**
 * @brief Convenience: load a TTF file from disk and create a font.
 *
 * The file contents are allocated internally and freed on ui_font_destroy().
 *
 * @param path  File path to a .ttf file.
 * @param pixel_size  Desired font size in pixels.
 * @return Font handle, or NULL if the file cannot be read.
 */
ui_font_t *ui_font_load_file(const char *path, float pixel_size);

/* ========================================================================== */
/*  Font metrics                                                               */
/* ========================================================================== */

/**
 * @brief Get recommended line height (ascent - descent + lineGap), in pixels.
 */
int ui_font_line_height(const ui_font_t *font);

/**
 * @brief Get font ascent (pixels above baseline), positive value.
 */
int ui_font_ascent(const ui_font_t *font);

/**
 * @brief Get font descent (pixels below baseline), negative value.
 */
int ui_font_descent(const ui_font_t *font);

/* ========================================================================== */
/*  Text measurement                                                           */
/* ========================================================================== */

/**
 * @brief Measure the pixel width of a single line of UTF-8 text.
 *
 * @param font  Font handle.
 * @param text  UTF-8 string (NULL-terminated).
 * @return Width in pixels.
 */
int ui_text_measure_width(const ui_font_t *font, const char *text);

/**
 * @brief Calculate how many bytes of text fit within a given pixel width.
 *
 * Useful for line-breaking / pagination. Breaks at the last character
 * that fits entirely within max_width.
 *
 * @param font       Font handle.
 * @param text       UTF-8 string.
 * @param max_width  Available width in pixels.
 * @return Number of bytes from text that fit (always on a UTF-8 boundary).
 */
int ui_text_fit_width(const ui_font_t *font, const char *text, int max_width);

/* ========================================================================== */
/*  Text drawing                                                               */
/* ========================================================================== */

/**
 * @brief Draw a single line of UTF-8 text.
 *
 * Renders anti-aliased glyphs blended onto the existing framebuffer content.
 * The (x, y) position is the top-left corner of the text bounding box
 * (y is at the ascent line, not the baseline).
 *
 * @param fb     Framebuffer pointer (4bpp, same as ui_canvas).
 * @param font   Font handle.
 * @param x      Left X coordinate (logical, 0~539).
 * @param y      Top Y coordinate (logical, 0~959).
 * @param text   UTF-8 string (NULL-terminated).
 * @param color  Grayscale color for the text (e.g. UI_COLOR_BLACK).
 * @return Advance width in pixels.
 */
int ui_text_draw_line(uint8_t *fb, const ui_font_t *font,
                      int x, int y, const char *text, uint8_t color);

/**
 * @brief Draw UTF-8 text with automatic line wrapping.
 *
 * Wraps text within the specified width. For CJK characters, wrapping
 * can occur at any character boundary. For Latin text, wrapping prefers
 * word boundaries (spaces).
 *
 * @param fb           Framebuffer pointer.
 * @param font         Font handle.
 * @param x            Left X coordinate of the text box.
 * @param y            Top Y coordinate of the text box.
 * @param width        Available width for text wrapping.
 * @param max_height   Maximum height (0 = unlimited). Text beyond this is clipped.
 * @param text         UTF-8 string (NULL-terminated).
 * @param color        Grayscale color for the text.
 * @param align        Horizontal alignment within the box.
 * @param line_spacing Extra pixels between lines (can be 0).
 * @return Total height used in pixels.
 */
int ui_text_draw_wrapped(uint8_t *fb, const ui_font_t *font,
                         int x, int y, int width, int max_height,
                         const char *text, uint8_t color,
                         ui_text_align_t align, int line_spacing);

/* ========================================================================== */
/*  Pagination                                                                 */
/* ========================================================================== */

/**
 * @brief Compute how many bytes of text fit within a rectangular area.
 *
 * Uses the same line-wrapping logic as ui_text_draw_wrapped() but does not
 * render anything. Useful for computing page boundaries in a paginated view.
 *
 * @param font         Font handle.
 * @param text         UTF-8 string.
 * @param width        Available width for text wrapping.
 * @param max_height   Available height.
 * @param line_spacing Extra pixels between lines.
 * @return Number of bytes from text that fit in the area.
 */
int ui_text_paginate(const ui_font_t *font, const char *text,
                     int width, int max_height, int line_spacing);

#ifdef __cplusplus
}
#endif

#endif /* UI_TEXT_H */
