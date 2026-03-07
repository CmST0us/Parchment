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

/* ── 文字模式波形 (参考 M5GFX Panel_EPD lut_eraser + lut_text) ──
 *
 * Phase 0-1:  消去 — 将当前像素向中灰偏移（减少残影）
 * Phase 2-13: 先白后涂 — 渐变刷白，再逐步涂至目标灰度
 *
 * 总计 14 相位，LCD 模式每相位约 8ms，总计约 112ms。
 * 支持 16 级灰度，无闪烁。
 */
#define TEXT_MODE_PHASES 14

/* M5GFX lut_eraser: 动作取决于当前像素灰度 (from)
 * 索引: 0=黑 ~ 15=白, 值: 0=noop 1=darken 2=lighten */
static const uint8_t s_eraser_lut[2][16] = {
    {2,2,2,2,2,2,2,2,2,2,2,0,0,0,1,1},
    {2,2,0,0,0,1,1,1,1,1,1,1,1,1,1,1},
};

/* M5GFX lut_text: 动作取决于目标像素灰度 (to)
 * 索引: 0=黑 ~ 15=白
 * M5GFX 编码 1=darken 2=lighten 3=noop → epdiy 1=darken 2=lighten 0=noop */
static const uint8_t s_text_lut[12][16] = {
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {2,2,2,2,2,2,2,2,2,2,2,1,2,2,2,1},
    {2,2,2,2,1,2,2,2,2,2,2,1,2,2,2,1},
    {1,2,2,1,1,1,1,1,0,0,1,1,0,0,1,2},
    {1,0,0,1,1,1,1,0,0,1,1,1,1,0,1,2},
    {1,0,0,1,2,2,1,1,1,1,2,1,1,1,1,2},
    {0,1,0,2,2,2,1,1,1,2,2,1,1,1,2,0},
    {1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2},
    {1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2},
    {1,1,1,1,1,1,1,1,1,1,1,0,0,0,0,2},
};

static const EpdWaveformTempInterval s_text_mode_temp[1] = {
    { .min = 0, .max = 50 }
};
static uint8_t s_text_mode_data[TEXT_MODE_PHASES][16][4];
static EpdWaveformPhases s_text_mode_phases;
static const EpdWaveformPhases *s_text_mode_ranges[1];
static EpdWaveformMode s_text_mode_mode;
static const EpdWaveformMode *s_text_mode_modes[1];
static EpdWaveform s_text_mode_waveform;

static void init_text_mode_waveform(void) {
    /* Phase 0-1: Eraser — 动作取决于 from (当前灰度)
     * from==to 时 noop，避免未变化像素闪烁 */
    for (int p = 0; p < 2; p++) {
        for (int t = 0; t < 16; t++) {
            for (int bi = 0; bi < 4; bi++) {
                uint8_t byte_val = 0;
                for (int fi = 0; fi < 4; fi++) {
                    int f = bi * 4 + fi;
                    uint8_t action = (f == t) ? 0 : s_eraser_lut[p][f];
                    byte_val |= (action << (6 - 2 * fi));
                }
                s_text_mode_data[p][t][bi] = byte_val;
            }
        }
    }

    /* Phase 2-13: Text — 动作取决于 to (目标灰度)
     * from==to 时 noop，只驱动实际变化的像素 */
    for (int p = 0; p < 12; p++) {
        for (int t = 0; t < 16; t++) {
            uint8_t action = s_text_lut[p][t];
            for (int bi = 0; bi < 4; bi++) {
                uint8_t byte_val = 0;
                for (int fi = 0; fi < 4; fi++) {
                    int f = bi * 4 + fi;
                    uint8_t a = (f == t) ? 0 : action;
                    byte_val |= (a << (6 - 2 * fi));
                }
                s_text_mode_data[2 + p][t][bi] = byte_val;
            }
        }
    }

    /* 组装波形结构体 */
    s_text_mode_phases.phases = TEXT_MODE_PHASES;
    s_text_mode_phases.phase_times = NULL;
    s_text_mode_phases.luts = (const uint8_t *)s_text_mode_data;

    s_text_mode_ranges[0] = &s_text_mode_phases;

    s_text_mode_mode.type = 5;  /* MODE_GL16 */
    s_text_mode_mode.temp_ranges = 1;
    s_text_mode_mode.range_data = s_text_mode_ranges;

    s_text_mode_modes[0] = &s_text_mode_mode;

    s_text_mode_waveform.num_modes = 1;
    s_text_mode_waveform.num_temp_ranges = 1;
    s_text_mode_waveform.mode_data = s_text_mode_modes;
    s_text_mode_waveform.temp_intervals = s_text_mode_temp;
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

    /* 生成文字模式波形 LUT */
    init_text_mode_waveform();

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

void epd_driver_set_all_white(void) {
    if (s_initialized) {
        epd_hl_set_all_white(&s_hl_state);
    }
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

esp_err_t epd_driver_white_black_du_then_gl16(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    int fb_size = epd_width() / 2 * epd_height();

    /* 保存目标帧内容 */
    uint8_t *target = (uint8_t *)heap_caps_malloc(fb_size, MALLOC_CAP_SPIRAM);
    if (target == NULL) {
        ESP_LOGE(TAG, "Failed to allocate temp buffer for W>B>GL16");
        return ESP_ERR_NO_MEM;
    }
    memcpy(target, s_framebuffer, fb_size);

    epd_poweron();

    /* 第一步：DU 快速刷白 */
    memset(s_framebuffer, 0xFF, fb_size);
    enum EpdDrawError err = epd_hl_update_screen(&s_hl_state, MODE_DU, 25);
    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "White DU step failed: %d", err);
        memcpy(s_framebuffer, target, fb_size);
        heap_caps_free(target);
        epd_poweroff();
        return ESP_FAIL;
    }

    /* 第二步：DU 快速刷黑 */
    memset(s_framebuffer, 0x00, fb_size);
    err = epd_hl_update_screen(&s_hl_state, MODE_DU, 25);
    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Black DU step failed: %d", err);
        memcpy(s_framebuffer, target, fb_size);
        heap_caps_free(target);
        epd_poweroff();
        return ESP_FAIL;
    }

    /* 第三步：GL16 显示目标内容 */
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

esp_err_t epd_driver_update_screen_text_mode(void) {
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    /* 临时切换到文字模式波形 */
    const EpdWaveform *original = s_hl_state.waveform;
    s_hl_state.waveform = &s_text_mode_waveform;

    epd_poweron();
    enum EpdDrawError err = epd_hl_update_screen(&s_hl_state, MODE_GL16, 25);
    epd_poweroff();

    /* 恢复原始波形 */
    s_hl_state.waveform = original;

    if (err != EPD_DRAW_SUCCESS) {
        ESP_LOGE(TAG, "Text mode update failed: %d", err);
        return ESP_FAIL;
    }
    return ESP_OK;
}
