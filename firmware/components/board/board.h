#pragma once
/*
 * board: todo lo que toca hardware de la placa vive aqui.
 * Las apps NO usan esto directamente: usan ui_core (sys_*) y storage.
 * Solo el sistema (ui_core) y main.c llaman a board_*.
 */
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "board_pins.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Inicializa I2C, pantalla, luz de fondo (apagada), touch, perilla y vibracion. */
esp_err_t board_init(void);

/* ---- Pantalla ---- */
/* Se llama desde una interrupcion cada vez que termina de enviarse un bloque. */
typedef void (*board_lcd_done_cb_t)(void *ctx);
void board_lcd_set_done_cb(board_lcd_done_cb_t cb, void *ctx);

/* Envia un rectangulo [x1,x2) x [y1,y2) a la pantalla (asincrono).
 * pixels: RGB565 con bytes invertidos (big-endian), en RAM interna con DMA.
 * x1 e y1 deben ser pares; x2 e y2 tambien (el controlador trabaja en pares). */
esp_err_t board_lcd_draw(int x1, int y1, int x2, int y2, const void *pixels);

/* Brillo de la luz de fondo, 0-100 %. */
void board_backlight_set(uint8_t percent);

/* ---- Touch ---- */
/* Devuelve true si hay un dedo en la pantalla y llena x, y (0-359). */
bool board_touch_read(uint16_t *x, uint16_t *y);

/* ---- Perilla ---- */
/* dir = +1 (derecha) o -1 (izquierda). Se llama desde la tarea esp_timer. */
typedef void (*board_knob_cb_t)(int dir);
void board_knob_set_cb(board_knob_cb_t cb);

/* ---- Vibracion (DRV2605) ---- */
bool board_haptic_present(void);
/* effect: numero de la biblioteca de efectos del DRV2605 (1 = click fuerte,
 * 4 = click seco, 7 = golpe suave, 10 = doble click, 12 = triple click...). */
void board_haptic_play(uint8_t effect);

#ifdef __cplusplus
}
#endif
