#include "epdiy.h"

const EpdGlyph* epd_get_glyph(const EpdFont* font, uint32_t code_point) {
    if (!font || !font->intervals || !font->glyph) return NULL;
    int lo = 0, hi = (int)font->interval_count - 1;
    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        const EpdUnicodeInterval* interval = &font->intervals[mid];
        if (code_point < interval->first) {
            hi = mid - 1;
        } else if (code_point > interval->last) {
            lo = mid + 1;
        } else {
            return &font->glyph[interval->offset + (code_point - interval->first)];
        }
    }
    return NULL;
}
