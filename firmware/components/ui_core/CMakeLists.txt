idf_component_register(SRCS "ui_core.c" "launcher.c"
                       INCLUDE_DIRS "."
                       REQUIRES lvgl fonts storage esp_timer
                       PRIV_REQUIRES board esp_app_format)
