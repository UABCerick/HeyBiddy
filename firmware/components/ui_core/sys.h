#pragma once
/*
 * sys.h — lo que el sistema le ofrece a las apps.
 * Las apps incluyen "app.h", "sys.h", "storage.h" y "biddy_fonts.h"; nunca "board.h".
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "app.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SYS_SCREEN_W 360
#define SYS_SCREEN_H 360

/* ---- Navegacion ---- */
void sys_go_home(void);                 /* regresar al menu (se aplica al terminar el evento actual) */
bool sys_open_app(const char *id);      /* abrir otra app por id; false si no existe */

/* ---- Vibracion (respeta el ajuste "Vibracion" del usuario) ---- */
void sys_haptic(uint8_t effect);        /* efecto del DRV2605: 1 click fuerte, 7 golpe suave, 10 doble, 12 triple... */
void sys_haptic_click(void);            /* el click corto que usa la perilla */

/* ---- Hilos ---- */
/* Si tu app crea su propia tarea (red, audio...), NO toques LVGL desde ella:
 * pide que el sistema ejecute una funcion en su tarea con sys_run_on_ui(). */
typedef void (*sys_ui_fn_t)(void *arg);
bool sys_run_on_ui(sys_ui_fn_t fn, void *arg);

/* ---- Red y hora ----
 * La red se elige en la placa (Ajustes -> Wi-Fi) y se guarda en NVS; al arrancar el sistema
 * se conecta solo y pone la hora por SNTP.
 * La hora se lee con time() / localtime_r() como siempre (zona: Londres). */
bool sys_net_connected(void);           /* true si hay Wi-Fi con IP ahora mismo */
bool sys_time_synced(void);             /* true si la hora vino de internet (sigue en true sin Wi-Fi) */

typedef enum {
    SYS_WIFI_NO_NETWORK,                /* no hay red guardada */
    SYS_WIFI_CONNECTING,
    SYS_WIFI_CONNECTED,
    SYS_WIFI_WRONG_PASSWORD,
    SYS_WIFI_NOT_FOUND,                 /* la red guardada no aparece */
} sys_wifi_status_t;

typedef struct {
    char ssid[33];
    int8_t rssi;                        /* dBm: -50 muy buena, -80 mala */
    bool secure;                        /* pide contrasena */
} sys_wifi_ap_t;

sys_wifi_status_t sys_wifi_status(void);
bool sys_wifi_saved_ssid(char *out, size_t len);        /* false si no hay red guardada */
bool sys_wifi_scan_start(void);                          /* buscar redes (asincrono) */
int  sys_wifi_scan_results(sys_wifi_ap_t *out, int max); /* -1 mientras busca; luego cuantas copio */
void sys_wifi_connect(const char *ssid, const char *pass); /* guarda la red y se conecta ("" = abierta) */
void sys_wifi_forget(void);                              /* borra la red guardada */

/* ---- Standby ----
 * Tras un rato sin perilla ni touch la pantalla se apaga. La app sigue abierta y su on_tick
 * sigue corriendo, pero no recibe perilla ni touch. Un doble tap la enciende.
 * sys_wake() la enciende desde la app (por ejemplo, una alarma). */
void sys_wake(void);
bool sys_is_standby(void);
uint16_t sys_get_standby_s(void);       /* segundos sin tocar nada antes del standby; 0 = nunca */
void     sys_set_standby_s(uint16_t s); /* lo usa la app Ajustes; se guarda en NVS */

/* ---- Pantalla directa (solo apps con APP_FLAG_RAW_DISPLAY) ----
 * Copia un rectangulo de pixeles a la pantalla y espera a que termine.
 * pixels: RGB565 con los bytes invertidos (big-endian), w*h valores, filas seguidas.
 * Puede estar en PSRAM. x, y, w y h deben ser pares. */
void sys_raw_draw(int x, int y, int w, int h, const uint16_t *pixels);

/* ---- Ajustes del sistema (los usa la app Ajustes) ---- */
uint8_t sys_get_brightness(void);       /* 5-100 % */
void    sys_set_brightness(uint8_t percent);
bool    sys_get_haptic_enabled(void);
void    sys_set_haptic_enabled(bool on);
uint32_t sys_get_boot_count(void);      /* cuantas veces ha arrancado la placa (prueba de que NVS sobrevive) */
const char *sys_firmware_version(void); /* contenido de firmware/version.txt */

/* Lista de apps instaladas (en el orden del menu) */
size_t sys_app_count(void);
const app_t *sys_app_at(size_t index);

/* Utilidad: color 0xRRGGBB a RGB565 big-endian (el formato de sys_raw_draw) */
static inline uint16_t sys_rgb565(uint32_t rgb)
{
    uint16_t c = (uint16_t)(((rgb >> 8) & 0xF800) | ((rgb >> 5) & 0x07E0) | ((rgb >> 3) & 0x001F));
    return (uint16_t)((c >> 8) | (c << 8));
}

#ifdef __cplusplus
}
#endif
