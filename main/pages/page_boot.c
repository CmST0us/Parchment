/**
 * @file page_boot.c
 * @brief 启动画面页面实现。
 *
 * 居中显示品牌标识（Logo 占位、标题、副标题）和初始化状态文字，
 * 2 秒后通过 FreeRTOS timer 自动跳转到下一页面。
 */

#include "page_boot.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "esp_log.h"

#include "ui_canvas.h"
#include "ui_event.h"
#include "ui_font.h"
#include "ui_render.h"

static const char *TAG = "page_boot";

/** 自动跳转 timer 句柄。 */
static TimerHandle_t s_jump_timer = NULL;

/* ---- Timer callback ---- */

/** Timer 到期回调：发送 UI_EVENT_TIMER 到事件队列。 */
static void boot_timer_cb(TimerHandle_t timer) {
    (void)timer;
    ui_event_t ev = { .type = UI_EVENT_TIMER };
    ui_event_send(&ev);
}

/* ---- 页面回调 ---- */

/** 进入启动画面：创建 2 秒 one-shot timer。 */
static void boot_on_enter(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "Boot screen entered");

    s_jump_timer = xTimerCreate("boot_jump", pdMS_TO_TICKS(2000),
                                pdFALSE, NULL, boot_timer_cb);
    if (s_jump_timer != NULL) {
        xTimerStart(s_jump_timer, 0);
    }
    ui_render_mark_full_dirty();
}

/** 退出启动画面：清理未到期的 timer。 */
static void boot_on_exit(void) {
    if (s_jump_timer != NULL) {
        xTimerStop(s_jump_timer, 0);
        xTimerDelete(s_jump_timer, 0);
        s_jump_timer = NULL;
    }
}

/** 事件处理：收到 timer 事件后跳转。 */
static void boot_on_event(ui_event_t *event) {
    if (event->type == UI_EVENT_TIMER) {
        ESP_LOGI(TAG, "Timer fired, navigating to next page");
        /* Library 页面未实现前，暂跳回自身。 */
        ui_page_replace(&page_boot, NULL);
    }
}

/** 渲染启动画面。 */
static void boot_on_render(uint8_t *fb) {
    ui_canvas_fill(fb, UI_COLOR_WHITE);

    /* 布局垂直居中区域: Logo(120) + gap(16) + title(36) + gap(8)
       + subtitle(20) + gap(24) + status(20) = ~244px 总高
       起始 y ≈ (960 - 244) / 2 = 358 */
    const int center_x = UI_SCREEN_WIDTH / 2;
    int y = 358;

    /* Logo 占位: 居中 120×120 矩形边框 */
    const int logo_size = 120;
    int logo_x = center_x - logo_size / 2;
    ui_canvas_draw_rect(fb, logo_x, y, logo_size, logo_size,
                        UI_COLOR_DARK, 2);
    /* Logo 区域内绘制大写 "P" 作为占位 */
    const EpdFont *logo_font = ui_font_get(40);
    int p_w = ui_canvas_measure_text(logo_font, "P");
    ui_canvas_draw_text(fb, center_x - p_w / 2, y + 76, logo_font,
                        "P", UI_COLOR_DARK);
    y += logo_size + 16;

    /* 标题: "Parchment" — 36px, BLACK, 居中 */
    const EpdFont *title_font = ui_font_get(36);
    const char *title = "Parchment";
    int title_w = ui_canvas_measure_text(title_font, title);
    ui_canvas_draw_text(fb, center_x - title_w / 2, y + 30,
                        title_font, title, UI_COLOR_BLACK);
    y += 36 + 8;

    /* 副标题: "E-Ink Reader" — 20px (最接近 18px), DARK, 居中 */
    const EpdFont *sub_font = ui_font_get(20);
    const char *subtitle = "E-Ink Reader";
    int sub_w = ui_canvas_measure_text(sub_font, subtitle);
    ui_canvas_draw_text(fb, center_x - sub_w / 2, y + 16,
                        sub_font, subtitle, UI_COLOR_DARK);
    y += 20 + 24;

    /* 状态: "Initializing..." — 20px (最接近 14px), MEDIUM, 居中 */
    const char *status = "Initializing...";
    int status_w = ui_canvas_measure_text(sub_font, status);
    ui_canvas_draw_text(fb, center_x - status_w / 2, y + 16,
                        sub_font, status, UI_COLOR_MEDIUM);
}

/** 启动画面页面实例。 */
ui_page_t page_boot = {
    .name = "boot",
    .on_enter = boot_on_enter,
    .on_exit = boot_on_exit,
    .on_event = boot_on_event,
    .on_render = boot_on_render,
};
