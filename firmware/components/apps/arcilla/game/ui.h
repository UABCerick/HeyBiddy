/* Interfaz del juego. Solo entiende 4 entradas: perilla izquierda/derecha, toque y toque largo. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "engine.h"

typedef enum { IN_L, IN_R, IN_TAP, IN_LONG } input_t;
typedef enum { HAP_TICK, HAP_OK, HAP_NO, HAP_THUD, HAP_HIT, HAP_MAGIC, HAP_WIN } haptic_t;

void ui_init(lang_t lang, bool haptic_on, int tests_pass, int tests_total);
void ui_input(input_t ev);
void ui_holding(bool holding, uint32_t since_ms); /* para dibujar el aro de "mantener" */
void ui_frame(void);                              /* dibuja un cuadro completo en el framebuffer */
void ui_drag(int dx, int dy);                     /* el dedo se desliza: mueve el mapa */
void ui_drag_end(void);

/* Lo implementa la plataforma (placa o simulador) */
uint32_t hal_ms(void);
void hal_haptic(haptic_t kind);
void hal_save_settings(lang_t lang, bool haptic_on);
void hal_exit(void);   /* salir del juego (en Hey Biddy: volver al menú del sistema) */
