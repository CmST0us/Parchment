/**
 * @file sim_main.c
 * @brief SDL desktop simulator entry point for Parchment UI.
 *
 * Provides main() which:
 * 1. Initializes an SDL2 window that displays the E-Ink framebuffer.
 * 2. Calls app_main() to start the UI framework (same code as on device).
 * 3. Runs the SDL event loop, translating mouse input to touch gestures.
 *
 * The UI core loop runs on a background pthread (via FreeRTOS shim).
 * The SDL main thread handles rendering and input translation.
 *
 * Build: see simulator/CMakeLists.txt
 *
 * Controls:
 *   - Left click + release quickly  → TAP at that position
 *   - Click + drag + release        → SWIPE (direction determined by drag)
 *   - Click + hold 800ms            → LONG_PRESS
 *   - Escape / close window         → Quit
 */

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "ui_event.h"
#include "epd_driver.h"

/* Forward declaration: app_main is defined in main/main.c */
extern void app_main(void);

/* Display-dirty flag from sim_epd_driver.c */
extern volatile int g_sim_display_dirty;

/* -------------------------------------------------------------------------- */
/*  Constants                                                                  */
/* -------------------------------------------------------------------------- */

/** Logical screen dimensions (portrait, same as UI_SCREEN_WIDTH/HEIGHT). */
#define SCREEN_W 540
#define SCREEN_H 960

/** Physical framebuffer dimensions (landscape). */
#define FB_PHYS_W 960
#define FB_PHYS_H 540

/** Default window scale factor. */
#define WINDOW_SCALE 0.75

/** Gesture recognition thresholds (same as embedded). */
#define GESTURE_TAP_MAX_DIST_SQ   (20 * 20)
#define GESTURE_TAP_MAX_TIME_MS   500
#define GESTURE_SWIPE_MIN_DIST_SQ (40 * 40)
#define GESTURE_LONG_PRESS_MS     800
#define GESTURE_LONG_PRESS_DIST_SQ (20 * 20)

/* -------------------------------------------------------------------------- */
/*  SDL state                                                                  */
/* -------------------------------------------------------------------------- */

static SDL_Window   *s_window   = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture  *s_texture  = NULL;

/* -------------------------------------------------------------------------- */
/*  Mouse gesture tracking                                                     */
/* -------------------------------------------------------------------------- */

static bool     s_mouse_down = false;
static bool     s_long_press_fired = false;
static int      s_start_x, s_start_y;
static int      s_cur_x, s_cur_y;
static uint32_t s_down_time;

static void send_ui_event(ui_event_type_t type, int16_t x, int16_t y) {
    ui_event_t evt = {
        .type = type,
        .x    = x,
        .y    = y,
        .data.param = 0,
    };
    ui_event_send(&evt);
}

/**
 * @brief Check for long press while mouse is held down.
 */
static void check_long_press(void) {
    if (!s_mouse_down || s_long_press_fired) return;

    uint32_t elapsed = SDL_GetTicks() - s_down_time;
    int dx = s_cur_x - s_start_x;
    int dy = s_cur_y - s_start_y;
    int dist_sq = dx * dx + dy * dy;

    if (elapsed >= GESTURE_LONG_PRESS_MS && dist_sq < GESTURE_LONG_PRESS_DIST_SQ) {
        send_ui_event(UI_EVENT_TOUCH_LONG_PRESS,
                      (int16_t)s_start_x, (int16_t)s_start_y);
        s_long_press_fired = true;
    }
}

/**
 * @brief Handle mouse button release and classify gesture.
 */
