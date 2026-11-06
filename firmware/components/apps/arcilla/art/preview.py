from PIL import Image
from pal import rgb
import importlib, sys
mod = importlib.import_module(sys.argv[1]); D = getattr(mod, sys.argv[2]); scale = int(sys.argv[3]); out = sys.argv[4]
names = list(D.keys()); w = max(len(D[n][0]) for n in names); h = max(len(D[n]) for n in names)
cols = int(sys.argv[5]) if len(sys.argv) > 5 else len(names)
rows = (len(names) + cols - 1) // cols
im = Image.new('RGB', ((w + 2) * cols, (h + 2) * rows * 2), (86, 140, 64))
for team in (0, 1):
    for k, n in enumerate(names):
        for y, r in enumerate(D[n]):
            assert len(r) == len(D[n][0]), (n, y, len(r))
            for x, ch in enumerate(r):
                if ch != '.': im.putpixel(((k % cols) * (w + 2) + x + 1, (team * rows + k // cols) * (h + 2) + y + 1), rgb(ch, team))
im.resize((im.width * scale, im.height * scale), Image.NEAREST).save(out)
print(names)
