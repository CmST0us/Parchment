/**
 * @file sim_epd_driver.c
 * @brief epd_driver stub for simulator — 硬件相关函数的空实现。
 */

#include "epd_driver.h"
#include <string.h>

static uint8_t s_stub_fb[960 / 2 * 540];  // 4bpp framebuffer

uint8_t *epd_driver_get_framebuffer(void) {
    return s_stub_fb;
}

void epd_driver_clear(void) {
    memset(s_stub_fb, 0xFF, sizeof(s_stub_fb));
}

esp_err_t epd_driver_update_screen_custom(epd_refresh_mode_t mode) {
    (void)mode;
    return 0;  /* ESP_OK */
}

esp_err_t epd_driver_update_area_custom(int x, int y, int w, int h,
                                         epd_refresh_mode_t mode) {
    (void)x; (void)y; (void)w; (void)h; (void)mode;
    return 0;
}

esp_err_t epd_driver_white_black_du_then_gl16(void) {
    return 0;
}

void epd_driver_set_all_white(void) {
    memset(s_stub_fb, 0xFF, sizeof(s_stub_fb));
}

void epd_driver_set_all_black(void) {
    memset(s_stub_fb, 0x00, sizeof(s_stub_fb));
}
