/* Arte detallado: figura completa (retratos) y figura de tablero (32 px), con paleta por equipo. NULL = no hay. */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct { uint16_t w, h; const uint8_t *px; const uint16_t *pal; } hiart_t;
extern const hiart_t *const HI_FULL[9][2];
extern const hiart_t *const HI_SPLASH[9][2];  /* pantalla completa (360x360) para el corte de especial */
extern const hiart_t *const HI_MINI[9][2];
extern const uint8_t HI_CROP[9];   /* % de la altura que se ve en el corte de medio cuerpo */
