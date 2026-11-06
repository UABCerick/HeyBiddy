idf_component_register(SRCS "board.c" "board_lcd_init.c" "bidi_switch_knob.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_lcd driver esp_driver_i2c esp_driver_ledc esp_driver_gpio esp_timer
                       PRIV_REQUIRES esp_lcd_sh8601)
