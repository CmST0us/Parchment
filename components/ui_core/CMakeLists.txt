idf_component_register(
    SRCS "ui_event.c"
         "ui_page.c"
         "ui_canvas.c"
         "ui_font.c"
         "ui_touch.c"
         "ui_render.c"
         "ui_core.c"
    INCLUDE_DIRS "include"
    REQUIRES "epd_driver" "gt911"
)
