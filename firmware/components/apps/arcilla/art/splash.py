# Arte de pantalla completa para el corte de especial, hecho para la pantalla redonda.
# Quita el aro gris del borde, ajusta el círculo interior a 360 px y deja fuera del círculo transparente.
# Uso: python3 art/splash.py entrada.png art/hires/salida.png
import sys, numpy as np
from PIL import Image
src, out = sys.argv[1], sys.argv[2]
im = Image.open(src).convert('RGB'); a = np.asarray(im).astype(int)
h, w = a.shape[:2]; cy, cx = h // 2, w // 2
row = a[cy]; bright = row.max(1)
# desde el borde: saltar lo negro de fuera, luego el aro claro, y quedarse donde vuelve lo oscuro
x = 0
while x < cx and bright[x] < 40: x += 1
while x < cx and bright[x] >= 40: x += 1
r = cx - x - 14
crop = im.crop((cx - r, cy - r, cx + r, cy + r)).resize((360, 360), Image.LANCZOS)
c = np.asarray(crop.convert('RGBA')).copy()
yy, xx = np.mgrid[0:360, 0:360]
c[..., 3] = np.where((xx - 179.5) ** 2 + (yy - 179.5) ** 2 <= 180 ** 2, 255, 0)
Image.fromarray(c, 'RGBA').save(out)
print(out, 'radio interior', r)
