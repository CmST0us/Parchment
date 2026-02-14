idf_component_register(
    SRCS "main.cpp" "board.c"
    INCLUDE_DIRS "." "pages"
    PRIV_REQUIRES "epd_driver" "gt911" "sd_storage" "settings_store" "ui_core" "ink_ui"
)

# C++17 编译选项
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -fno-exceptions
    -fno-rtti
)

# 将 fonts_data/ 目录打包为 LittleFS 镜像，烧录到 fonts 分区
littlefs_create_partition_image(fonts ../fonts_data FLASH_IN_PROJECT)
