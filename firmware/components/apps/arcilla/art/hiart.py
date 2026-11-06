# Arte detallado (retratos y figuras de tablero) a partir de los recortes en art/hires/*.png.
# Uso: python3 art/hiart.py game   (desde la carpeta de la app)
# Para agregar una criatura: recorta su imagen con art/cutout.py y añádela a CONFIG.
import sys, os, colorsys, numpy as np
from PIL import Image
from scipy import ndimage
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = sys.argv[1]
CIDS = ['larva', 'soldier', 'beast', 'colossus', 'stalker', 'shaman', 'golem', 'wyrmling', 'hunter']
TEAM_HUE = [215, 24]
# files: una imagen para los dos equipos (se recolorea) o una por equipo
# accent: rango de tono (grados) que toma el color del equipo; keep0: el equipo azul conserva el original
CONFIG = {
    'larva':    {'files': ['larva.png'], 'accent': (75, 140), 'keep0': False, 'crop': 72, 'min_s': 0.5, 'min_v': 0.5},
    'beast':    {'files': ['beast.png'], 'tint': 0.16, 'crop': 50},
    'colossus': {'files': ['colossus.png'], 'accent': (175, 215), 'keep0': True, 'crop': 48},
    'wyrmling': {'files': ['wyrmling.png'], 'accent': (0, 55), 'keep0': False, 'crop': 70, 'hue0': 205},
    'hunter':   {'files': ['hunter.png'], 'accent': (70, 165), 'keep0': False, 'crop': 50},
    'soldier': {'files': ['knight.png'], 'accent': (195, 255), 'keep0': True, 'crop': 52},
    'stalker': {'files': ['stalker.png'], 'accent': (250, 320), 'keep0': False, 'crop': 60},
    'shaman':  {'files': ['shaman_full.png'], 'accent': (340, 20), 'keep0': False, 'crop': 46, 'splash': 'shaman_splash.png'},
    'golem':   {'files': ['golem_blue.png', 'golem_orange.png'], 'crop': 50},
}
def in_range(h, r): return r[0] <= h <= r[1] if r[0] <= r[1] else (h >= r[0] or h <= r[1])
def recolor(pal, rng, hue, min_s=0.22, min_v=0.12):
    out = []
    for (r, g, b) in pal:
        h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
        if s > min_s and v > min_v and in_range(h * 360, rng):
            r, g, b = [round(c * 255) for c in colorsys.hsv_to_rgb(hue / 360, min(1, s * 1.05), v)]
        out.append((r, g, b))
    return out
def tint(pal, team, k):
    t = TEAM_RGB[team]
    return [tuple(round(c * (1 - k) + tc * k) for c, tc in zip(p, t)) for p in pal]
TEAM_RGB = [(60, 110, 200), (210, 100, 40)]
def team_pals(pal, cfg):
    if 'tint' in cfg: return [tint(pal, 0, cfg['tint']), tint(pal, 1, cfg['tint'])]
    kw = {'min_s': cfg.get('min_s', 0.22), 'min_v': cfg.get('min_v', 0.12)}
    p0 = pal if cfg.get('keep0') else recolor(pal, cfg['accent'], cfg.get('hue0', TEAM_HUE[0]), **kw)
    return [p0, recolor(pal, cfg['accent'], TEAM_HUE[1], **kw)]
