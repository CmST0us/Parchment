idf_component_register(
    SRCS "ui_event.c"
         "ui_page.c"
         "ui_canvas.c"
         "ui_touch.c"
         "ui_render.c"
         "ui_core.c"
         "ui_font.c"
         "ui_text.c"
    INCLUDE_DIRS "include" "fonts"
    REQUIRES "epd_driver" "gt911" "epdiy"
)
