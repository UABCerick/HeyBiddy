# Casillas de 20x20 al estilo de los tácticos clásicos (se dibujan al doble: 40 px).
# Además: decoraciones grandes (torre de maná, castillo) que se dibujan encima del suelo.
import random
TS = 20
def blank(c, w=TS, h=TS): return [[c] * w for _ in range(h)]
def put(g, x, y, c):
    if 0 <= y < len(g) and 0 <= x < len(g[0]) and c != '.': g[y][x] = c
def stamp(g, ox, oy, rows):
    for y, row in enumerate(rows):
        for x, c in enumerate(row): put(g, ox + x, oy + y, c)
def grass(seed, flowers=False):
    r = random.Random(seed); g = blank('1')
    for _ in range(5):  # manchas más oscuras
        cx, cy, rr = r.randrange(TS), r.randrange(TS), r.randrange(2, 4)
        for y in range(cy - rr, cy + rr + 1):
            for x in range(cx - rr, cx + rr + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= rr * rr and r.random() < 0.8: put(g, x % TS, y % TS, '2')
    for _ in range(26): put(g, r.randrange(TS), r.randrange(TS), r.choice('2244'))
    for _ in range(4):  # matitas
        x, y = r.randrange(1, TS - 2), r.randrange(2, TS - 1)
        put(g, x, y, '3'); put(g, x + 1, y - 1, '3'); put(g, x + 2, y, '3'); put(g, x + 1, y, '4')
    if flowers:
        for _ in range(3): put(g, r.randrange(TS), r.randrange(TS), r.choice('wyr'))
    return g
PINE = [
"....K....",
"...KOK...",
"..KOvOK..",
"...KOK...",
"..KOOvK..",
".KOOvOOK.",
"..KOOOK..",
".KOOOvOK.",
"KOOOOOOOK",
".KKOOOKK.",
"....N....",
"...NN...."]
def forest(seed):
    g = grass(seed)
    for y in range(TS):
        for x in range(TS):
            if g[y][x] in '14': g[y][x] = '2'
    for (x, y) in [(0, 0), (10, 2), (5, 8)][: 3]: stamp(g, x, y, PINE)
    return g
MOUNT = [
"............oo......",
"...........oQQo.....",
"....oo....oQQmMo....",
"...oQQo..oQmmmMMo...",
"..oQQmMooQmmmmMMzo..",
"..oQmmMMoommjmMMzo..",
".oQmmmMMMommmmMMzzo.",
".ommjmMMzommmMMMzzo.",
"ommmmMMMzzommMMzzzzo",
"ommjmMMzzzommMMzzzzo",
"ommmMMMzzzzomMMzzzzo",
"ommMMMMzzzzzMMzzzzzo",
"ozzzzzzzzzzzzzzzzzzo",
".oooooooooooooooooo."]
def mountain(seed):
    g = grass(seed); stamp(g, 0, 5, MOUNT); return g
def wall():
    g = blank('s')
    for y in range(TS):
        for x in range(TS):
            row = (y - 4) // 4; off = 5 if row % 2 else 0
            if y < 4: continue
            if (y - 4) % 4 == 3: g[y][x] = 'S'
            elif (x + off) % 10 == 9: g[y][x] = 'S'
            elif (y - 4) % 4 == 0: g[y][x] = 'j'
    for x in range(TS):
        g[0][x] = 'o' if x % 5 in (2,) else 'j'
        for y in (1, 2): g[y][x] = 's' if x % 5 != 2 else 'S'
        g[3][x] = 'z'
    for x in range(TS): g[TS - 1][x] = 'z'
    for y in range(0, 3):
        for x in range(TS):
            if x % 5 == 2: g[y][x] = '2' if y < 2 else 'S'   # almenas
    return g
def path(seed):
    r = random.Random(seed); g = blank('e')
    for _ in range(36): put(g, r.randrange(TS), r.randrange(TS), r.choice('eEE5'))
    for _ in range(4):
        x, y = r.randrange(1, TS - 1), r.randrange(1, TS - 1); put(g, x, y, 'M'); put(g, x - 1, y, 'm'); put(g, x, y + 1, 'z')
    return g
def ruins():
    g = path(77)
    stamp(g, 2, 2, ["oooo.", "ojjSo", "osjSo", "osjSo", "osjSo", "osjSo", "osSSo", "osjSo", "ozzzo", "ooooo"])
    stamp(g, 11, 8, ["ooo..", "ojSo.", "osSo.", "ozzo.", "oooo."])
    stamp(g, 9, 15, ["ojjjSo", "ozzzzo", "oooooo"])
    return g
def water(f):
    g = blank('7')
    for y in range(TS):
        for x in range(TS):
            if (x + 2 * y + f * 3) % 9 == 0: g[y][x] = '8'
    for k in range(4):
        y = 2 + k * 5; x0 = (k * 7 + f * 3) % TS
        for i in range(5): g[y][(x0 + i) % TS] = '0' if i in (1, 2, 3) else '9'
    return g
def lava(f):
    r = random.Random(9); g = blank('U')
    for _ in range(50): put(g, r.randrange(TS), r.randrange(TS), 'R')
    for k in range(5):
        x = (2 + k * 4 + f * 2) % (TS - 2); y = (1 + k * 4 + f) % (TS - 2)
        put(g, x, y, 'l'); put(g, x + 1, y, 'y' if f else 'l'); put(g, x, y + 1, 'l'); put(g, x + 1, y + 1, 'l')
    return g
def ice():
    r = random.Random(4); g = blank('t')
    for _ in range(20): put(g, r.randrange(TS), r.randrange(TS), 'A')
    for (x, y) in [(3, 4), (4, 5), (5, 5), (6, 6), (7, 6), (12, 13), (13, 14), (14, 14), (15, 15), (15, 4), (16, 3), (17, 3)]: put(g, x, y, 'T')
    return g
def control():
    g = grass(21)
    for y in range(2, 18): put(g, 6, y, 'n')
    for x in range(5, 9): put(g, x, 18, 'N')
    put(g, 6, 1, 'y')
    for y in range(2, 10):
        for x in range(7, 17 - (y - 2) // 2 - (1 if y in (5, 6) else 0)): put(g, x, y, 'B')
    for x in range(7, 15): put(g, x, 2, 'L')
    for y in range(3, 10): put(g, 7, y, 'D')
    return g
def shrine():
    g = grass(31)
    for y in range(TS):
        for x in range(TS):
            d = (x - 9.5) ** 2 / 64 + (y - 13) ** 2 / 28
            if d <= 1: g[y][x] = 'J' if d > 0.75 else ('h' if (x + y) % 5 else 'i')
    return g
# Torre de maná: columna de piedra verdosa con un cuenco arriba; la esfera se dibuja aparte (animada)
TOWER = [
"..oooooooooo..",
".ohhhhhhhhhhJo",
"oihhJJJJJJhhio",
".oiihhhhhhiio.",
"..oiiiiiiiio..",
"...ohhhhhhJo..",
"...ohMjjMhJo..",
"...ohmMMmhJo..",
"...ohjMMjiJo..",
"...ohmjjmiJo..",
"...ohMjjMiJo..",
"...ohhiiiiJo..",
"...oihJiJiJo..",
"...ohMjjMiJo..",
"...ohmMMmiJo..",
"...ohjMMjiJo..",
"...ohmjjmiJo..",
"...ohMjjMiJo..",
"...ohhiiiiJo..",
"..ohhJihJiJJo.",
"..ohhhhhhhhJo.",
".oihhhhhhhhiJo",
"oiiJJJJJJJJiio",
"oJJJJJJJJJJJJo",
".oooooooooooo."]
# Castillo pequeño de 48x40 con techos del color del equipo
def castle():
    W, H = 48, 40; g = blank('.', W, H)
    def rect(x0, y0, x1, y1, c):
        for y in range(y0, y1):
            for x in range(x0, x1): put(g, x, y, c)
    def tower(x0, y0, w, h):
        rect(x0, y0, x0 + w, y0 + h, 's'); rect(x0, y0, x0 + 1, y0 + h, 'j'); rect(x0 + w - 1, y0, x0 + w, y0 + h, 'S')
        for y in range(y0 + 3, y0 + h, 4): rect(x0, y, x0 + w, y + 1, 'S')
        rect(x0 + w // 2 - 1, y0 + 4, x0 + w // 2 + 1, y0 + 7, 'k')
        for i in range(w // 2 + 2):  # techo cónico
            rect(x0 + w // 2 - i, y0 - 9 + i, x0 + w // 2 + i, y0 - 8 + i, 'B' if i > 1 else 'L')
            put(g, x0 + w // 2 - i, y0 - 9 + i, 'H'); put(g, x0 + w // 2 + i - 1, y0 - 9 + i, 'D')
        put(g, x0 + w // 2, y0 - 11, 'y'); put(g, x0 + w // 2, y0 - 10, 'n')
    rect(6, 18, 42, 38, 's')
    for y in range(18, 38, 4): rect(6, y + 3, 42, y + 4, 'S')
    for x in range(6, 42, 4): rect(x, 15, x + 2, 18, 's')
    tower(1, 14, 9, 24); tower(38, 14, 9, 24); tower(19, 6, 10, 32)
    rect(20, 28, 28, 38, 'k'); rect(21, 27, 27, 28, 'k'); rect(19, 28, 20, 38, 'j'); rect(28, 28, 29, 38, 'S')
    rect(0, 37, 48, 40, 'z')
    # contorno
    out = [row[:] for row in g]
    for y in range(H):
        for x in range(W):
            if g[y][x] == '.' and any(0 <= x + dx < W and 0 <= y + dy < H and g[y + dy][x + dx] != '.' for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[y][x] = 'o'
    return out
T = {
 'grass0': grass(1), 'grass1': grass(2), 'grass2': grass(3, True), 'grass3': grass(4), 'forest': forest(11), 'forest1': forest(12),
 'mountain': mountain(5), 'wall': wall(), 'ruins': ruins(), 'path': path(3),
 'water0': water(0), 'water1': water(1), 'water2': water(2), 'lava0': lava(0), 'lava1': lava(1),
 'ice': ice(), 'control': control(), 'shrine': shrine(), 'tower': [list(r) for r in TOWER], 'castle': castle()}
T = {k: [''.join(r) for r in v] for k, v in T.items()}
