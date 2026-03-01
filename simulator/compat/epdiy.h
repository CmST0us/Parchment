/**
 * @file epdiy.h
 * @brief Minimal epdiy compatibility header for simulator.
 * Contains only font-related types needed by Canvas and ui_font.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t first;
    uint32_t last;
    uint32_t offset;
} EpdUnicodeInterval;

typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t advance_x;
    int16_t left;
    int16_t top;
    uint32_t compressed_size;
    uint32_t data_offset;
} EpdGlyph;

typedef struct {
    const uint8_t* bitmap;
    const EpdGlyph* glyph;
    const EpdUnicodeInterval* intervals;
    uint32_t interval_count;
    bool compressed;
    uint16_t advance_y;
    int ascender;
    int descender;
} EpdFont;

const EpdGlyph* epd_get_glyph(const EpdFont* font, uint32_t code_point);

#ifdef __cplusplus
}
#endif
