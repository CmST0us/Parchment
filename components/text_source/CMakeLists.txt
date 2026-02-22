idf_component_register(
    SRCS "src/TextSource.cpp" "src/TextWindow.cpp" "src/EncodingConverter.cpp"
    INCLUDE_DIRS "include"
    REQUIRES "esp_common" "freertos"
    PRIV_REQUIRES "text_encoding" "sd_storage" "esp_psram"
)
