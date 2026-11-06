/* Arte en índices de paleta. Índice 0 = transparente; 2..5 = colores del equipo (se recolorean al dibujar). */
#pragma once
#include <stdint.h>
#define ART_NPAL 58
#define ART_TEAM0 2
typedef struct { uint8_t w, h; const uint8_t *px; } art_t;
typedef enum { TILE_GRASS0, TILE_GRASS1, TILE_GRASS2, TILE_GRASS3, TILE_FOREST, TILE_FOREST1, TILE_MOUNTAIN, TILE_WALL, TILE_RUINS, TILE_PATH, TILE_WATER0, TILE_WATER1, TILE_WATER2, TILE_LAVA0, TILE_LAVA1, TILE_ICE, TILE_CONTROL, TILE_SHRINE, TILE_TOWER, TILE_CASTLE, TILE_COUNT } tile_id_t;
#define ART_IDX_SAND 50
extern const uint16_t ART_PAL[ART_NPAL];
extern const art_t ART_UNIT[9];
extern const art_t ART_BUST[9];
extern const art_t ART_TILE[TILE_COUNT];
