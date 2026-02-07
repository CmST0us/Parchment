idf_component_register(
    SRCS "main.c" "board.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES "epd_driver" "gt911" "sd_storage" "ui_core" "ui_text" "epub_reader"
)
