#pragma once
/*
 * net — Wi-Fi (estacion) y hora por SNTP.
 * Solo para el sistema y main.c. Las apps usan las funciones sys_* equivalentes de sys.h.
 *
 * La red se elige en la placa (Ajustes -> Wi-Fi) y se guarda en NVS, namespace "net".
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NET_NS "net"            /* namespace de NVS del sistema de red (reservado) */

typedef enum {
    NET_NO_NETWORK,             /* no hay red guardada */
    NET_CONNECTING,
    NET_CONNECTED,
    NET_WRONG_PASSWORD,
    NET_NOT_FOUND,              /* la red guardada no aparece */
} net_status_t;

typedef struct {
    char ssid[33];
    int8_t rssi;                /* dBm: -50 muy buena, -80 mala */
    bool secure;                /* pide contrasena */
} net_ap_t;

/* Arranca el Wi-Fi en segundo plano y regresa de inmediato.
 * Si hay una red guardada en NVS, se conecta a ella. */
void net_start(void);

bool net_is_connected(void);
/* true en cuanto SNTP puso la hora al menos una vez desde el arranque. */
bool net_time_synced(void);
net_status_t net_status(void);

/* Red guardada (false si no hay ninguna). */
bool net_saved_ssid(char *out, size_t len);

/* Buscar redes (asincrono). net_scan_results devuelve -1 mientras busca,
 * y despues cuantas redes copio en out (ordenadas por senal, sin repetidas). */
bool net_scan_start(void);
int net_scan_results(net_ap_t *out, int max);

/* Guarda la red en NVS y se conecta. pass = "" para redes abiertas. */
void net_connect(const char *ssid, const char *pass);
/* Borra la red guardada y se desconecta. */
void net_forget(void);

#ifdef __cplusplus
}
#endif
