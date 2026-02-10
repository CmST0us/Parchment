/**
 * @file main.c
 * @brief Parchment 墨水屏阅读器 - 应用入口。
 *
 * 初始化板级硬件（EPD、触摸、SD 卡）和 ui_core 框架，启动事件循环。
 */

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "board.h"
#include "epd_driver.h"
#include "gt911.h"
#include "sd_storage.h"
#include "ui_core.h"
#include "ui_font.h"
#include "ui_text.h"

static const char *TAG = "parchment";

/* ========================================================================== */
/*  临时验证页面：文字渲染验证                                                   */
/* ========================================================================== */

/** 测试歌词文本（中日英混排）。 */
static const char *s_lyrics =
    "いつの間にか来たね曾几何时起来到了此处\n"
    "こんな遠いところまで来到了如此遥远的地方\n"
    "ああ　空はいつもの青い空啊啊 天空仍是一如既往的湛蓝\n"
    "何もなかったから正因为曾经一无所有\n"
    "何かつかみたい想いが想要抓住某些事物的想法\n"
    "ああ　道を作ってくれたのかな啊啊 是否已为我们开辟出道路\n"
    "キセキは起こると知ったよ我早知道奇迹肯定会发生\n"
    "あの頃の僕らへと　教えてあげたい好想让昔日的我们也明白这一点\n"
    "あきらめないことが　夢への手がかりだと绝对不轻言放弃 这便是通往梦想的线索\n"
    "今だから言ってもいいかな　言いたいね正因是现在我还能否如此阐述 好想说出口\n"
    "迷いに揺れるこころ　それでも前を向いて即便内心因迷惘而动摇也要望向前方\n"
    "やまない雨はないと走ってきたよ不停歇的雨并不存在 我们就是如此一路奔来\n"
    "だからねサンシャイン　見上がれば熱い太陽才得以遇见sunshine 只要仰望便能看见炽热的太阳\n"
    "ひとりからふたりへ从一个人到你伴相随\n"
    "ふたりからみんなの中に从我俩到伙伴齐聚一堂\n"
    "ああ　伝わってくれた夢の鼓動啊啊 让我感受到了梦想的鼓动\n"
    "何もなかったのに明明就一无所有\n"
    "何ができる気がしてたよ却感觉能够成就些什么\n"
    "ああ　大胆すぎたかな　でも頑張れた啊啊 是否有些胆大 但我们已努力过\n"
    "キセキが起こったね本当に奇迹发生了 此乃千真万确\n"
    "悩んでた僕らへと　教えてあげたい好想告诉曾为此烦恼不已的我们\n"
    "投げ出したい時こそ　大きく変わる時さ正因是放手一搏之时 巨大变化才会随之而来\n"
    "そこにきっとチャンスがあるから　あったから那里肯定有着转机 肯定也曾有过转机\n"
    "動いてないと探せない　休んでも止まらないで倘若不动身便无法寻得 哪怕休息片刻也不会停下脚步\n"
    "明けない夜はないと走ってきたね没有黎明的黑夜并不存在 我们就是这样一路奔来\n"
    "そうだよサンシャイン　いつだって熱い太陽正是如此sunshine 永远都是炽热的太阳\n"
    "ヒカリ　ユウキ　ミライ　歌にすれば光芒 勇气 未来 只要唱出口\n"
    "照れくさい言葉だって届けられるから正因是羞于启齿的话语才能传达出去\n"
    "大きな声で！用宏亮的声音！\n";

/** 当前展示模式: 0=字号一览, 1=分页歌词 */
static int s_demo_mode = 0;
/** 分页模式的当前页偏移 */
static int s_page_offset = 0;
/** 渲染后计算的下一页偏移 */
static int s_next_offset = 0;

static void text_demo_on_enter(void *arg) {
    ESP_LOGI(TAG, "Text rendering demo page entered");
    s_demo_mode = 0;
    s_page_offset = 0;
    s_next_offset = 0;
    ui_render_mark_full_dirty();
}

static void text_demo_on_event(ui_event_t *event) {
    if (event->type == UI_EVENT_TOUCH_TAP) {
        if (s_demo_mode == 0) {
            /* 点击任意位置：进入分页歌词模式 */
            s_demo_mode = 1;
            s_page_offset = 0;
            s_next_offset = 0;
            ui_render_mark_full_dirty();
        } else {
            /* 点击右半屏翻下一页，左半屏翻上一页 */
            if (event->x > UI_SCREEN_WIDTH / 2) {
                /* 下一页 — 使用上次渲染计算的偏移 */
                s_page_offset = s_next_offset;
                ui_render_mark_full_dirty();
            } else {
                /* 回到字号一览 */
                s_demo_mode = 0;
                ui_render_mark_full_dirty();
            }
        }
    }
}

