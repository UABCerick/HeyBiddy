# Recorta una figura de un fondo liso o de cuadros sin dejar halo blanco.
# 1) detecta el fondo a resolución completa (claro y poco saturado, conectado al borde)
# 2) encoge la máscara unos píxeles para no arrastrar el fondo
# 3) reduce promediando solo píxeles de la figura (sin mezclar con blanco)
# 4) limpia el borde y le pone un contorno oscuro de 1 px, como el pixel art
# Uso: python3 art/cutout.py entrada.jpg salida.png [ancho=160]
import sys, numpy as np
from PIL import Image
from scipy import ndimage
src, out = sys.argv[1], sys.argv[2]; W = int(sys.argv[3]) if len(sys.argv) > 3 else 160
# Uso alternativo: ... salida.png ancho alto  (alto en lugar de ancho si se da un cuarto argumento 'h')
src_im = Image.open(src)
if src_im.mode == 'RGBA' and np.asarray(src_im)[..., 3].min() < 128:
    # ya trae transparencia: se usa tal cual
    rgba = np.asarray(src_im).astype(np.float32); a = rgba[..., :3]
    fig = rgba[..., 3] > 140
else:
    a = np.asarray(src_im.convert('RGB')).astype(np.float32)
    mx, mn = a.max(2), a.min(2)
    bg = (mx - mn < 34) & (mx > 160)
    lab, n = ndimage.label(bg)
    edge = set(np.unique(np.concatenate([lab[0], lab[-1], lab[:, 0], lab[:, -1]]))) - {0}
    fig = ~np.isin(lab, list(edge))
fig = ndimage.binary_opening(fig, iterations=2)
if len(sys.argv) > 4 and sys.argv[4] == 'h':  # el número es la altura final
    ys, xs = np.where(fig); W = round(W * a.shape[1] / (ys.max() - ys.min() + 1))
f = a.shape[1] / W
fig = ndimage.binary_erosion(fig, iterations=max(1, int(f * 0.6)))   # fuera el borde contaminado
labf, nf = ndimage.label(fig)
sizes = ndimage.sum(fig, labf, range(1, nf + 1))
fig = np.isin(labf, [i + 1 for i, s in enumerate(sizes) if s > f * f * 30])
H = round(a.shape[0] / f)
al = fig.astype(np.float32)
prem = a * al[..., None]
def box(x):
    im = Image.fromarray(x.astype(np.float32), 'F')
    return np.asarray(im.resize((W, H), Image.BOX))
asum = box(al)
rgb = np.dstack([box(prem[..., k]) for k in range(3)]) / np.maximum(asum, 1e-6)[..., None]
mask = asum > 0.55
mask = ndimage.binary_opening(mask) | (mask & ndimage.binary_erosion(mask))
# borde: si un píxel del borde quedó claro y gris (resto de fondo), oscurecerlo
border = mask & ~ndimage.binary_erosion(mask)
mxs, mns = rgb.max(2), rgb.min(2)
pale = border & (mxs - mns < 30) & (mxs > 150)
rgb[pale] *= 0.55
ring = ndimage.binary_dilation(mask) & ~mask
outimg = np.zeros((H, W, 4), np.uint8)
outimg[..., :3] = np.clip(rgb, 0, 255); outimg[..., 3] = mask * 255
outimg[ring] = (24, 16, 30, 255)
ys, xs = np.where(outimg[..., 3] > 0)
outimg = outimg[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
Image.fromarray(outimg, 'RGBA').save(out)
print(out, outimg.shape[1], 'x', outimg.shape[0])
