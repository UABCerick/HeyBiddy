# Retratos: Scale3x (16 -> 48) de los sprites de tablero, con luz de borde y sombra suave
from units import U
def scale3x(g):
    h, w = len(g), len(g[0]); out = [['.'] * (w * 3) for _ in range(h * 3)]
    P = lambda x, y: g[min(max(y, 0), h - 1)][min(max(x, 0), w - 1)]
    for y in range(h):
        for x in range(w):
            A, B, C = P(x-1, y-1), P(x, y-1), P(x+1, y-1)
            D, E, F = P(x-1, y), P(x, y), P(x+1, y)
            G, H, I = P(x-1, y+1), P(x, y+1), P(x+1, y+1)
            e = [E] * 9
            if B != H and D != F:
                e[0] = D if D == B else E
                e[1] = B if (D == B and E != C) or (B == F and E != A) else E
                e[2] = F if B == F else E
                e[3] = D if (D == B and E != G) or (D == H and E != A) else E
                e[5] = F if (B == F and E != I) or (H == F and E != C) else E
                e[6] = D if D == H else E
                e[7] = H if (D == H and E != I) or (H == F and E != G) else E
                e[8] = F if H == F else E
            for k in range(9): out[y * 3 + k // 3][x * 3 + k % 3] = e[k]
    return [''.join(r) for r in out]
SHADE = {'B': 'D', 'L': 'B', 'H': 'L', 'w': 'W', 'g': 'G', 'G': 'q', 'F': 'f', 'm': 'M', 'M': 'z', 'y': 'Y', 'n': 'N', 'c': 'C', 'r': 'R', 'a': 'P'}
LIGHT = {'D': 'B', 'B': 'L', 'L': 'H', 'W': 'w', 'G': 'g', 'f': 'F', 'M': 'm', 'z': 'M', 'Y': 'y', 'N': 'n', 'q': 'G'}
def shade(g):
    h, w = len(g), len(g[0]); out = [list(r) for r in g]
    for y in range(h):
        for x in range(w):
            c = g[y][x]
            if c in '.o': continue
            # borde inferior-derecho más oscuro, superior-izquierdo con luz
            if (x + 1 < w and g[y][x + 1] == 'o') or (y + 1 < h and g[y + 1][x] == 'o'):
                out[y][x] = SHADE.get(c, c)
            elif (x > 0 and g[y][x - 1] == 'o') or (y > 0 and g[y - 1][x] == 'o'):
                out[y][x] = LIGHT.get(c, c)
    return [''.join(r) for r in out]
P = {k: shade(scale3x(v)) for k, v in U.items()}
