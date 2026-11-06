#pragma once
/*
 * app.h — el contrato que cumple cada app de Hey Biddy.
 *
 * Cada app vive en firmware/components/apps/<id>/ y define UNA variable
 * `const app_t app_<id>` con estas funciones. Luego se agrega una linea en
 * firmware/components/apps/registry.c y listo: aparece en el menu.
 *
 * Todas las funciones se llaman desde la tarea del sistema (la misma que
 * dibuja la pantalla), asi que dentro de ellas puedes usar LVGL libremente.
 * Ninguna debe bloquear mas de unos milisegundos.
 */
#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Gestos del touch que el sistema reconoce y entrega a la app activa */
typedef enum {
    TOUCH_PRESS,        /* el dedo toco la pantalla */
    TOUCH_MOVE,         /* el dedo se movio (dx, dy = desplazamiento desde el ultimo MOVE) */
    TOUCH_RELEASE,      /* el dedo se levanto (siempre llega, despues de TAP o SWIPE si los hubo) */
    TOUCH_TAP,          /* toque corto sin moverse */
    TOUCH_LONG_PRESS,   /* dedo quieto 0.6 s (llega una vez, antes del RELEASE) */
    TOUCH_SWIPE_LEFT,
    TOUCH_SWIPE_RIGHT,
    TOUCH_SWIPE_UP,
    TOUCH_SWIPE_DOWN,
} touch_type_t;

typedef struct {
    touch_type_t type;
    int16_t x, y;       /* posicion actual, 0-359 */
    int16_t dx, dy;     /* en MOVE: desplazamiento; en SWIPE: desplazamiento total */
} touch_event_t;

/* Banderas de app */
#define APP_FLAG_RAW_DISPLAY     (1u << 0)  /* la app dibuja la pantalla ella misma con sys_raw_draw() (sin LVGL) */
#define APP_FLAG_NO_HOME_GESTURE (1u << 1)  /* desactiva "deslizar desde arriba = menu"; la app debe llamar sys_go_home() */

typedef struct {
    const char *id;       /* corto, sin espacios, maximo 15 letras. Es tambien el namespace de NVS de la app */
    const char *name;     /* nombre visible en el menu ("Reloj") */
    const char *version;  /* "0.1.0" — se muestra en Ajustes */
    const char *icon;     /* un LV_SYMBOL_* (ver lvgl/src/font/lv_symbol_def.h) */
    uint32_t color;       /* color de acento, 0xRRGGBB */
    uint32_t flags;       /* APP_FLAG_* */
    uint16_t tick_ms;     /* cada cuanto llamar on_tick; 0 = 33 ms (30 por segundo) */

    /* Al abrir la app. `screen` es una pantalla LVGL vacia y negra que es solo tuya;
     * crea ahi tus objetos. El sistema la borra (con todo lo que contenga) al salir.
     * En apps APP_FLAG_RAW_DISPLAY, screen es NULL. */
    void (*on_enter)(lv_obj_t *screen);
    /* Cada click de la perilla: +1 derecha, -1 izquierda. */
    void (*on_knob)(int dir);
    /* Gestos del touch (ver touch_type_t). */
    void (*on_touch)(const touch_event_t *ev);
    /* Periodicamente mientras la app esta abierta. dt_ms = tiempo desde la llamada anterior. */
    void (*on_tick)(uint32_t dt_ms);
    /* Al salir (al menu o a otra app). Guarda lo que haga falta; no uses objetos LVGL despues. */
    void (*on_exit)(void);
} app_t;

#ifdef __cplusplus
}
#endif
