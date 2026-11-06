# Copia de espressif/esp_lcd_sh8601 2.0.1 (esp-iot-solution, Apache-2.0).
# Se quito la dependencia de cmake_utilities para que compile sin descargar nada;
# la version que antes ponia cmake_utilities se define aqui a mano.
idf_component_register(SRCS "esp_lcd_sh8601.c" INCLUDE_DIRS "include" PRIV_REQUIRES "driver" REQUIRES "esp_lcd")
target_compile_definitions(${COMPONENT_LIB} PRIVATE ESP_LCD_SH8601_VER_MAJOR=2 ESP_LCD_SH8601_VER_MINOR=0 ESP_LCD_SH8601_VER_PATCH=1)
