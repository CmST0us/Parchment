# InkUI — Parchment E-Reader UI Framework (C++17)
# UIKit 风格的墨水屏 UI 框架

set(INK_UI_SRCS
    "src/core/Geometry.cpp"
    "src/core/EpdDriver.cpp"
    "src/core/Canvas.cpp"
)

idf_component_register(
    SRCS ${INK_UI_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES epd_driver epdiy
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall -Wextra -Wno-unused-parameter
)
