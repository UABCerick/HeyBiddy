# Cómo agregar una app

Una app es una carpeta en `firmware/components/apps/<id>/` más una línea en `registry.c`. No se toca nada más: el sistema la pone en el menú, le pasa la perilla y el touch, y le da su propio espacio de datos.

## 1. Crear la carpeta

La forma más rápida es copiar el Contador, que es la app más simple:

```
firmware/components/apps/counter/   →   firmware/components/apps/notes/
                       counter.c    →                        notes.c
```

Dentro del archivo cambia `APP_ID`, el nombre, el ícono, el color, la versión y el nombre de la variable (`app_counter` → `app_notes`).

El `id`:
- mide como máximo 15 letras, sin espacios (`notes`, `alarms`, `arcilla`);
- es también el *namespace* de NVS donde la app guarda sus datos;
- `system` y `launcher` están reservados.

Todos los `.c` de la carpeta (incluidas las subcarpetas) se compilan solos; las carpetas `host/` y `test/` se ignoran.

## 2. Registrarla

En `firmware/components/apps/registry.c`:

```c
extern const app_t app_notes;          // ← agregar

const app_t *const g_apps[] = {
    &app_clock,
    &app_counter,
    &app_notes,                         // ← agregar (el orden es el del menú)
    &app_settings,
};
```

## 3. El contrato

```c
const app_t app_notes = {
    .id = "notes",
    .name = "Notas",
    .version = "0.1.0",                 // se muestra en Ajustes
    .icon = LV_SYMBOL_EDIT,             // ver components/lvgl/src/font/lv_symbol_def.h
    .color = 0x4CD964,
    .tick_ms = 100,                     // cada cuánto llamar on_tick (0 = 33 ms)
    .on_enter = notes_enter,            // crea tus objetos LVGL en `screen`
    .on_knob  = notes_knob,             // +1 / -1 por cada click
    .on_touch = notes_touch,            // TAP, LONG_PRESS, SWIPE_*, PRESS, MOVE, RELEASE
    .on_tick  = notes_tick,
    .on_exit  = notes_exit,             // guarda lo pendiente
};
```

Cualquiera de las funciones puede ser `NULL`. Ponles un prefijo con el nombre de la app (`notes_enter`, no `on_exit`): `on_exit` ya existe en la librería de C y no compila.

Reglas:
- Todas las funciones corren en la tarea del sistema: puedes usar LVGL directo, pero ninguna debe tardar más de unos milisegundos.
- La pantalla que recibe `on_enter` es solo de tu app y el sistema la borra al salir; no guardes punteros a objetos LVGL después de `on_exit`.
- Para salir desde la app: `sys_go_home()`. Para abrir otra: `sys_open_app("clock")`.
- Si creas una tarea propia (red, audio), no toques LVGL desde ella: usa `sys_run_on_ui(funcion, arg)`.
- Incluye solo `app.h`, `sys.h`, `storage.h` y `biddy_fonts.h`. Nunca `board.h`.

## 4. Guardar datos

```c
int32_t n = 0;
storage_get_i32("notes", "count", &n);       // si no existe, n se queda en 0
storage_set_i32("notes", "count", n + 1);
storage_set_blob("notes", "list", &lista, sizeof(lista));
```

Usa siempre tu propio `id` como primer argumento. Los datos sobreviven a reinicios y a `idf.py flash`. Para no gastar la flash, guarda cuando algo cambió y no en cada cuadro (el Contador espera 1 s sin cambios).

## 5. Textos y fuentes

`biddy_fonts.h` trae Montserrat con acentos y ñ: `biddy_font_16`, `biddy_font_22`, `biddy_font_32`, y `biddy_font_digits_84` (solo números, `:` y `-`). Las fuentes de LVGL (`lv_font_montserrat_*`) no tienen acentos.

## 6. Apps que dibujan la pantalla ellas mismas (juegos)

Si la app tiene su propio motor de dibujo (como Arcilla), declara `APP_FLAG_RAW_DISPLAY`:

```c
.flags = APP_FLAG_RAW_DISPLAY,
```

- `on_enter` recibe `NULL` y LVGL queda en pausa mientras la app está abierta.
- Dibuja con `sys_raw_draw(x, y, w, h, pixeles)`: RGB565 con bytes invertidos (`sys_rgb565(0xRRGGBB)` convierte un color), el buffer puede estar en PSRAM, y `x`, `y`, `w`, `h` deben ser pares.
- Si la app usa el deslizamiento desde arriba para otra cosa, agrega `APP_FLAG_NO_HOME_GESTURE` y ofrece una opción "Salir" que llame `sys_go_home()`.

### Integrar Arcilla

Su motor (`components/game`) ya es portable y solo pide cuatro funciones; dentro de `apps/arcilla/` se conectan así:

| Arcilla pide | Se implementa con |
| --- | --- |
| `hal_ms()` | `esp_timer_get_time() / 1000` |
| `hal_haptic(efecto)` | `sys_haptic(efecto)` |
| `hal_save_settings(...)` | `storage_set_blob("arcilla", ...)` |
| `platform_random_byte()` | `esp_random()` |
| `board_flush()` del framebuffer | `sys_raw_draw(0, 0, 360, 360, fb)` |

Su `main.c` y su `board.c` ya no se usan: el sistema inicializa la placa. La perilla y el touch llegan por `on_knob` y `on_touch`, y "volver" en su menú principal llama `sys_go_home()`.

## 7. Probar

1. `idf.py build` debe pasar con **todas** las apps.
2. Opcional, sin la placa: `bash host/sim.sh` en WSL o Linux genera capturas en `firmware/host/capturas/`. Para incluir la app nueva, agrega sus pasos en `host/sim.c`.
3. Flashea y prueba en la placa.
4. Commit pequeño con el nombre de la app: `notes: borrar con confirmación`.
