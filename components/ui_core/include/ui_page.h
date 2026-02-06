/**
 * @file ui_page.h
 * @brief 页面栈管理。
 *
 * 定义页面接口和栈导航操作（push/pop/replace）。
 * 每个页面实现 on_enter/on_exit/on_event/on_render 四个回调。
 */

#ifndef UI_PAGE_H
#define UI_PAGE_H

#include <stdint.h>
#include "esp_err.h"
#include "ui_event.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 页面结构体。回调字段允许为 NULL。 */
typedef struct ui_page {
    const char *name;                    /**< 页面名称 */
    void (*on_enter)(void *arg);         /**< 进入页面时调用 */
    void (*on_exit)(void);               /**< 离开页面时调用 */
    void (*on_event)(ui_event_t *event); /**< 处理事件 */
    void (*on_render)(uint8_t *fb);      /**< 渲染到 framebuffer */
} ui_page_t;

/**
 * @brief 初始化页面栈。
 *
 * 清空页面栈，最大深度 8 层。
 */
void ui_page_stack_init(void);

/**
 * @brief 将页面压入栈顶。
 *
 * 调用当前页面的 on_exit，然后调用新页面的 on_enter(arg)。
 * 栈满时不执行操作并记录错误日志。
 *
 * @param page 页面指针。
 * @param arg  传递给 on_enter 的参数。
 */
void ui_page_push(ui_page_t *page, void *arg);

/**
 * @brief 弹出栈顶页面。
 *
 * 调用栈顶页面的 on_exit，然后调用前一页面的 on_enter(NULL)。
 * 栈中只有一个页面时不执行操作并记录警告日志。
 */
void ui_page_pop(void);

/**
 * @brief 替换栈顶页面。
 *
 * 等效于 pop + push，但不影响栈深度。
 *
 * @param page 新页面指针。
 * @param arg  传递给 on_enter 的参数。
 */
void ui_page_replace(ui_page_t *page, void *arg);

/**
 * @brief 获取当前栈顶页面。
 *
 * @return 栈顶页面指针，栈空时返回 NULL。
 */
ui_page_t *ui_page_current(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_PAGE_H */
