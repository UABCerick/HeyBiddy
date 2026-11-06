# Vista previa del mapa (misma lógica que el dibujo del juego) para revisar el arte sin la placa.
import sys, math
from PIL import Image, ImageDraw
from pal import rgb
from tiles import T, TS
MAP = ["........", ".T.:.u..", "...#..L.", "C.S..~.R", "R.~..S.C", ".L..#...", "..u.:.T.", "........"]
if len(sys.argv) > 2: MAP = sys.argv[2].split('/')
S = 2; C = TS * S; M = 3
w, h = len(MAP[0]), len(MAP)
W, H = (w + 2 * M) * C, (h + 2 * M) * C
im = Image.new('RGB', (W, H))
def blit(name, px, py, team=0):
    for y, r in enumerate(T[name]):
        for x, ch in enumerate(r):
            if ch != '.': im.paste(rgb(ch, team), (px + x * S, py + y * S, px + x * S + S, py + y * S + S))
def h32(x, y): v = (x * 374761393 + y * 668265263) & 0xffffffff; v = (v ^ (v >> 13)) * 1274126177 & 0xffffffff; return v ^ (v >> 16)
def tile_at(x, y):
    if 0 <= x < w and 0 <= y < h: return MAP[y][x]
    r = h32(x + 50, y + 50) % 100   # paisaje fuera del tablero
    return 'T' if r < 30 else 'R' if r < 40 else '.'
NAME = {'T': 'forest', 'R': 'mountain', '#': 'wall', 'u': 'ruins', ':': 'path', '~': 'water0', 'L': 'lava0', 'I': 'ice', 'C': 'control', 'S': 'shrine'}
for y in range(-M, h + M):
    for x in range(-M, w + M):
        t = tile_at(x, y); px, py = (x + M) * C, (y + M) * C
        n = NAME.get(t, 'grass%d' % (h32(x, y) % 4))
        if t == 'T' and h32(x, y) % 2: n = 'forest1'
        blit(n, px, py)
        # bordes: agua y lava con orilla, camino con borde suave
        for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
            if t in '~LI:' and tile_at(x + dx, y + dy) != t and not (t == '~' and tile_at(x + dx, y + dy) == '~'):
                col = {'~': rgb('e'), 'L': rgb('u'), 'I': rgb('T'), ':': rgb('2')}[t]
                th = 3 if t != ':' else 2
                d = ImageDraw.Draw(im)
                if dy == -1: d.rectangle((px, py, px + C - 1, py + th - 1), fill=col)
                if dy == 1: d.rectangle((px, py + C - th, px + C - 1, py + C - 1), fill=col)
                if dx == -1: d.rectangle((px, py, px + th - 1, py + C - 1), fill=col)
                if dx == 1: d.rectangle((px + C - th, py, px + C - 1, py + C - 1), fill=col)
                if t == '~':
                    foam = rgb('0')
                    if dy == -1: d.line((px, py + th, px + C - 1, py + th), fill=foam)
                    if dy == 1: d.line((px, py + C - th - 1, px + C - 1, py + C - th - 1), fill=foam)
# marco del área de juego
d = ImageDraw.Draw(im)
d.rectangle((M * C - 1, M * C - 1, (M + w) * C, (M + h) * C), outline=(250, 240, 200))
# torres
for y in range(h):
    for x in range(w):
        if MAP[y][x] == 'S':
            px, py = (x + M) * C + C // 2 - 14, (y + M) * C + C - 50 - 2
            blit('tower', px, py)
            cx, cy = px + 14, py - 6
            for r, a in ((11, 60), (8, 120)): d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=(180, 140, 255))
            d.ellipse((cx - 5, cy - 6, cx + 3, cy + 2), fill=(230, 220, 255))
# castillos
blit('castle', (M + w // 2) * C - 48, (M + h) * C + 4, 0)
blit('castle', (M + w // 2) * C - 48, (M - 2) * C - 4, 1)
im.save(sys.argv[1])
