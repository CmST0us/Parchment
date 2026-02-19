idf_component_register(
    SRCS "main.cpp"
         "controllers/BootViewController.cpp"
         "controllers/LibraryViewController.cpp"
         "controllers/ReaderViewController.cpp"
         "views/BootLogoView.cpp"
         "views/BookCoverView.cpp"
    INCLUDE_DIRS "."
    PRIV_REQUIRES "sd_storage" "settings_store" "ink_ui" "book_store" "text_encoding" "font_engine" "ui_icon" "M5GFX" "M5Unified" "esp_littlefs"
)

# C++17 编译选项
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -fno-exceptions
    -fno-rtti
)

# 将 fonts_data/ 目录打包为 LittleFS 镜像，烧录到 fonts 分区
littlefs_create_partition_image(fonts ../fonts_data FLASH_IN_PROJECT)
