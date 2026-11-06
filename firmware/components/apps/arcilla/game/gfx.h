/* Dibujo por software sobre un framebuffer RGB565 de 360×360. Sin dependencias de la placa. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "font_data.h"

#define SCR 360
#define RGB(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

typedef enum { AL_LEFT, AL_CENTER, AL_RIGHT } align_t;

void gfx_init(uint16_t *framebuffer, uint16_t *background);
uint16_t *gfx_fb(void);
void gfx_background(void);                    /* copia el fondo precalculado */
/* Dibujar en un lienzo propio (por ejemplo, la caché del tablero) y volver a la pantalla */
void gfx_target(uint16_t *buf, int w, int h);
void gfx_target_screen(void);
int gfx_tw(void);
int gfx_th(void);
const uint16_t *gfx_bg(void);                /* fondo precalculado, 360×360 */
void gfx_fill(int x, int y, int w, int h, uint16_t c);
void gfx_fill_a(int x, int y, int w, int h, uint16_t c, uint8_t alpha);
void gfx_pixel_a(int x, int y, uint16_t c, uint8_t alpha);
void gfx_arc(int cx, int cy, int r, float a0, float a1, int thick, uint16_t c, uint8_t alpha);
void gfx_global_alpha(uint8_t a);             /* multiplica todo lo que se dibuje después (255 = opaco) */

/* Texto UTF-8; y es el centro vertical de las mayúsculas, como textBaseline = middle */
int gfx_text_w(const char *s, const font_t *f);
void gfx_text(const char *s, int x, int y, const font_t *f, uint16_t c, align_t al, bool shadow);
int gfx_wrap(const char *s, int cx, int y, int max_w, int lh, const font_t *f, uint16_t c); /* centrado; devuelve líneas */
int gfx_wrap_count(const char *s, int max_w, const font_t *f);
void gfx_px(int x, int y, uint16_t c, uint8_t alpha);                 /* un píxel, con recorte */
void gfx_disc(int cx, int cy, int r, uint16_t c, uint8_t alpha);      /* círculo relleno */
void gfx_ring(int cx, int cy, int r, int thick, uint16_t c, uint8_t alpha); /* anillo */
void gfx_art(const uint8_t *px, int w, int h, float x, float y, float sc, bool flip, const uint16_t *pal, uint8_t alpha, int mode, uint16_t sil);
void gfx_art_sub(const uint8_t *px, int w, int h, int sx0, int sy0, int sw, int sh, float x, float y, float sc, bool flip, const uint16_t *pal, uint8_t alpha, int mode, uint16_t sil);