static void render_font_showcase(uint8_t *fb) {
    ui_canvas_fill(fb, UI_COLOR_WHITE);

    int y = 40;
    int sizes[] = { 20, 24, 28, 32, 36, 40 };

    for (int i = 0; i < 6; i++) {
        const EpdFont *font = ui_font_get(sizes[i]);
        char label[32];
        snprintf(label, sizeof(label), "%dpx:", sizes[i]);
        ui_canvas_draw_text(fb, 24, y, font, label, UI_COLOR_DARK);

        int label_w = ui_canvas_measure_text(font, label);
        ui_canvas_draw_text(fb, 24 + label_w + 8, y, font,
                            "奇迹闪耀 sunshine", UI_COLOR_BLACK);

        y += sizes[i] + 16;
    }

    /* 底部提示 */
    const EpdFont *small = ui_font_get(20);
    ui_canvas_draw_text(fb, 24, UI_SCREEN_HEIGHT - 40, small,
                        "点击任意位置查看分页歌词", UI_COLOR_MEDIUM);
}

static void render_lyrics_page(uint8_t *fb) {
    ui_canvas_fill(fb, UI_COLOR_WHITE);

    const EpdFont *font = ui_font_get(36);
    int margin = 24;
    int top_y = 48;
    int content_width = UI_SCREEN_WIDTH - margin * 2;
    int content_height = UI_SCREEN_HEIGHT - top_y - 48;
    int line_height = 40; /* 24px 字号 × 1.67 行距 */

    /* 页眉 */
    const EpdFont *header_font = ui_font_get(20);
    ui_canvas_draw_text(fb, margin, 28, header_font,
                        "キセキヒカル — 奇迹闪耀", UI_COLOR_DARK);
    ui_canvas_draw_hline(fb, margin, 36, content_width, UI_COLOR_LIGHT);

    /* 渲染当前页 */
    ui_text_result_t result = ui_canvas_draw_text_page(
        fb, margin, top_y, content_width, content_height,
        line_height, font, s_lyrics, s_page_offset, UI_COLOR_BLACK);

    /* 记录下一页偏移（仅在事件处理中使用） */
    if (!result.reached_end) {
        s_next_offset = s_page_offset + result.bytes_consumed;
    } else {
        s_next_offset = s_page_offset;
    }

    /* 页脚 */
    ui_canvas_draw_hline(fb, margin, UI_SCREEN_HEIGHT - 44,
                         content_width, UI_COLOR_LIGHT);
    char footer[64];
    snprintf(footer, sizeof(footer), "%d 行  %s",
             result.lines_rendered,
             result.reached_end ? "[END]" : "点击右侧翻页 →");
    ui_canvas_draw_text(fb, margin, UI_SCREEN_HEIGHT - 24,
                        header_font, footer, UI_COLOR_DARK);
}

static void text_demo_on_render(uint8_t *fb) {
    if (s_demo_mode == 0) {
        render_font_showcase(fb);
    } else {
        render_lyrics_page(fb);
    }
}

static ui_page_t text_demo_page = {
    .name = "text_demo",
    .on_enter = text_demo_on_enter,
    .on_exit = NULL,
    .on_event = text_demo_on_event,
    .on_render = text_demo_on_render,
};

/* ========================================================================== */
/*  app_main                                                                   */
/* ========================================================================== */

void app_main(void) {
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  Parchment E-Reader v0.2");
    ESP_LOGI(TAG, "  M5Stack PaperS3 (ESP32-S3)");
    ESP_LOGI(TAG, "========================================");

    /* 1. 板级初始化 */
    ESP_LOGI(TAG, "[1/5] Board init...");
    board_init();

    /* 2. EPD 显示初始化 */
    ESP_LOGI(TAG, "[2/5] EPD init...");
    if (epd_driver_init() != ESP_OK) {
        ESP_LOGE(TAG, "EPD init failed!");
        return;
    }
    ESP_LOGI(TAG, "[2/5] EPD init done, fullclear should have executed");

    /* 3. GT911 触摸初始化 */
    ESP_LOGI(TAG, "[3/5] Touch init...");
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
    ESP_LOGI(TAG, "[4/5] SD card mount...");
    sd_storage_config_t sd_cfg = {
        .miso_gpio = BOARD_SD_MISO,
        .mosi_gpio = BOARD_SD_MOSI,
        .clk_gpio = BOARD_SD_CLK,
        .cs_gpio = BOARD_SD_CS,
    };
    if (sd_storage_mount(&sd_cfg) != ESP_OK) {
        ESP_LOGW(TAG, "SD card not available");
    }

    /* 5. UI 框架初始化 + 启动 */
    ESP_LOGI(TAG, "[5/5] UI init...");
    ui_core_init();
    ui_touch_start(BOARD_TOUCH_INT);
    ui_page_push(&text_demo_page, NULL);
    ui_core_run();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  All systems running");
    ESP_LOGI(TAG, "========================================");
}