static void handle_mouse_up(int x, int y) {
    if (!s_mouse_down) return;
    s_mouse_down = false;

    /* If long press already fired, don't generate another gesture */
    if (s_long_press_fired) return;

    int dx = x - s_start_x;
    int dy = y - s_start_y;
    int dist_sq = dx * dx + dy * dy;
    uint32_t elapsed = SDL_GetTicks() - s_down_time;

    if (dist_sq >= GESTURE_SWIPE_MIN_DIST_SQ) {
        /* Swipe: determine primary direction */
        ui_event_type_t type;
        int abs_dx = dx < 0 ? -dx : dx;
        int abs_dy = dy < 0 ? -dy : dy;

        if (abs_dx >= abs_dy) {
            type = dx < 0 ? UI_EVENT_TOUCH_SWIPE_LEFT
                          : UI_EVENT_TOUCH_SWIPE_RIGHT;
        } else {
            type = dy < 0 ? UI_EVENT_TOUCH_SWIPE_UP
                          : UI_EVENT_TOUCH_SWIPE_DOWN;
        }

        send_ui_event(type, (int16_t)s_start_x, (int16_t)s_start_y);
    } else if (dist_sq < GESTURE_TAP_MAX_DIST_SQ &&
               elapsed < GESTURE_TAP_MAX_TIME_MS) {
        /* Tap */
        send_ui_event(UI_EVENT_TOUCH_TAP,
                      (int16_t)s_start_x, (int16_t)s_start_y);
    }
}

/* -------------------------------------------------------------------------- */
/*  Framebuffer → SDL texture conversion                                       */
/* -------------------------------------------------------------------------- */

/**
 * @brief Convert the 4bpp physical framebuffer to the SDL ARGB8888 texture.
 *
 * Physical layout (landscape 960×540, 4bpp packed):
 *   byte at [py * 480 + px/2]
 *   even px: gray stored in low nibble  → (byte & 0x0F) << 4
 *   odd  px: gray stored in high nibble → byte & 0xF0
 *
 * Display as portrait (540×960):
 *   SDL pixel (sx, sy) → logical (lx=sx, ly=sy) → physical (px=ly, py=lx)
 */
static void update_texture(void) {
    uint8_t *fb = epd_driver_get_framebuffer();
    if (!fb) return;

    void *pixels;
    int pitch;

    if (SDL_LockTexture(s_texture, NULL, &pixels, &pitch) != 0) {
        fprintf(stderr, "SDL_LockTexture failed: %s\n", SDL_GetError());
        return;
    }

    uint32_t *dst = (uint32_t *)pixels;
    int dst_stride = pitch / 4; /* pixels per row in the texture */

    for (int sy = 0; sy < SCREEN_H; sy++) {
        for (int sx = 0; sx < SCREEN_W; sx++) {
            /* Logical (lx, ly) = (sx, sy) → physical (px, py) = (sy, sx) */
            int px = sy;
            int py = sx;

            uint8_t byte_val = fb[py * (FB_PHYS_W / 2) + px / 2];
            uint8_t gray;
            if (px & 1) {
                gray = byte_val & 0xF0;
            } else {
                gray = (byte_val & 0x0F) << 4;
            }

            /* Expand 4-bit to 8-bit: 0x00→0x00, 0x10→0x11, ..., 0xF0→0xFF */
            gray = gray | (gray >> 4);

            dst[sy * dst_stride + sx] = 0xFF000000u | (gray << 16) | (gray << 8) | gray;
        }
    }

    SDL_UnlockTexture(s_texture);
}

/* -------------------------------------------------------------------------- */
/*  Main                                                                       */
/* -------------------------------------------------------------------------- */

/** Optional screenshot path: if set, save framebuffer after first render and exit. */
static const char *s_screenshot_path = NULL;

/**
 * @brief Save current framebuffer as BMP to the given path.
 */
