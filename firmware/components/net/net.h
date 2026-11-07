#pragma once
/*
 * net — Wi-Fi (estacion) y hora por SNTP.
 * Solo para el sistema y main.c. Las apps usan sys_net_connected() y sys_time_synced().
 */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Arranca el Wi-Fi en segundo plano y regresa de inmediato.
 * Si no hay SSID en menuconfig, solo lo avisa en el monitor. */
void net_start(void);

/* true mientras la placa tiene IP en el hotspot. */
bool net_is_connected(void);

/* true en cuanto SNTP puso la hora al menos una vez desde el arranque.
 * Despues sigue en true aunque se pierda el Wi-Fi: el reloj interno sigue corriendo. */
bool net_time_synced(void);

#ifdef __cplusplus
}
#endif
