/**
 * @file main.c
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 初始化硬件和 UI 框架，推入交互测试页面，启动事件循环。
 * 测试页面包含：
 *   - 标题文字 (TrueType 渲染)
 *   - 6 个 Toggle 按钮 (TAP)，带文字标签
 *   - 5 点翻页指示器 (SWIPE LEFT/RIGHT)
 *   - 进度条 (SWIPE UP/DOWN)
 *   - 文本换行渲染示例区域
 *   - 长按全部重置 (LONG PRESS)
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board.h"
#include "epd_driver.h"
#include "gt911.h"
#include "sd_storage.h"
#include "ui_core.h"
#include "ui_text.h"

static const char *TAG = "parchment";

/* ========================================================================== */
/*  交互测试页面                                                                */
/* ========================================================================== */

/* ---- 字体 ---- */

/** 字体搜索路径列表（按优先级）。 */
static const char *s_font_search_paths[] = {
    "/sdcard/font.ttf",                                                  /* ESP32 SD 卡 */
    "/usr/share/fonts/truetype/fonts-japanese-gothic.ttf",               /* Linux CJK */
    "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",   /* Linux */
    "/usr/share/fonts/truetype/freefont/FreeSans.ttf",                   /* Linux */
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",                   /* Linux */
    "/System/Library/Fonts/PingFang.ttc",                                /* macOS CJK */
    "/System/Library/Fonts/Helvetica.ttc",                               /* macOS */
    "font.ttf",                                                          /* 当前目录 */
    NULL,
};

static ui_font_t *s_font_title = NULL;  /**< 标题字体 (32px) */
static ui_font_t *s_font_body  = NULL;  /**< 正文字体 (22px) */
static ui_font_t *s_font_btn   = NULL;  /**< 按钮标签字体 (18px) */

/* ---- 布局常量 ---- */

/* 按钮网格：2列×3行 */
#define BTN_COLS        2
#define BTN_ROWS        3
#define BTN_COUNT       (BTN_COLS * BTN_ROWS)
#define BTN_W           220
#define BTN_H           160
#define BTN_GAP         30
#define BTN_GRID_W      (BTN_COLS * BTN_W + (BTN_COLS - 1) * BTN_GAP)
#define BTN_GRID_X      ((UI_SCREEN_WIDTH - BTN_GRID_W) / 2)
#define BTN_GRID_Y      80
#define BTN_BORDER      3

/* 翻页指示器：5个圆点 */
#define PAGE_COUNT      5
#define DOT_RADIUS      10
#define DOT_GAP         30
#define DOT_ROW_W       (PAGE_COUNT * DOT_RADIUS * 2 + (PAGE_COUNT - 1) * DOT_GAP)
#define DOT_ROW_X       ((UI_SCREEN_WIDTH - DOT_ROW_W) / 2)
#define DOT_ROW_Y       680
#define DOT_REGION_H    (DOT_RADIUS * 2 + 10)

/* 进度条 */
#define BAR_W           420
#define BAR_H           30
#define BAR_X           ((UI_SCREEN_WIDTH - BAR_W) / 2)
#define BAR_Y           780
#define BAR_BORDER      2
#define LEVEL_MAX       10
#define BAR_REGION_H    (BAR_H + 10)

/* 文本渲染示例区域 */
#define TEXT_DEMO_X     30
#define TEXT_DEMO_Y     840
#define TEXT_DEMO_W     (UI_SCREEN_WIDTH - 60)
#define TEXT_DEMO_H     100

/* ---- 状态变量 ---- */
static bool s_buttons[BTN_COUNT];
static int  s_page;
static int  s_level;
static bool s_full_render;

/* ---- 辅助：绘制填充圆 ---- */

/**
 * @brief 绘制填充圆（Bresenham 中点圆算法）。
 */
