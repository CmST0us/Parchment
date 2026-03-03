/**
 * @file main.cpp
 * @brief 模拟器入口 -- 创建 HAL 实例, 初始化 SDL, 启动 InkUI Application。
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <thread>
#include <atomic>
#include <SDL.h>

#include "SdlDisplayDriver.h"
#include "SdlTouchDriver.h"
#include "DesktopPlatform.h"
#include "DesktopFontProvider.h"
#include "DesktopSystemInfo.h"

#include "ink_ui/InkUI.h"
#include "controllers/BootViewController.h"

extern "C" {
#include "settings_store.h"
#include "ui_font.h"
#include "sd_storage.h"
}

static ink::Application app;
static std::atomic<bool> running{true};

/// 信号处理：SIGTERM/SIGINT 时直接退出，避免 detached thread 导致 core dump
static void signalHandler(int sig) {
    (void)sig;
    _exit(0);
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;

    // Install signal handlers for clean shutdown
    std::signal(SIGTERM, signalHandler);
    std::signal(SIGINT, signalHandler);

    fprintf(stderr, "Parchment Simulator starting...\n");

    // SDL init
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) < 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int scale = 1;
    // Check for --scale argument
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
            scale = atoi(argv[i + 1]);
            if (scale < 1) scale = 1;
            if (scale > 4) scale = 4;
        }
    }

    // Create HAL instances
    SdlDisplayDriver display(scale);
    SdlTouchDriver touch(scale);
    DesktopPlatform platform;
    DesktopFontProvider fonts(FONTS_MOUNT_POINT);
    DesktopSystemInfo systemInfo;

    // Init storage
    settings_store_init();

    // SD storage stub (sets mounted=true)
    sd_storage_config_t sd_cfg = {};
    sd_storage_mount(&sd_cfg);

    // Init fonts
    fonts.init();

    // Init InkUI Application
    if (!app.init(display, touch, platform, systemInfo)) {
        fprintf(stderr, "Application init failed\n");
        SDL_Quit();
        return 1;
    }

    // Set StatusBar font and battery
    auto* sb = app.statusBar();
    if (sb) {
        sb->setFont(ui_font_get(16));
        sb->updateBattery(systemInfo.batteryPercent());
    }

    // Push boot page
    app.navigator().push(std::make_unique<BootViewController>(app));

    // Start app.run() in a background thread
    // (main thread will pump SDL events)
    // Detach because app.run() is [[noreturn]] — thread never joins.
    std::thread appThread([]() {
        app.run();
    });
    appThread.detach();

    // Main thread: SDL event loop
    while (running.load()) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    fprintf(stderr, "Parchment Simulator: window closed, exiting.\n");
                    SDL_Quit();
                    _exit(0);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        touch.pushMouseEvent(event.button.x, event.button.y, true);
                    }
                    break;
                case SDL_MOUSEMOTION:
                    if (event.motion.state & SDL_BUTTON_LMASK) {
                        touch.pushMouseEvent(event.motion.x, event.motion.y, true);
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        touch.pushMouseEvent(event.button.x, event.button.y, false);
                    }
                    break;
            }
        }
        // 主线程负责 SDL 渲染（SDL API 必须在创建 renderer 的线程调用）
        display.presentIfNeeded();
        SDL_Delay(10); // 100 Hz poll rate
    }

    // Cleanup (won't normally reach here since app.run() is [[noreturn]])
    SDL_Quit();
    return 0;
}
