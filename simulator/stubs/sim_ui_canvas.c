/**
 * @file sim_ui_canvas.c
 * @brief Minimal ui_canvas stubs for simulator.
 * Only provides functions referenced by ui_icon.c.
 */
#include <stdint.h>

void ui_canvas_draw_bitmap_fg(uint8_t *fb, int x, int y, int w, int h,
                               const uint8_t *bitmap, uint8_t color) {
    (void)fb; (void)x; (void)y; (void)w; (void)h;
    (void)bitmap; (void)color;
    /* No-op in simulator — icons are drawn via InkUI Canvas, not old ui_canvas */
}
