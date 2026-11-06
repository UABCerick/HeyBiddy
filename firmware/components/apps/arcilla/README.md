# Arcilla (título provisional)

Juego táctico de criaturas de arcilla para dos jugadores o contra Biddy (Fácil, Medio o Difícil), estilo Fire Emblem.
Es una app de Hey Biddy: se abre desde el menú circular (icono de flechas cruzadas) y vuelve al
menú con la opción **Salir** del menú del juego. La partida queda en memoria hasta apagar la placa.

Versión de la app: `ARCILLA_VERSION` en `arcilla.c` (0.7.0).

Cómo juega Biddy, cómo se miden los niveles y qué queda listo para la personalidad: [`IA.md`](IA.md).

## Controles

| Acción | Control |
| --- | --- |
| Elegir | girar la perilla |
| Confirmar | tocar la pantalla |
| Volver / ver detalle | mantener el dedo |
| Mover el mapa | deslizar el dedo (arriba, abajo, izquierda, derecha) |

## Cómo está hecha

- `arcilla.c`: lo único que habla con el sistema. Usa `APP_FLAG_RAW_DISPLAY` (dibuja la pantalla
  ella misma con `sys_raw_draw`, sin LVGL), `sys_haptic`, `sys_go_home` y guarda idioma y
  vibración en su espacio de NVS `arcilla` (clave `cfg`). La memoria de Biddy va en la clave `brain`,
  y Biddy piensa en su propia tarea (`biddy_ai`, núcleo 0).
- `game/`: motor (`engine.c`), IA (`ai.c`), cerebro de Biddy con niveles y ejércitos (`brain.c`), interfaz (`ui.c`), dibujo (`gfx.c`), textos ES/EN
  (`texts.c`), fuentes y arte generados (`font_data.c`, `art_data.c`, `hiart_data.c`).
  Es C portable: compila igual en la placa y en el PC.
- Memoria: dos búferes de pantalla de 360×360 y una franja de 24 filas, todo en PSRAM.
- `host/`, `art/`, `licenses/`: herramientas de PC. `host/` no entra al firmware.

## Herramientas de PC (Linux o WSL, desde esta carpeta)

```bash
# pruebas del motor (37 en español y 37 en inglés)
gcc -std=c11 -O1 -Igame game/engine.c host/test_engine.c -lm -o /tmp/t && /tmp/t

# capturas de pantallas del juego (tablero, constructor, cut-ins)
gcc -std=c11 -O1 -DUI_DEBUG -Igame game/engine.c game/ai.c game/brain.c game/texts.c game/font_data.c game/gfx.c \
    game/ui.c game/art_data.c game/hiart_data.c host/shots.c -lm -o /tmp/shots && /tmp/shots /tmp

# partida completa con entradas al azar: <carpeta> <idioma 0/1> <semilla> <capturas 0/1> <mapa> [nivel de Biddy 0/1/2]
gcc -std=gnu11 -O1 -DUI_DEBUG -Igame game/engine.c game/ai.c game/brain.c game/texts.c game/font_data.c game/gfx.c \
    game/ui.c game/art_data.c game/hiart_data.c host/sim.c -lm -o /tmp/sim && /tmp/sim /tmp 0 5 1 2 1

# laboratorio de Biddy: torneo entre niveles, variedad de ejércitos, tiempos (ver IA.md)
gcc -std=c11 -O2 -Igame game/engine.c game/ai.c game/brain.c host/brain_lab.c -lm -o /tmp/lab && /tmp/lab torneo 300

# laboratorio de balance: <partidas> <mapa> <semilla> <partidas.csv> <unidades.csv> [ajustes]
gcc -std=c11 -O2 -Igame game/engine.c game/ai.c host/balance.c -lm -o /tmp/bal
```

El simulador del sistema (`bash host/sim.sh` desde `firmware/`) también guarda capturas de Arcilla
(`10_arcilla_menu`, `11_arcilla_jugar`).

## Arte

Necesita Python 3 con Pillow.

- `python3 art/export.py game` regenera `game/art_data.c/h` (unidades, bustos, terreno, torres, castillo).
- `python3 art/hiart.py game` regenera `game/hiart_data.c/h` desde `art/hires/*.png`
  (retratos, minis del mapa, splash del Chamán). Los colores de cada equipo se sacan con paletas.

## Pendiente

- Colossus como elemental de roca y Wyrmling como axolote de fuego (faltan las imágenes).
- Probar en la placa: dirección del arrastre del mapa y fluidez.
- Medir en la placa cuánto piensa Biddy en Difícil (`Biddy pensó N ms` en el monitor).

Fuentes VT323 y Jersey 10 con licencia OFL (`licenses/`).