static void save_screenshot_bmp(const char *path) {
    SDL_Surface *surf = SDL_CreateRGBSurface(0, SCREEN_W, SCREEN_H, 32,
                                              0x00FF0000, 0x0000FF00,
                                              0x000000FF, 0xFF000000);
    if (!surf) {
        fprintf(stderr, "Failed to create surface: %s\n", SDL_GetError());
        return;
    }

    uint8_t *fb = epd_driver_get_framebuffer();
    if (!fb) { SDL_FreeSurface(surf); return; }

    uint32_t *dst = (uint32_t *)surf->pixels;
    int dst_stride = surf->pitch / 4;

    for (int sy = 0; sy < SCREEN_H; sy++) {
        for (int sx = 0; sx < SCREEN_W; sx++) {
            int px = sy, py = sx;
            uint8_t byte_val = fb[py * (FB_PHYS_W / 2) + px / 2];
            uint8_t gray;
            if (px & 1) { gray = byte_val & 0xF0; }
            else        { gray = (byte_val & 0x0F) << 4; }
            gray = gray | (gray >> 4);
            dst[sy * dst_stride + sx] = 0xFF000000u | (gray << 16) | (gray << 8) | gray;
        }
    }

    if (SDL_SaveBMP(surf, path) == 0) {
        printf("Screenshot saved: %s\n", path);
    } else {
        fprintf(stderr, "Failed to save BMP: %s\n", SDL_GetError());
    }
    SDL_FreeSurface(surf);
}

int main(int argc, char *argv[]) {
    /* Check for --screenshot option */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            s_screenshot_path = argv[++i];
        }
    }

    /* ---- SDL init ---- */
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int win_w = (int)(SCREEN_W * WINDOW_SCALE);
    int win_h = (int)(SCREEN_H * WINDOW_SCALE);

    s_window = SDL_CreateWindow(
        "Parchment Simulator (540x960 E-Ink)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, win_h,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN);

    if (!s_window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    s_renderer = SDL_CreateRenderer(
        s_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!s_renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return 1;
    }

    /* Logical size ensures mouse coordinates map to 540×960 regardless of window size */
    SDL_RenderSetLogicalSize(s_renderer, SCREEN_W, SCREEN_H);

    s_texture = SDL_CreateTexture(
        s_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        SCREEN_W, SCREEN_H);

    if (!s_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(s_renderer);
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return 1;
    }

    printf("=== Parchment SDL Simulator ===\n");
    printf("  Window : %dx%d (logical %dx%d)\n", win_w, win_h, SCREEN_W, SCREEN_H);
    printf("  Input  : mouse click=TAP, drag=SWIPE, hold=LONG_PRESS\n");
    printf("  Quit   : ESC or close window\n");
    printf("================================\n\n");

    /* ---- Start application (same as ESP-IDF app_main) ---- */
    app_main();

    /* ---- SDL event loop ---- */
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                if (e.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
                break;

            case SDL_MOUSEBUTTONDOWN:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    s_mouse_down = true;
                    s_long_press_fired = false;
                    s_start_x = e.button.x;
                    s_start_y = e.button.y;
                    s_cur_x   = e.button.x;
                    s_cur_y   = e.button.y;
                    s_down_time = SDL_GetTicks();
                }
                break;

            case SDL_MOUSEMOTION:
                if (s_mouse_down) {
                    s_cur_x = e.motion.x;
                    s_cur_y = e.motion.y;
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (e.button.button == SDL_BUTTON_LEFT) {
                    handle_mouse_up(e.button.x, e.button.y);
                }
                break;
            }
        }

        /* Check for long press while holding */
        check_long_press();

        /* Update SDL texture if framebuffer was modified */
        if (g_sim_display_dirty) {
            g_sim_display_dirty = 0;
            update_texture();
            SDL_RenderClear(s_renderer);
            SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
            SDL_RenderPresent(s_renderer);

            /* Screenshot mode: save after first render and exit */
            if (s_screenshot_path) {
                SDL_Delay(100); /* Brief delay for render completion */
                save_screenshot_bmp(s_screenshot_path);
                running = false;
            }
        }

        SDL_Delay(16); /* ~60 fps */
    }

    /* ---- Cleanup ---- */
    SDL_DestroyTexture(s_texture);
    SDL_DestroyRenderer(s_renderer);
    SDL_DestroyWindow(s_window);
    SDL_Quit();

    return 0;
}
