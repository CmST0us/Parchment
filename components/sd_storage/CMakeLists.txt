idf_component_register(
    SRCS "sd_storage.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "driver" "fatfs" "vfs"
)
