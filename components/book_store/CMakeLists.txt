idf_component_register(
    SRCS "book_store.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES "sd_storage" "text_encoding"
)
