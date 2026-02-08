idf_component_register(
    SRCS "main.c" "board.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES "epd_driver" "gt911" "sd_storage" "ui_core" "esp_littlefs"
)

# 将 sdcard/fonts/ 目录打包为 LittleFS 镜像，烧录到 "fonts" 分区
littlefs_create_partition_image(fonts ../sdcard FLASH_IN_PROJECT)
