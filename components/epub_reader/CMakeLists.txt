idf_component_register(
    SRCS "epub_reader.c"
         "miniz/miniz.c"
         "miniz/miniz_tdef.c"
         "miniz/miniz_tinfl.c"
         "miniz/miniz_zip.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
)
