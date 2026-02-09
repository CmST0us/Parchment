/**
 * @file ui_render.c
 * @brief 渲染管线实现：脏区域追踪和按需刷新。
 */

#include "ui_render.h"
#include "ui_canvas.h"
#include "epd_driver.h"
#include "esp_log.h"

static const char *TAG = "ui_render";

/** 物理 framebuffer 高度（横屏）。 */
#define FB_PHYS_HEIGHT 540

/** 屏幕总面积（逻辑坐标）。 */
#define SCREEN_AREA (UI_SCREEN_WIDTH * UI_SCREEN_HEIGHT)

/** 全屏刷新阈值：脏区域面积超过此比例时退化为全屏刷新。 */
#define FULL_REFRESH_THRESHOLD_PCT 60

/** 脏区域状态。 */
static struct {
    int x0, y0, x1, y1;  /**< 包围矩形（逻辑坐标，左上-右下） */
    bool dirty;           /**< 是否有脏区域 */
    bool full_refresh;    /**< 强制全屏刷新 */
} s_dirty;

void ui_render_init(void) {
    s_dirty.dirty = false;
    s_dirty.full_refresh = false;
    s_dirty.x0 = 0;
    s_dirty.y0 = 0;
    s_dirty.x1 = 0;
    s_dirty.y1 = 0;
    ESP_LOGI(TAG, "Render pipeline initialized");
}

void ui_render_mark_dirty(int x, int y, int w, int h) {
    if (w <= 0 || h <= 0) {
        return;
    }

    int nx0 = x;
    int ny0 = y;
    int nx1 = x + w;
    int ny1 = y + h;

    /* 裁剪到屏幕范围 */
    if (nx0 < 0) nx0 = 0;
    if (ny0 < 0) ny0 = 0;
    if (nx1 > UI_SCREEN_WIDTH) nx1 = UI_SCREEN_WIDTH;
    if (ny1 > UI_SCREEN_HEIGHT) ny1 = UI_SCREEN_HEIGHT;

    if (nx0 >= nx1 || ny0 >= ny1) {
        return;
    }

    if (!s_dirty.dirty) {
        s_dirty.x0 = nx0;
        s_dirty.y0 = ny0;
        s_dirty.x1 = nx1;
        s_dirty.y1 = ny1;
        s_dirty.dirty = true;
    } else {
        /* 合并为 bounding box */
        if (nx0 < s_dirty.x0) s_dirty.x0 = nx0;
        if (ny0 < s_dirty.y0) s_dirty.y0 = ny0;
        if (nx1 > s_dirty.x1) s_dirty.x1 = nx1;
        if (ny1 > s_dirty.y1) s_dirty.y1 = ny1;
    }
}

void ui_render_mark_full_dirty(void) {
    s_dirty.x0 = 0;
    s_dirty.y0 = 0;
    s_dirty.x1 = UI_SCREEN_WIDTH;
    s_dirty.y1 = UI_SCREEN_HEIGHT;
    s_dirty.dirty = true;
    s_dirty.full_refresh = true;
}

bool ui_render_is_dirty(void) {
    return s_dirty.dirty;
}

void ui_render_flush(void) {
    if (!s_dirty.dirty) {
        return;
    }

    int w = s_dirty.x1 - s_dirty.x0;
    int h = s_dirty.y1 - s_dirty.y0;
    int area = w * h;
    int threshold = SCREEN_AREA * FULL_REFRESH_THRESHOLD_PCT / 100;

    if (s_dirty.full_refresh || area >= threshold) {
        ESP_LOGI(TAG, "Full screen refresh");
        epd_driver_update_screen();
    } else {
        /* 逻辑坐标 → 物理坐标：转置 + X翻转 */
        int phys_x = s_dirty.y0;
        int phys_y = FB_PHYS_HEIGHT - s_dirty.x1;
        int phys_w = h;
        int phys_h = w;
        ESP_LOGI(TAG, "Partial refresh: logical(%d,%d %dx%d) -> phys(%d,%d %dx%d)",
                 s_dirty.x0, s_dirty.y0, w, h, phys_x, phys_y, phys_w, phys_h);
        epd_driver_update_area(phys_x, phys_y, phys_w, phys_h);
    }

    /* 清除脏区域状态 */
    s_dirty.dirty = false;
    s_dirty.full_refresh = false;
}
