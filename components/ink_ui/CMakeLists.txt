# InkUI — Parchment E-Reader UI Framework (C++17)
# UIKit 风格的墨水屏 UI 框架

set(INK_UI_SRCS
    "src/core/Geometry.cpp"
    "src/core/EpdDriver.cpp"
    "src/core/Canvas.cpp"
    "src/core/View.cpp"
    "src/core/FlexLayout.cpp"
    "src/core/RenderEngine.cpp"
    "src/core/GestureRecognizer.cpp"
    "src/core/ViewController.cpp"
    "src/core/NavigationController.cpp"
    "src/core/Application.cpp"
    "src/views/TextLabel.cpp"
    "src/views/IconView.cpp"
    "src/views/ButtonView.cpp"
    "src/views/HeaderView.cpp"
    "src/views/ProgressBarView.cpp"
    "src/views/SeparatorView.cpp"
    "src/views/PageIndicatorView.cpp"
    "src/views/StatusBarView.cpp"
)

idf_component_register(
    SRCS ${INK_UI_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES epd_driver epdiy gt911
    PRIV_REQUIRES ui_core battery
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -fno-exceptions
    -fno-rtti
    -Wall -Wextra -Wno-unused-parameter
)
