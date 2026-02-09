/**
 * @file epd_driver.c
 * @brief E-Ink 显示驱动封装层实现。
 */

#include "epd_driver.h"
#include "epdiy.h"
#include "esp_log.h"

static const char *TAG = "epd_driver";

/* epdiy 高层状态 */
static EpdiyHighlevelState s_hl_state;
static uint8_t *s_framebuffer = NULL;
static bool s_initialized = false;


esp_err_t epd_driver_init(void) {
    if (s_initialized) {
        ESP_LOGW(TAG, "EPD driver already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing EPD driver (ED047TC2, 960x540)...");

    /* 初始化 epdiy: M5PaperS3 板级 + ED047TC2 显示屏 */
    epd_init(&epd_board_m5papers3, &ED047TC2, EPD_LUT_64K);

    /* 初始化高层状态，分配帧缓冲区（PSRAM） */
    s_hl_state = epd_hl_init(EPD_BUILTIN_WAVEFORM);
    s_framebuffer = epd_hl_get_framebuffer(&s_hl_state);

    if (s_framebuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer in PSRAM");
        epd_deinit();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Framebuffer allocated at %p", s_framebuffer);

    /* 全屏清除，消除上电残影 */
    ESP_LOGI(TAG, "Calling epd_fullclear...");
    epd_poweron();
    epd_fullclear(&s_hl_state, 25);
    epd_poweroff();
    ESP_LOGI(TAG, "epd_fullclear done");

    s_initialized = true;
    ESP_LOGI(TAG, "EPD driver initialized successfully");
    return ESP_OK;
}

void epd_driver_deinit(void) {
    if (!s_initialized) {
        return;
    }
    epd_poweroff();
    epd_deinit();
    s_framebuffer = NULL;
    s_initialized = false;
    ESP_LOGI(TAG, "EPD driver deinitialized");
}

uint8_t *epd_driver_get_framebuffer(void) {
    return s_framebuffer;
}

esp_err_t epd_driver_update_screen(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    epd_poweron();
    enum EpdDrawError err = epd_hl_update_screen(&s_hl_state, MODE_GL16, 25);
    epd_poweroff();

    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Screen update failed: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t epd_driver_update_area(int x, int y, int w, int h) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    EpdRect area = {.x = x, .y = y, .width = w, .height = h};

    epd_poweron();
    enum EpdDrawError err = epd_hl_update_area(&s_hl_state, MODE_DU, 25, area);
    epd_poweroff();

    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Area update failed: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void epd_driver_clear(void) {
    if (!s_initialized) {
        return;
    }
    epd_poweron();
    epd_fullclear(&s_hl_state, 25);
    epd_poweroff();
    ESP_LOGI(TAG, "Screen cleared");
}

void epd_driver_draw_pixel(int x, int y, uint8_t color) {
    if (s_framebuffer != NULL) {
        epd_draw_pixel(x, y, color, s_framebuffer);
    }
}

void epd_driver_fill_rect(int x, int y, int w, int h, uint8_t color) {
    if (s_framebuffer != NULL) {
        EpdRect rect = {.x = x, .y = y, .width = w, .height = h};
        epd_fill_rect(rect, color, s_framebuffer);
    }
}

void epd_driver_set_all_white(void) {
    if (s_initialized) {
        epd_hl_set_all_white(&s_hl_state);
    }
}
