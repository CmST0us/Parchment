# FontEngine — Parchment 字体引擎 (.bin 格式 + 多级缓存 + 面积加权缩放)

set(FONT_ENGINE_SRCS
    "src/font_loader.c"
    "src/font_cache.c"
    "src/font_scaler.c"
    "src/font_rle.c"
)

idf_component_register(
    SRCS ${FONT_ENGINE_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES esp_common
    PRIV_REQUIRES esp_littlefs
)

target_compile_options(${COMPONENT_LIB} PRIVATE
    -Wall -Wextra -Wno-unused-parameter
)
