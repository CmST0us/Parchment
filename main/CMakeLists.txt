idf_component_register(
    SRCS "main.c" "board.c"
         "pages/bookshelf_page.c" "pages/reader_page.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES "epd_driver" "gt911" "sd_storage" "ui_core"
                  "littlefs_storage" "font_manager" "text_layout"
)

# 将 fonts/ 目录打包为 LittleFS 镜像，烧录到 storage 分区
littlefs_create_partition_image(storage ../fonts FLASH_IN_PROJECT)
