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
