cmake_minimum_required(VERSION 3.16)
project(parchment_sim C)

set(CMAKE_C_STANDARD 11)

# --------------------------------------------------------------------------
#  Find SDL2
# --------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

# --------------------------------------------------------------------------
#  Source files
# --------------------------------------------------------------------------

# Simulator-specific sources
set(SIM_SOURCES
    sim_main.c
    sim_epd_driver.c
    sim_touch.c
    sim_stubs.c
    sim_freertos.c
)

# Reuse ui_core sources (compiled with platform shim headers)
set(UI_CORE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../components/ui_core)
set(UI_CORE_SOURCES
    ${UI_CORE_DIR}/ui_event.c
    ${UI_CORE_DIR}/ui_page.c
    ${UI_CORE_DIR}/ui_canvas.c
    ${UI_CORE_DIR}/ui_render.c
    ${UI_CORE_DIR}/ui_core.c
)

# Reuse ui_text sources
set(UI_TEXT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../components/ui_text)
set(UI_TEXT_SOURCES
    ${UI_TEXT_DIR}/ui_text.c
)

# Application main (provides app_main and test page)
set(APP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../main/main.c
)

# --------------------------------------------------------------------------
#  Executable
# --------------------------------------------------------------------------
add_executable(parchment_sim
    ${SIM_SOURCES}
    ${UI_CORE_SOURCES}
    ${UI_TEXT_SOURCES}
    ${APP_SOURCES}
)

# --------------------------------------------------------------------------
#  Include paths (order matters: platform shims must come first)
# --------------------------------------------------------------------------
target_include_directories(parchment_sim PRIVATE
    # 1. Platform shim headers (override ESP-IDF headers)
    ${CMAKE_CURRENT_SOURCE_DIR}/platform

    # 2. Component include directories
    ${UI_CORE_DIR}/include
    ${UI_TEXT_DIR}/include
    ${UI_TEXT_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/epd_driver/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/gt911/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../components/sd_storage/include

    # 3. Main directory (for board.h)
    ${CMAKE_CURRENT_SOURCE_DIR}/../main

    # 4. SDL2
    ${SDL2_INCLUDE_DIRS}
)

# --------------------------------------------------------------------------
#  Linking
# --------------------------------------------------------------------------
target_link_libraries(parchment_sim PRIVATE
    ${SDL2_LIBRARIES}
    pthread
    m
)

target_compile_options(parchment_sim PRIVATE
    ${SDL2_CFLAGS_OTHER}
    -Wall -Wextra -Wno-unused-parameter
)