static void draw_filled_circle(uint8_t *fb, int cx, int cy, int r, uint8_t color) {
    int x = 0, y = r;
    int d = 1 - r;
    while (x <= y) {
        ui_canvas_draw_hline(fb, cx - y, cy + x, 2 * y + 1, color);
        ui_canvas_draw_hline(fb, cx - y, cy - x, 2 * y + 1, color);
        ui_canvas_draw_hline(fb, cx - x, cy + y, 2 * x + 1, color);
        ui_canvas_draw_hline(fb, cx - x, cy - y, 2 * x + 1, color);
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

/**
 * @brief 绘制空心圆（Bresenham 中点圆算法）。
 */
static void draw_circle_outline(uint8_t *fb, int cx, int cy, int r, uint8_t color) {
    int x = 0, y = r;
    int d = 1 - r;
    while (x <= y) {
        ui_canvas_draw_pixel(fb, cx + x, cy + y, color);
        ui_canvas_draw_pixel(fb, cx - x, cy + y, color);
        ui_canvas_draw_pixel(fb, cx + x, cy - y, color);
        ui_canvas_draw_pixel(fb, cx - x, cy - y, color);
        ui_canvas_draw_pixel(fb, cx + y, cy + x, color);
        ui_canvas_draw_pixel(fb, cx - y, cy + x, color);
        ui_canvas_draw_pixel(fb, cx + y, cy - x, color);
        ui_canvas_draw_pixel(fb, cx - y, cy - x, color);
        if (d < 0) {
            d += 2 * x + 3;
        } else {
            d += 2 * (x - y) + 5;
            y--;
        }
        x++;
    }
}

/* ---- 辅助：按钮区域计算 ---- */

static void btn_rect(int idx, int *x, int *y, int *w, int *h) {
    int col = idx % BTN_COLS;
    int row = idx / BTN_COLS;
    *x = BTN_GRID_X + col * (BTN_W + BTN_GAP);
    *y = BTN_GRID_Y + row * (BTN_H + BTN_GAP);
    *w = BTN_W;
    *h = BTN_H;
}

/**
 * @brief 命中测试：返回点击到的按钮索引，-1 表示未命中。
 */
static int btn_hit_test(int tx, int ty) {
    for (int i = 0; i < BTN_COUNT; i++) {
        int bx, by, bw, bh;
        btn_rect(i, &bx, &by, &bw, &bh);
        if (tx >= bx && tx < bx + bw && ty >= by && ty < by + bh) {
            return i;
        }
    }
    return -1;
}

/* ---- 绘制单个按钮 ---- */

static void draw_button(uint8_t *fb, int idx) {
    int bx, by, bw, bh;
    btn_rect(idx, &bx, &by, &bw, &bh);

    /* 先清除区域 */
    ui_canvas_fill_rect(fb, bx, by, bw, bh, UI_COLOR_WHITE);

    if (s_buttons[idx]) {
        /* ON: 深灰填充 */
        ui_canvas_fill_rect(fb, bx, by, bw, bh, UI_COLOR_DARK);
        /* 内部画一个较浅的边框以示区分 */
        ui_canvas_draw_rect(fb, bx, by, bw, bh, UI_COLOR_BLACK, BTN_BORDER);
    } else {
        /* OFF: 白底黑框 */
        ui_canvas_draw_rect(fb, bx, by, bw, bh, UI_COLOR_BLACK, BTN_BORDER);
    }

    /* 按钮标签 */
    uint8_t label_color = s_buttons[idx] ? UI_COLOR_WHITE : UI_COLOR_BLACK;
    if (s_font_btn) {
        /* 使用文字标签 */
        char label[16];
        snprintf(label, sizeof(label), "BTN %d", idx + 1);
        int tw = ui_text_measure_width(s_font_btn, label);
        int tx = bx + (bw - tw) / 2;
        int ty = by + (bh - ui_font_line_height(s_font_btn)) / 2;
        ui_text_draw_line(fb, s_font_btn, tx, ty, label, label_color);
    } else {
        /* 无字体时回退：画菱形标识 */
        int cx = bx + bw / 2;
        int cy = by + bh / 2;
        int mark = 12 + idx * 4;
        for (int i = 0; i <= mark; i++) {
            int half = (i * mark) / mark;
            if (half > 0) {
                ui_canvas_draw_hline(fb, cx - half, cy - mark + i, 2 * half + 1, label_color);
                ui_canvas_draw_hline(fb, cx - half, cy + mark - i, 2 * half + 1, label_color);
            }
        }
    }
}

/* ---- 绘制翻页指示器 ---- */

static void draw_page_dots(uint8_t *fb) {
    /* 清除指示器区域 */
    ui_canvas_fill_rect(fb, DOT_ROW_X - 5, DOT_ROW_Y - DOT_RADIUS - 5,
                        DOT_ROW_W + 10, DOT_REGION_H, UI_COLOR_WHITE);

    for (int i = 0; i < PAGE_COUNT; i++) {
        int cx = DOT_ROW_X + DOT_RADIUS + i * (DOT_RADIUS * 2 + DOT_GAP);
        int cy = DOT_ROW_Y;
        if (i == s_page) {
            draw_filled_circle(fb, cx, cy, DOT_RADIUS, UI_COLOR_BLACK);
        } else {
            /* 空心圆：画多圈形成粗边框 */
            draw_circle_outline(fb, cx, cy, DOT_RADIUS, UI_COLOR_BLACK);
            draw_circle_outline(fb, cx, cy, DOT_RADIUS - 1, UI_COLOR_BLACK);
        }
    }
}

/* ---- 绘制进度条 ---- */

static void draw_progress_bar(uint8_t *fb) {
    /* 清除进度条区域 */
    ui_canvas_fill_rect(fb, BAR_X - 5, BAR_Y - 5,
                        BAR_W + 10, BAR_REGION_H, UI_COLOR_WHITE);

    /* 外框 */
    ui_canvas_draw_rect(fb, BAR_X, BAR_Y, BAR_W, BAR_H, UI_COLOR_BLACK, BAR_BORDER);

    /* 填充部分 */
    if (s_level > 0) {
        int inner_x = BAR_X + BAR_BORDER;
        int inner_y = BAR_Y + BAR_BORDER;
        int inner_max_w = BAR_W - 2 * BAR_BORDER;
        int inner_h = BAR_H - 2 * BAR_BORDER;
        int fill_w = (inner_max_w * s_level) / LEVEL_MAX;
        ui_canvas_fill_rect(fb, inner_x, inner_y, fill_w, inner_h, UI_COLOR_DARK);
    }

    /* 刻度标记 */
    for (int i = 1; i < LEVEL_MAX; i++) {
        int tx = BAR_X + (BAR_W * i) / LEVEL_MAX;
        ui_canvas_draw_vline(fb, tx, BAR_Y + BAR_H + 2, 6, UI_COLOR_MEDIUM);
    }
}

/* ---- 绘制全部元素 ---- */

static void draw_text_demo(uint8_t *fb) {
    if (!s_font_body) return;

    ui_canvas_fill_rect(fb, TEXT_DEMO_X, TEXT_DEMO_Y, TEXT_DEMO_W, TEXT_DEMO_H, UI_COLOR_WHITE);

    ui_text_draw_wrapped(fb, s_font_body,
                         TEXT_DEMO_X, TEXT_DEMO_Y, TEXT_DEMO_W, TEXT_DEMO_H,
                         "Parchment is an E-Ink e-reader built on ESP32-S3. "
                         "It features a 4.7\" display with 16-level grayscale, "
                         "touch input, and this text rendering engine with "
                         "anti-aliased TrueType fonts and automatic line wrapping.",
                         UI_COLOR_BLACK, UI_TEXT_ALIGN_LEFT, 2);
}

static void draw_all(uint8_t *fb) {
    ui_canvas_fill(fb, UI_COLOR_WHITE);

    /* 标题文字 */
    if (s_font_title) {
        const char *title = "Parchment";
        int tw = ui_text_measure_width(s_font_title, title);
        int tx = (UI_SCREEN_WIDTH - tw) / 2;
        ui_text_draw_line(fb, s_font_title, tx, 16, title, UI_COLOR_BLACK);
    }

    /* 顶部分隔线 */
    ui_canvas_draw_hline(fb, 30, 60, UI_SCREEN_WIDTH - 60, UI_COLOR_MEDIUM);

    /* 按钮 */
    for (int i = 0; i < BTN_COUNT; i++) {
        draw_button(fb, i);
    }

    /* 按钮区与指示器之间分隔线 */
    int sep_y = BTN_GRID_Y + BTN_ROWS * (BTN_H + BTN_GAP) - BTN_GAP + 20;
    ui_canvas_draw_hline(fb, 30, sep_y, UI_SCREEN_WIDTH - 60, UI_COLOR_MEDIUM);

    /* 翻页指示器 */
    draw_page_dots(fb);

    /* 指示器与进度条之间分隔线 */
    ui_canvas_draw_hline(fb, 30, 740, UI_SCREEN_WIDTH - 60, UI_COLOR_MEDIUM);

    /* 进度条 */
    draw_progress_bar(fb);

    /* 进度条与文本区域之间分隔线 */
    ui_canvas_draw_hline(fb, 30, 830, UI_SCREEN_WIDTH - 60, UI_COLOR_MEDIUM);

    /* 文本渲染示例区域 */
    draw_text_demo(fb);
}

/* ---- 页面回调 ---- */

static void test_page_on_enter(void *arg) {
    ESP_LOGI(TAG, "Interactive test page entered");

    /* 重置状态 */
    memset(s_buttons, 0, sizeof(s_buttons));
    s_page = 0;
    s_level = 0;
    s_full_render = true;

    ui_render_mark_full_dirty();
}

static void test_page_on_event(ui_event_t *event) {
    switch (event->type) {

    case UI_EVENT_TOUCH_TAP: {
        int idx = btn_hit_test(event->x, event->y);
        if (idx >= 0) {
            s_buttons[idx] = !s_buttons[idx];
            ESP_LOGI(TAG, "Button %d → %s", idx, s_buttons[idx] ? "ON" : "OFF");
            /* 仅标记该按钮区域为脏 */
            int bx, by, bw, bh;
            btn_rect(idx, &bx, &by, &bw, &bh);
            ui_render_mark_dirty(bx, by, bw, bh);
        }
        break;
    }

    case UI_EVENT_TOUCH_SWIPE_LEFT:
        if (s_page < PAGE_COUNT - 1) {
            s_page++;
            ESP_LOGI(TAG, "Page → %d", s_page);
            ui_render_mark_dirty(DOT_ROW_X - 5, DOT_ROW_Y - DOT_RADIUS - 5,
                                 DOT_ROW_W + 10, DOT_REGION_H);
        }
        break;

    case UI_EVENT_TOUCH_SWIPE_RIGHT:
        if (s_page > 0) {
            s_page--;
            ESP_LOGI(TAG, "Page → %d", s_page);
            ui_render_mark_dirty(DOT_ROW_X - 5, DOT_ROW_Y - DOT_RADIUS - 5,
                                 DOT_ROW_W + 10, DOT_REGION_H);
        }
        break;

    case UI_EVENT_TOUCH_SWIPE_UP:
        if (s_level < LEVEL_MAX) {
            s_level++;
            ESP_LOGI(TAG, "Level → %d", s_level);
            ui_render_mark_dirty(BAR_X - 5, BAR_Y - 5, BAR_W + 10, BAR_REGION_H);
        }
        break;

    case UI_EVENT_TOUCH_SWIPE_DOWN:
        if (s_level > 0) {
            s_level--;
            ESP_LOGI(TAG, "Level → %d", s_level);
            ui_render_mark_dirty(BAR_X - 5, BAR_Y - 5, BAR_W + 10, BAR_REGION_H);
        }
        break;

    case UI_EVENT_TOUCH_LONG_PRESS:
        ESP_LOGI(TAG, "Long press → RESET ALL");
        memset(s_buttons, 0, sizeof(s_buttons));
        s_page = 0;
        s_level = 0;
        s_full_render = true;
        ui_render_mark_full_dirty();
        break;

    default:
        break;
    }
}

static void test_page_on_render(uint8_t *fb) {
    if (s_full_render) {
        s_full_render = false;
        draw_all(fb);
        return;
    }

    /*
     * 局部刷新：只重绘脏区域涉及的元素。
     * 由于 on_event 中精确标记了变化区域，这里根据最近操作类型重绘。
     * 简化策略：各绘制函数内部先清除自己区域再重绘，互不干扰。
     */

    /* 按钮：检查每个按钮是否需要重绘（由脏区域系统合并处理） */
    for (int i = 0; i < BTN_COUNT; i++) {
        draw_button(fb, i);
    }

    /* 翻页指示器 */
    draw_page_dots(fb);

    /* 进度条 */
    draw_progress_bar(fb);
}

/** 测试页面定义。 */
static ui_page_t test_page = {
    .name = "interactive_test",
    .on_enter = test_page_on_enter,
    .on_exit = NULL,
    .on_event = test_page_on_event,
    .on_render = test_page_on_render,
};

/* ========================================================================== */
/*  app_main                                                                   */
/* ========================================================================== */

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.3");
    ESP_LOGI(TAG, "  M5Stack PaperS3 (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    /* 1. 板级初始化 */
    ESP_LOGI(TAG, "[1/6] Board init...");
    board_init();

    /* 2. EPD 显示初始化 */
    ESP_LOGI(TAG, "[2/6] EPD init...");
    if (epd_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "EPD init failed!");
        return;
    }

    /* 3. GT911 触摸初始化 */
    ESP_LOGI(TAG, "[3/6] Touch init...");
    gt911_config_t touch_cfg = {
        .sda_gpio = BOARD_TOUCH_SDA,
        .scl_gpio = BOARD_TOUCH_SCL,
        .int_gpio = BOARD_TOUCH_INT,
        .rst_gpio = BOARD_TOUCH_RST,
        .i2c_port = BOARD_TOUCH_I2C_NUM,
        .i2c_freq = BOARD_TOUCH_I2C_FREQ,
    };
    if (gt911_init(&touch_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "Touch init failed, touch unavailable");
    }

    /* 4. SD 卡挂载 */
    ESP_LOGI(TAG, "[4/6] SD card mount...");
    sd_storage_config_t sd_cfg = {
        .miso_gpio = BOARD_SD_MISO,
        .mosi_gpio = BOARD_SD_MOSI,
        .clk_gpio = BOARD_SD_CLK,
        .cs_gpio = BOARD_SD_CS,
    };
    if (sd_storage_mount(&sd_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
    }

    /* 5. 加载字体 */
    ESP_LOGI(TAG, "[5/6] Loading fonts...");
    {
        const char *font_path = NULL;
        for (int i = 0; s_font_search_paths[i] != NULL; i++) {
            FILE *f = fopen(s_font_search_paths[i], "rb");
            if (f) {
                fclose(f);
                font_path = s_font_search_paths[i];
                break;
            }
        }

        if (font_path) {
            ESP_LOGI(TAG, "Font found: %s", font_path);
            s_font_title = ui_font_load_file(font_path, 32.0f);
            s_font_body  = ui_font_load_file(font_path, 22.0f);
            s_font_btn   = ui_font_load_file(font_path, 18.0f);

            if (s_font_title && s_font_body && s_font_btn) {
                ESP_LOGI(TAG, "Fonts loaded: title=%dpx body=%dpx btn=%dpx",
                         ui_font_line_height(s_font_title),
                         ui_font_line_height(s_font_body),
                         ui_font_line_height(s_font_btn));
            }
        } else {
            ESP_LOGW(TAG, "No font file found, text rendering disabled");
        }
    }

    /* 6. UI 框架初始化 + 启动 */
    ESP_LOGI(TAG, "[6/6] UI init...");
    ui_core_init();
    ui_touch_start(BOARD_TOUCH_INT);
    ui_page_push(&test_page, NULL);
    ui_core_run();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  All systems running");
    ESP_LOGI(TAG, "========================================");
}
