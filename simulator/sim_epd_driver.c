/**
 * @file sim_epd_driver.c
 * @brief SDL-based epd_driver implementation for desktop simulator.
 *
 * Replaces the epdiy-based EPD driver with a simple in-memory framebuffer.
 * The SDL main loop reads this framebuffer to update the window texture.
 */

#include "epd_driver.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/** Physical framebuffer dimensions (landscape, same as real hardware). */
#define FB_PHYS_WIDTH  960
#define FB_PHYS_HEIGHT 540
#define FB_SIZE        (FB_PHYS_WIDTH * FB_PHYS_HEIGHT / 2)

/** Framebuffer (4bpp, 2 pixels per byte). */
static uint8_t *s_framebuffer = NULL;
static int s_initialized = 0;

/**
 * @brief Display-dirty flag, read by the SDL main loop.
 *
 * Set to 1 by update_screen/update_area; cleared by SDL render loop.
 */
volatile int g_sim_display_dirty = 0;

esp_err_t epd_driver_init(void) {
    if (s_initialized) {
        return ESP_OK;
    }

    s_framebuffer = (uint8_t *)calloc(1, FB_SIZE);
    if (!s_framebuffer) {
        fprintf(stderr, "sim_epd_driver: failed to allocate framebuffer\n");
        return ESP_FAIL;
    }

    /* Initialize to white (0xFF = both nibbles 0xF) */
    memset(s_framebuffer, 0xFF, FB_SIZE);

    s_initialized = 1;
    printf("sim_epd_driver: initialized (%dx%d, %d bytes)\n",
           FB_PHYS_WIDTH, FB_PHYS_HEIGHT, FB_SIZE);
    return ESP_OK;
}

void epd_driver_deinit(void) {
    if (s_framebuffer) {
        free(s_framebuffer);
        s_framebuffer = NULL;
    }
    s_initialized = 0;
}

uint8_t *epd_driver_get_framebuffer(void) {
    return s_framebuffer;
}

esp_err_t epd_driver_update_screen(void) {
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    g_sim_display_dirty = 1;
    return ESP_OK;
}

esp_err_t epd_driver_update_area(int x, int y, int w, int h) {
    (void)x; (void)y; (void)w; (void)h;
    if (!s_initialized) return ESP_ERR_INVALID_STATE;
    g_sim_display_dirty = 1;
    return ESP_OK;
}

void epd_driver_clear(void) {
    if (s_framebuffer) {
        memset(s_framebuffer, 0xFF, FB_SIZE);
        g_sim_display_dirty = 1;
    }
}

void epd_driver_draw_pixel(int x, int y, uint8_t color) {
    if (!s_framebuffer) return;
    if (x < 0 || x >= FB_PHYS_WIDTH || y < 0 || y >= FB_PHYS_HEIGHT) return;

    uint8_t *p = &s_framebuffer[y * (FB_PHYS_WIDTH / 2) + x / 2];
    if (x % 2) {
        *p = (*p & 0x0F) | (color & 0xF0);
    } else {
        *p = (*p & 0xF0) | (color >> 4);
    }
}

void epd_driver_fill_rect(int x, int y, int w, int h, uint8_t color) {
    for (int iy = y; iy < y + h; iy++) {
        for (int ix = x; ix < x + w; ix++) {
            epd_driver_draw_pixel(ix, iy, color);
        }
    }
}

void epd_driver_set_all_white(void) {
    if (s_framebuffer) {
        memset(s_framebuffer, 0xFF, FB_SIZE);
    }
}
