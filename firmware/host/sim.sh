#!/usr/bin/env bash
# Simulador de PC: compila el menu y las apps con LVGL y guarda capturas en host/capturas/.
# Requiere gcc (Linux o WSL). Correr desde firmware/:   bash host/sim.sh
set -e
cd "$(dirname "$0")/.."
mkdir -p host/build host/capturas
LV_SRCS=$(find components/lvgl/src -name '*.c')
APP_SRCS=$(find components/apps -name '*.c' -not -path '*/host/*' -not -path '*/test*/*')
gcc -O1 -std=gnu11 -w -DLV_CONF_INCLUDE_SIMPLE \
    -Ihost -Ihost/stubs -Icomponents/lvgl -Icomponents -Icomponents/ui_core -Icomponents/storage \
    -Icomponents/fonts -Icomponents/apps $(for d in components/apps/*/; do echo -I$d; done) \
    host/sim.c components/ui_core/launcher.c components/fonts/*.c $APP_SRCS $LV_SRCS \
    -lm -o host/build/sim
./host/build/sim host/capturas
# convertir a PNG si hay Python con Pillow
python3 - <<'EOF' 2>/dev/null || echo "(sin Pillow: las capturas quedan en .ppm)"
import glob, os
from PIL import Image
for p in glob.glob("host/capturas/*.ppm"):
    Image.open(p).save(p[:-4] + ".png"); os.remove(p)
EOF
