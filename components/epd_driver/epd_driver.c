/**
 * @file epd_driver.c
 * @brief E-Ink 显示驱动封装层实现。
 */

#include "epd_driver.h"
#include "epdiy.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "epd_driver";

/* epdiy 高层状态 */
static EpdiyHighlevelState s_hl_state;
static uint8_t *s_framebuffer = NULL;
static bool s_initialized = false;

/* ── 快速 GL16 波形（灰度版 DU）──
 *
 * 15 个相位，运行时生成 LUT 覆盖所有 16×16 from→to 灰度组合。
 * Phase p 中：|delta| > p 的像素施加 lighten/darken，其余不操作。
 * 效果：GL16 级灰度精度，DU 级速度，无闪烁。
 */
#define FAST_GL16_PHASES 15
static int s_fast_gl16_times[FAST_GL16_PHASES] = {
    10, 10, 10, 10, 10, 15, 15, 20, 20, 30, 30, 50, 50, 80, 120
};
static const EpdWaveformTempInterval s_fast_gl16_temp[1] = {
    { .min = 0, .max = 50 }
};
static uint8_t s_fast_gl16_data[FAST_GL16_PHASES][16][4];
static EpdWaveformPhases s_fast_gl16_phases;
static const EpdWaveformPhases *s_fast_gl16_ranges[1];
static EpdWaveformMode s_fast_gl16_mode;
static const EpdWaveformMode *s_fast_gl16_modes[1];
static EpdWaveform s_fast_gl16_waveform;

static void init_fast_gl16_waveform(void) {
    /* 生成 LUT: 对每个 (phase, to, from) 计算 lighten/darken/nothing */
    for (int p = 0; p < FAST_GL16_PHASES; p++) {
        for (int t = 0; t < 16; t++) {
            for (int bi = 0; bi < 4; bi++) {
                uint8_t byte_val = 0;
                for (int fi = 0; fi < 4; fi++) {
                    int f = bi * 4 + fi;
                    int delta = t - f;
                    uint8_t action = 0;
                    if (delta > 0 && p < delta)
                        action = 2;  /* lighten */
                    else if (delta < 0 && p < -delta)
                        action = 1;  /* darken */
                    byte_val |= (action << (6 - 2 * fi));
                }
                s_fast_gl16_data[p][t][bi] = byte_val;
            }
        }
    }

    /* 组装波形结构体 */
    s_fast_gl16_phases.phases = FAST_GL16_PHASES;
    s_fast_gl16_phases.phase_times = s_fast_gl16_times;
    s_fast_gl16_phases.luts = (const uint8_t *)s_fast_gl16_data;

    s_fast_gl16_ranges[0] = &s_fast_gl16_phases;

    s_fast_gl16_mode.type = 5;  /* MODE_GL16 */
    s_fast_gl16_mode.temp_ranges = 1;
    s_fast_gl16_mode.range_data = s_fast_gl16_ranges;

    s_fast_gl16_modes[0] = &s_fast_gl16_mode;

    s_fast_gl16_waveform.num_modes = 1;
    s_fast_gl16_waveform.num_temp_ranges = 1;
    s_fast_gl16_waveform.mode_data = s_fast_gl16_modes;
    s_fast_gl16_waveform.temp_intervals = s_fast_gl16_temp;
}


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

    /* 生成快速 GL16 波形 LUT */
    init_fast_gl16_waveform();

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
    return epd_driver_update_area_mode(x, y, w, h, MODE_DU);
}

esp_err_t epd_driver_update_area_mode(int x, int y, int w, int h, int mode) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    EpdRect area = {.x = x, .y = y, .width = w, .height = h};

    epd_poweron();
    enum EpdDrawError err = epd_hl_update_area(&s_hl_state, (enum EpdDrawMode)mode, 25, area);
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

esp_err_t epd_driver_update_screen_fast_gl16(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 临时切换到快速 GL16 波形 */
    const EpdWaveform *original = s_hl_state.waveform;
    s_hl_state.waveform = &s_fast_gl16_waveform;

    epd_poweron();
    enum EpdDrawError err = epd_hl_update_screen(&s_hl_state, MODE_GL16, 25);
    epd_poweroff();

    /* 恢复原始波形 */
    s_hl_state.waveform = original;

    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Fast GL16 update failed: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t epd_driver_update_area_fast_gl16(int x, int y, int w, int h) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    EpdRect area = {.x = x, .y = y, .width = w, .height = h};

    const EpdWaveform *original = s_hl_state.waveform;
    s_hl_state.waveform = &s_fast_gl16_waveform;

    epd_poweron();
    enum EpdDrawError err = epd_hl_update_area(&s_hl_state, MODE_GL16, 25, area);
    epd_poweroff();

    s_hl_state.waveform = original;

    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Fast GL16 area update failed: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

void epd_driver_set_all_white(void) {
    if (s_initialized) {
        epd_hl_set_all_white(&s_hl_state);
    }
}

void epd_driver_set_fast_gl16_times(const int times[FAST_GL16_PHASES]) {
    memcpy(s_fast_gl16_times, times, sizeof(s_fast_gl16_times));
    /* LUT data 不依赖 phase_times，无需重建 */
}

const int* epd_driver_get_fast_gl16_times(void) {
    return s_fast_gl16_times;
}

esp_err_t epd_driver_white_du_then_gl16(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    int fb_size = epd_width() / 2 * epd_height();

    /* 保存目标帧内容 */
    uint8_t *target = (uint8_t *)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
    if (target == NULL) {
        ESP_LOGE(TAG, "Failed to allocate temp buffer for white DU→GL16");
        return ESP_ERR_NO_MEM;
    }
    memcpy(target, s_framebuffer, fb_size);

    /* 第一步：DU 快速刷白 */
    memset(s_framebuffer, 0xFF, fb_size);
    epd_poweron();
    enum EpdDrawError err = epd_hl_update_screen(&s_hl_state, MODE_DU, 25);
    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "White DU step failed: %d", err);
        memcpy(s_framebuffer, target, fb_size);
        heap_caps_free(target);
        epd_poweroff();
        return ESP_FAIL;
    }

    /* 第二步：GL16 显示目标内容 */
    memcpy(s_framebuffer, target, fb_size);
    err = epd_hl_update_screen(&s_hl_state, MODE_GL16, 25);
    epd_poweroff();

    heap_caps_free(target);

    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "GL16 target step failed: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}
