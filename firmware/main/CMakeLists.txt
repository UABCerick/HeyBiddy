idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES board ui_core storage apps esp_app_format nvs_flash)
