# net — Wi-Fi en modo estacion y hora por SNTP (componente compartido).
# Solo lo usan el sistema (ui_core) y main.c; las apps usan las funciones sys_wifi_* de sys.h.
idf_component_register(SRCS "net.c"
                       INCLUDE_DIRS "."
                       REQUIRES storage
                       PRIV_REQUIRES esp_wifi esp_netif esp_event esp_timer lwip)