def quantize(img, n):
    a = np.asarray(img.convert('RGBA'))
    alpha = a[..., 3] > 110
    rgb = a[..., :3].copy(); rgb[~alpha] = 0
    q = Image.fromarray(rgb, 'RGB').quantize(colors=n, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    idx = np.asarray(q).astype(np.int32) + 1; idx[~alpha] = 0
    p = q.getpalette()[:3 * n]; pal = [tuple(p[i:i + 3]) for i in range(0, len(p), 3)]
    return idx.astype(np.uint8), pal
def mini(img):
    h = 37; w = round(img.width * h / img.height)
    if w > 38: w = 38; h = round(img.height * w / img.width)
    a = np.asarray(img.resize((w, h), Image.LANCZOS)).astype(float)
    al = a[..., 3] > 110
    o = np.zeros((h + 2, w + 2), bool); o[1:-1, 1:-1] = al
    ring = ndimage.binary_dilation(o) & ~o
    out = np.zeros((h + 2, w + 2, 4), np.uint8)
    out[1:-1, 1:-1, :3] = np.clip((a[..., :3] - 128) * 1.15 + 134, 0, 255)
    out[..., 3] = o * 255; out[ring] = (26, 18, 32, 255)
    return Image.fromarray(out, 'RGBA')
def r565(c): r, g, b = c; return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
c = ['/* Generado por art/hiart.py: no editar a mano. */', '#include "hiart_data.h"', '']
splash = {k: ['NULL', 'NULL'] for k in CIDS}
full = {k: ['NULL', 'NULL'] for k in CIDS}; small = {k: ['NULL', 'NULL'] for k in CIDS}; crop = {k: 0 for k in CIDS}
def emit(name, idx, pals):
    h, w = idx.shape
    flat = idx.flatten().tolist()
    c.append(f"static const uint8_t {name}_px[{len(flat)}] = {{" + ",".join(map(str, flat)) + "};")
    for t, pal in enumerate(pals):
        c.append(f"static const uint16_t {name}_pal{t}[{len(pal) + 1}] = {{0," + ",".join(hex(r565(p)) for p in pal) + "};")
    for t in range(len(pals) if len(pals) > 1 else 1):
        c.append(f"static const hiart_t {name}_{t} = {{{w}, {h}, {name}_px, {name}_pal{t}}};")
for k, cfg in CONFIG.items():
    crop[k] = cfg['crop']
    imgs = [Image.open(os.path.join(HERE, 'hires', f)) for f in cfg['files']]
    for kind, store, n, make in (('F', full, 127, lambda im: im), ('M', small, 63, mini)):
        if len(imgs) == 2:
            for t, im in enumerate(imgs):
                idx, pal = quantize(make(im), n); emit(f"{kind}_{k}{t}", idx, [pal]); store[k][t] = f"&{kind}_{k}{t}_0"
        else:
            idx, pal = quantize(make(imgs[0]), n)
            emit(f"{kind}_{k}", idx, team_pals(pal, cfg)); store[k] = [f"&{kind}_{k}_0", f"&{kind}_{k}_1"]
for k, cfg in CONFIG.items():
    if 'splash' not in cfg: continue
    im = Image.open(os.path.join(HERE, 'hires', cfg['splash']))
    idx, pal = quantize(im, 254)
    emit(f"S_{k}", idx, team_pals(pal, cfg)); splash[k] = [f"&S_{k}_0", f"&S_{k}_1"]
c.append("const hiart_t *const HI_SPLASH[9][2] = {" + ", ".join("{" + ", ".join(splash[k]) + "}" for k in CIDS) + "};")
c.append("const hiart_t *const HI_FULL[9][2] = {" + ", ".join("{" + ", ".join(full[k]) + "}" for k in CIDS) + "};")
c.append("const hiart_t *const HI_MINI[9][2] = {" + ", ".join("{" + ", ".join(small[k]) + "}" for k in CIDS) + "};")
c.append("const uint8_t HI_CROP[9] = {" + ", ".join(str(crop[k]) for k in CIDS) + "};")
open(os.path.join(OUT, 'hiart_data.c'), 'w').write('\n'.join(c) + '\n')
open(os.path.join(OUT, 'hiart_data.h'), 'w').write('''/* Arte detallado: figura completa (retratos) y figura de tablero (32 px), con paleta por equipo. NULL = no hay. */
#pragma once
#include <stddef.h>
#include <stdint.h>
typedef struct { uint16_t w, h; const uint8_t *px; const uint16_t *pal; } hiart_t;
extern const hiart_t *const HI_FULL[9][2];
extern const hiart_t *const HI_SPLASH[9][2];  /* pantalla completa (360x360) para el corte de especial */
extern const hiart_t *const HI_MINI[9][2];
extern const uint8_t HI_CROP[9];   /* % de la altura que se ve en el corte de medio cuerpo */
''')
# vista previa de los dos equipos (opcional: tercer argumento = ruta del PNG)
if len(sys.argv) > 2:
    tiles = []
    for k, cfg in CONFIG.items():
        imgs = [Image.open(os.path.join(HERE, 'hires', f)) for f in cfg['files']]
        for t in range(2):
            im = imgs[t] if len(imgs) == 2 else imgs[0]
            idx, pal = quantize(im, 127)
            pals = [pal] if len(imgs) == 2 else team_pals(pal, cfg)
            p = pals[0] if len(imgs) == 2 else pals[t]
            rgb = np.zeros(idx.shape + (4,), np.uint8)
            for i, col in enumerate(p): rgb[idx == i + 1] = (*col, 255)
            tiles.append(Image.fromarray(rgb, 'RGBA'))
    W = sum(t.width + 4 for t in tiles); H = max(t.height for t in tiles)
    sheet = Image.new('RGBA', (W, H), (40, 44, 60, 255)); x = 0
    for t in tiles: sheet.alpha_composite(t, (x, H - t.height)); x += t.width + 4
    sheet.save(sys.argv[2])
print('ok')
