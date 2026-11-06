# Reglas de trabajo: un firmware, muchas apps

**Toda conversación o sesión que toque código para la placa sigue estas reglas.** La placa corre un solo firmware (`firmware/`) que contiene el sistema (menú, gestor de apps, almacenamiento) y todas las apps. Nunca se flashea una app suelta: se flashea el firmware completo construido desde la rama `main`.

## Partes del sistema

- `components/ui_core/`: el sistema. Contiene el **menú circular** (`launcher.c`), el **gestor de apps** y los gestos (`ui_core.c`), el **contrato de apps** (`app.h`) y los servicios para las apps (`sys.h`). Deslizar hacia abajo desde el borde de arriba siempre regresa al menú.
- `components/board/`: todo el hardware. Solo lo usa el sistema.
- `components/storage/`: datos en NVS, separados por app.
- `components/apps/<id>/`: una carpeta por app con las cinco funciones del contrato (`on_enter`, `on_knob`, `on_touch`, `on_tick`, `on_exit`).
- `components/apps/registry.c`: la lista del menú. Agregar una app consiste en crear una carpeta nueva y agregar una línea en esta lista.

## Reglas

1. **Partir siempre del repo más reciente** (`git pull` o clonar `UABCerick/HeyBiddy`). Nunca partir de un zip o de un archivo de una conversación anterior.
2. **Tocar solo la carpeta de tu app.** Si hace falta cambiar algo compartido (`ui_core/`, `board/`, `storage/`, `registry.c`, `partitions.csv`, `sdkconfig.defaults`), hay que decirlo explícitamente, explicar por qué y mantenerlo compatible con las demás apps.
3. **Datos aislados.** Cada app usa su propio namespace de NVS, igual a su `id`. Una app nunca lee ni borra el de otra.
4. **No cambiar la fila `nvs` de `partitions.csv`.** Moverla o cambiar su tamaño borra los datos de todas las apps.
5. **Compilar el firmware completo antes de entregar.** Un cambio está listo solo cuando `idf.py build` pasa con todas las apps.
6. **Commits pequeños con el nombre de la app**, por ejemplo `notes: borrar con confirmación`. Después, `git push` a `main`.
7. **Flashear solo desde el repo**, en la terminal de ESP-IDF de VS Code (`(venv)` sin punto, IDF 5.4.4) y con la placa en COM5:

   ```
   cd C:\dev\heybiddy
   git pull
   cd firmware
   idf.py -p COM5 build flash monitor
   ```

   `C:\dev\heybiddy` es el clon de GitHub. `C:\dev\hey-biddy` es una copia vieja sin git (ahí sigue el venv del servidor) y no se usa para flashear. Salir del monitor: Ctrl+]. Si la compilación falla por restos de una anterior: `idf.py fullclean` (solo borra `build/` en el PC).
8. **Nunca usar `idf.py erase-flash`** ni flashear un `.bin` con `write-flash 0x0`: borraría notas, alarmas y ajustes.
9. **Si algo se rompe:** `git log` muestra qué commit lo causó; `git revert <commit>` lo deshace sin tocar lo demás, y luego se vuelve a flashear.

## Versiones

- Cada app declara su versión en `app_t.version` y la pantalla de Ajustes la muestra.
- El firmware completo lleva su versión en `firmware/version.txt`; también aparece en Ajustes. Cuando una combinación de versiones funciona bien, se marca con una etiqueta: `git tag fw-0.1.0`.

## Estado de las apps

| App | Carpeta | Estado |
| --- | --- | --- |
| Sistema (menú, gestor, gestos, almacenamiento) | `ui_core/`, `storage/`, `board/` | v0.1.0 |
| Reloj (hora manual hasta tener Wi-Fi) | `apps/clock/` | v0.1.0 |
| Contador (ejemplo y prueba de NVS) | `apps/counter/` | v0.1.0 |
| Ajustes | `apps/settings/` | v0.1.0 |
| Arcilla (juego táctico, pantalla propia) | `apps/arcilla/` | v0.6.0 (ver `apps/arcilla/README.md`) |
| Notas, Alarmas, Biddy, Gato | `apps/notes`, `apps/alarms`, `apps/assistant`, `apps/tictactoe` | Por construir (Fase 1) |
| Wi-Fi y hora por SNTP | `components/net/` (nuevo, compartido) | Por construir (Fase 1, hito 4) |
