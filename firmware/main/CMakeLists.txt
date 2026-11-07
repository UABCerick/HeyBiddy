idf_component_register(SRCS "main.c"
                       INCLUDE_DIRS "."
                       REQUIRES board ui_core storage apps net esp_app_format nvs_flash)
