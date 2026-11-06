# Exporta el arte a game/art_data.c/h  (uso: python3 art/export.py game) (índices de paleta, 1 byte por píxel)
from pal import PAL, IDX
from units import U
from busts import P
from tiles import T
import sys
out = sys.argv[1]
ORDER = ['larva', 'soldier', 'beast', 'colossus', 'stalker', 'shaman', 'golem', 'wyrmling', 'hunter']
TILES = list(T.keys())
def rgb565(c): r, g, b = c; return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
def arr(name, rows):
    data = [IDX[ch] for r in rows for ch in r]
    lines = [', '.join(str(v) for v in data[i:i + 32]) for i in range(0, len(data), 32)]
    return f"static const uint8_t {name}[{len(data)}] = {{\n    " + ",\n    ".join(lines) + "\n};\n"
c = ['/* Generado por art/export.py: no editar a mano. Arte original del proyecto. */', '#include "art_data.h"', '']
pal = [0 if col is None else rgb565(col) for _, col in PAL]
c.append(f"const uint16_t ART_PAL[ART_NPAL] = {{{', '.join(hex(v) for v in pal)}}};\n")
for n in ORDER: c.append(arr(f"U_{n}", U[n]))
for n in ORDER: c.append(arr(f"B_{n}", P[n]))
for n in TILES: c.append(arr(f"T_{n}", T[n]))
c.append("const art_t ART_UNIT[9] = {" + ", ".join(f"{{16, 16, U_{n}}}" for n in ORDER) + "};")
c.append("const art_t ART_BUST[9] = {" + ", ".join(f"{{32, 32, B_{n}}}" for n in ORDER) + "};")
c.append("const art_t ART_TILE[TILE_COUNT] = {" + ", ".join(f"{{{len(T[n][0])}, {len(T[n])}, T_{n}}}" for n in TILES) + "};")
open(out + '/art_data.c', 'w').write('\n'.join(c) + '\n')
h = ['/* Arte en índices de paleta. Índice 0 = transparente; 2..5 = colores del equipo (se recolorean al dibujar). */',
     '#pragma once', '#include <stdint.h>', f'#define ART_NPAL {len(PAL)}', '#define ART_TEAM0 2',
     'typedef struct { uint8_t w, h; const uint8_t *px; } art_t;',
     'typedef enum { ' + ', '.join('TILE_' + n.upper() for n in TILES) + ', TILE_COUNT } tile_id_t;',
     f"#define ART_IDX_SAND {IDX['e']}",
     'extern const uint16_t ART_PAL[ART_NPAL];', 'extern const art_t ART_UNIT[9];', 'extern const art_t ART_BUST[9];', 'extern const art_t ART_TILE[TILE_COUNT];']
open(out + '/art_data.h', 'w').write('\n'.join(h) + '\n')
print(len(PAL), TILES)
