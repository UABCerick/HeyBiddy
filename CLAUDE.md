# Hey Biddy — instrucciones para Claude

Antes de tocar código de la placa, lee `docs/reglas-firmware-y-apps.md` y `docs/nueva-app.md` y síguelos.

- La placa corre un solo firmware (`firmware/`, ESP-IDF **5.4.4**, no 6.x). Las apps viven en `firmware/components/apps/<id>/` y se registran en `registry.c`.
- Toca solo la carpeta de la app en la que trabajas. Si cambias algo compartido (`ui_core/`, `board/`, `storage/`, `registry.c`, `partitions.csv`, `sdkconfig.defaults`), dilo explícitamente y mantén la compatibilidad con las demás apps.
- Nunca cambies la fila `nvs` de `partitions.csv`, nunca sugieras `erase-flash` y nunca flashees un `.bin` en `0x0`.
- Antes de hacer commit, `idf.py build` debe pasar con el firmware completo. Si cambias la interfaz, revisa las capturas con `bash firmware/host/sim.sh`.
- Commits pequeños con el prefijo de la app (`notes: ...`, `sys: ...`), y después `git push` a `main`.
- El usuario es Erick y escribe en español. Sus comandos son de Windows (PowerShell, terminal de ESP-IDF, COM5 = ESP32-S3).
