idf_component_register(
    SRCS "main.c" "board.c" "pages/page_boot.c"
    INCLUDE_DIRS "." "pages"
    PRIV_REQUIRES "epd_driver" "gt911" "sd_storage" "settings_store" "book_store" "ui_core"
)

# 将 fonts_data/ 目录打包为 LittleFS 镜像，烧录到 fonts 分区
littlefs_create_partition_image(fonts ../fonts_data FLASH_IN_PROJECT)
