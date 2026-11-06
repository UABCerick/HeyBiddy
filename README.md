# Hey Biddy

Acompañante personal sobre la **Waveshare ESP32-S3-Knob-Touch-LCD-1.8**.
La placa corre **un solo firmware** que funciona como un pequeño sistema operativo: un menú circular y todas las apps adentro. Actualizar o agregar una app **no borra** las demás ni sus datos guardados.

```
firmware/        ESP-IDF 5.4.4, chip principal (ESP32-S3)
  main/          arranque
  components/
    board/       hardware: pantalla, touch, perilla, vibración, luz
    ui_core/     el "sistema": gestor de apps, menú circular, gestos
    storage/     datos guardados por app (NVS)
    apps/        una carpeta por app + registry.c (la lista del menú)
    fonts/       fuentes con acentos e íconos
    lvgl/        LVGL 8.4.0 (copia local)
    esp_lcd_sh8601/  driver de la pantalla (copia local)
  host/          simulador de PC: capturas de las pantallas sin la placa
docs/            reglas y guías
server/          cerebro en la laptop (Fase 0B en adelante)
protocol/        contrato placa ↔ servidor
```

## Actualizar la placa (cada vez)

Terminal de ESP-IDF en VS Code (Ctrl+Shift+P → *ESP-IDF: Open ESP-IDF Terminal*), cable en la orientación del S3 (**COM5**):

```
cd C:\dev\hey-biddy
git pull
cd firmware
idf.py -p COM5 build flash monitor
```

Salir del monitor: **Ctrl+]**.

`idf.py flash` reescribe el programa pero **no toca la partición `nvs`**, donde viven los datos de todas las apps. Por eso el Contador, los ajustes y (más adelante) notas y alarmas sobreviven a cada actualización.

**Nunca** uses `idf.py erase-flash` ni `esptool write-flash 0x0 algo.bin` sobre este firmware: eso sí borra todo.

### La primera vez

- El firmware ya trae LVGL y el driver de pantalla: no descarga nada al compilar.
- Si `sdkconfig` no existe, se genera solo desde `sdkconfig.defaults` (flash de 16 MB, PSRAM, particiones propias).
- **No corras `idf.py set-target`**.
- La primera vez cambia la tabla de particiones (ahora hay espacio para actualizaciones por Wi-Fi); es normal que tarde un poco más.

## Cómo se usa

| Dónde | Gesto | Qué hace |
| --- | --- | --- |
| Menú | Girar la perilla | Elegir app (el aro gira) |
| Menú | Tocar el centro | Abrir la app elegida |
| Menú | Tocar un ícono del aro | Abrir esa app |
| Cualquier app | **Deslizar hacia abajo desde el borde de arriba** | Volver al menú |

Apps incluidas: **Reloj** (mantén el dedo para poner la hora), **Contador** (prueba de que los datos sobreviven) y **Ajustes** (brillo, vibración, versiones de cada app y del firmware, número de arranques).

## Agregar o cambiar una app

Ver [`docs/nueva-app.md`](docs/nueva-app.md). En resumen: una carpeta en `firmware/components/apps/<id>/` y una línea en `registry.c`. Las reglas de trabajo están en [`docs/reglas-firmware-y-apps.md`](docs/reglas-firmware-y-apps.md).

## Volver al firmware de fábrica

```
esptool -p COM5 write-flash 0x0 C:\dev\respaldos\respaldo_s3_fabrica.bin
```
