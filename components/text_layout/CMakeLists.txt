idf_component_register(
    SRCS "text_layout.c" "text_paginator.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "font_manager" "ui_core"
)
