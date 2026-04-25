idf_component_register(SRCS "Camstream.c" "Camera.c"
                    PRIV_REQUIRES nvs_flash esp_psram
                    REQUIRES esp_wifi esp_http_server
                    INCLUDE_DIRS "" "include")
