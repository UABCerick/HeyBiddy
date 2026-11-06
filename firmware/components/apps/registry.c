/*
 * registry.c — la lista de apps instaladas, en el orden en que salen en el menu.
 *
 * Para agregar una app:
 *   1. crea firmware/components/apps/<id>/ con su <id>.c que define `const app_t app_<id>`
 *   2. agrega aqui su `extern` y su linea en la tabla
 * Para quitar una app del menu, borra su linea (sus datos guardados se quedan en NVS).
 */
#include <stddef.h>
#include "registry.h"

extern const app_t app_clock;
extern const app_t app_counter;
extern const app_t app_settings;

const app_t *const g_apps[] = {
    &app_clock,
    &app_counter,
    &app_settings,      /* Ajustes siempre al final */
};

const size_t g_app_count = sizeof(g_apps) / sizeof(g_apps[0]);
