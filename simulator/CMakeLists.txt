cmake_minimum_required(VERSION 3.16)
project(parchment_sim CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# Simulator flags
add_compile_options(-fno-exceptions -fno-rtti -Wall -Wextra -Wno-unused-parameter)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)

set(ROOT ${CMAKE_SOURCE_DIR}/..)
set(INK_UI ${ROOT}/components/ink_ui)
set(COMP ${ROOT}/components)
set(APP ${ROOT}/main)

# InkUI core sources (exclude ESP32 HAL implementations)
file(GLOB INK_UI_CORE ${INK_UI}/src/core/*.cpp)
file(GLOB INK_UI_VIEWS ${INK_UI}/src/views/*.cpp)

# App sources
file(GLOB APP_CONTROLLERS ${APP}/controllers/*.cpp)
file(GLOB APP_VIEWS ${APP}/views/*.cpp)

# Component sources (portable)
set(COMP_SRCS
    ${COMP}/ui_core/ui_font.c
    ${COMP}/ui_core/ui_font_pfnt.c
    ${COMP}/book_store/book_store.c
    ${COMP}/settings_store/settings_store.c
    ${COMP}/text_encoding/text_encoding.c
    ${COMP}/text_encoding/text_encoding_gbk_table.c
    ${COMP}/text_source/src/TextSource.cpp
    ${COMP}/text_source/src/TextWindow.cpp
    ${COMP}/text_source/src/EncodingConverter.cpp
)

# Simulator sources
file(GLOB SIM_SRCS src/*.cpp)
file(GLOB STUB_SRCS stubs/*.c stubs/*.cpp)
file(GLOB COMPAT_SRCS compat/*.c)

add_executable(parchment_sim
    ${SIM_SRCS}
    ${INK_UI_CORE}
    ${INK_UI_VIEWS}
    ${APP_CONTROLLERS}
    ${APP_VIEWS}
    ${COMP_SRCS}
    ${STUB_SRCS}
    ${COMPAT_SRCS}
)

# Path remapping for simulator
target_compile_definitions(parchment_sim PRIVATE
    SIMULATOR=1
    FONTS_MOUNT_POINT="${CMAKE_SOURCE_DIR}/fonts"
    SD_MOUNT_POINT="${CMAKE_SOURCE_DIR}/data"
)

target_include_directories(parchment_sim PRIVATE
    # Simulator stubs (MUST come first to override ESP-IDF headers)
    ${CMAKE_SOURCE_DIR}/stubs/include
    ${CMAKE_SOURCE_DIR}/compat

    # InkUI framework
    ${INK_UI}/include

    # App
    ${APP}
    ${APP}/pages

    # Components
    ${COMP}/ui_core/include
    ${COMP}/book_store/include
    ${COMP}/settings_store/include
    ${COMP}/text_encoding/include
    ${COMP}/text_source/include
    ${COMP}/sd_storage/include
    ${COMP}/epd_driver/include
    ${COMP}/gt911/include

    # SDL2
    ${SDL2_INCLUDE_DIRS}
)

target_link_libraries(parchment_sim ${SDL2_LIBRARIES} pthread)
