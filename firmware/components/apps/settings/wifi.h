#pragma once
/* Pantalla Wi-Fi de Ajustes: buscar redes, escribir la contrasena con la perilla y conectar. */
#include <stdbool.h>
#include "app.h"

void wifi_screen_open(lv_obj_t *parent);   /* la dibuja encima de Ajustes */
bool wifi_screen_active(void);
void wifi_screen_knob(int dir);
void wifi_screen_touch(const touch_event_t *ev);
void wifi_screen_tick(void);
void wifi_screen_reset(void);              /* al salir de Ajustes (los objetos ya los borro el sistema) */
