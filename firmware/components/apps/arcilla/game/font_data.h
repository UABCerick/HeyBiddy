#pragma once
#include <stdint.h>
typedef struct { uint16_t cp; uint8_t w, h; int8_t xo, yo; uint8_t adv; uint32_t off; } glyph_t;
typedef struct { const glyph_t *g; uint16_t n; const uint8_t *bits; uint8_t cap, line; } font_t;
extern const font_t FONT_XS;
extern const font_t FONT_S;
extern const font_t FONT_M;
extern const font_t FONT_T1;
extern const font_t FONT_T2;
extern const font_t FONT_T3;
