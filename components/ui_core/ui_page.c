/**
 * @file ui_page.c
 * @brief 页面栈管理实现。
 */

#include "ui_page.h"
#include "esp_log.h"

static const char *TAG = "ui_page";

/** 页面栈最大深度。 */
#define UI_PAGE_STACK_MAX 8

/** 页面栈。 */
static ui_page_t *s_stack[UI_PAGE_STACK_MAX];

/** 栈顶索引（-1 表示空栈）。 */
static int s_top = -1;

void ui_page_stack_init(void) {
    s_top = -1;
    for (int i = 0; i < UI_PAGE_STACK_MAX; i++) {
        s_stack[i] = NULL;
    }
    ESP_LOGI(TAG, "Page stack initialized (max depth: %d)", UI_PAGE_STACK_MAX);
}

void ui_page_push(ui_page_t *page, void *arg) {
    if (page == NULL) {
        ESP_LOGE(TAG, "Cannot push NULL page");
        return;
    }

    if (s_top >= UI_PAGE_STACK_MAX - 1) {
        ESP_LOGE(TAG, "Page stack full, cannot push '%s'", page->name ? page->name : "?");
        return;
    }

    /* 调用当前页面的 on_exit */
    if (s_top >= 0 && s_stack[s_top] != NULL && s_stack[s_top]->on_exit != NULL) {
        s_stack[s_top]->on_exit();
    }

    /* 入栈 */
    s_top++;
    s_stack[s_top] = page;

    ESP_LOGI(TAG, "Push page '%s' (depth: %d)", page->name ? page->name : "?", s_top + 1);

    /* 调用新页面的 on_enter */
    if (page->on_enter != NULL) {
        page->on_enter(arg);
    }
}

void ui_page_pop(void) {
    if (s_top <= 0) {
        ESP_LOGW(TAG, "Cannot pop: stack has %d page(s)", s_top + 1);
        return;
    }

    /* 调用栈顶页面的 on_exit */
    ui_page_t *current = s_stack[s_top];
    if (current != NULL && current->on_exit != NULL) {
        current->on_exit();
    }

    /* 出栈 */
    s_stack[s_top] = NULL;
    s_top--;

    ESP_LOGI(TAG, "Pop to page '%s' (depth: %d)",
             s_stack[s_top]->name ? s_stack[s_top]->name : "?", s_top + 1);

    /* 调用前一页面的 on_enter */
    if (s_stack[s_top] != NULL && s_stack[s_top]->on_enter != NULL) {
        s_stack[s_top]->on_enter(NULL);
    }
}

void ui_page_replace(ui_page_t *page, void *arg) {
    if (page == NULL) {
        ESP_LOGE(TAG, "Cannot replace with NULL page");
        return;
    }

    if (s_top < 0) {
        /* 栈空时等同于 push */
        ui_page_push(page, arg);
        return;
    }

    /* 调用当前页面的 on_exit */
    ui_page_t *current = s_stack[s_top];
    if (current != NULL && current->on_exit != NULL) {
        current->on_exit();
    }

    /* 替换栈顶 */
    s_stack[s_top] = page;

    ESP_LOGI(TAG, "Replace with page '%s' (depth: %d)", page->name ? page->name : "?", s_top + 1);

    /* 调用新页面的 on_enter */
    if (page->on_enter != NULL) {
        page->on_enter(arg);
    }
}

ui_page_t *ui_page_current(void) {
    if (s_top < 0) {
        return NULL;
    }
    return s_stack[s_top];
}
