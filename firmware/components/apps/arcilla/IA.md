# Biddy en Arcilla: la IA de juego

Desde la versión 0.7.0 puedes jugar Arcilla contra Biddy en tres niveles: **Fácil, Medio y Difícil**. Biddy arma su propio ejército, lo despliega y juega sus turnos con la misma interfaz que tú: se ve qué criatura activa, qué acción elige, a quién apunta y cómo lanza los dados.

## Cómo se juega contra Biddy

1. Menú → **Contra Biddy**.
2. Gira para elegir el nivel y toca. Debajo ves cuántas veces le has ganado en ese nivel.
3. Arma tu ejército como siempre. Al sellarlo, Biddy arma el suyo y se revelan los dos (abajo del nombre de Biddy aparece su doctrina y el nivel).
4. Elige el mapa y despliega. Biddy coloca sus criaturas solo. Si mantienes el dedo, tus criaturas restantes se colocan en buenas casillas.
5. En el turno de Biddy no hace falta tocar nada. Mantener el dedo abre la pausa; tocar después de que caigan sus dados pasa más rápido al resultado.

El nivel elegido, el marcador y los últimos ejércitos de Biddy se guardan en la placa (NVS `arcilla`, clave `brain`) y sobreviven a reinicios y a `idf.py flash`.

## Qué cambia entre niveles

| | Fácil | Medio | Difícil |
| --- | --- | --- | --- |
| Búsqueda | 1 paso | 2 pasos en las 3 mejores opciones | 2 pasos en todas las opciones |
| Errores | elige seguido jugadas "casi buenas" | a veces | nunca |
| Poderes del invocador | olvida usarlos casi la mitad de las veces | casi siempre | siempre que convienen |
| Ejército | banda baja de fuerza (percentil 0–40) | banda media (30–72) | banda alta (62–100) |
| Despliegue | casi al azar | bueno, con descuidos | el mejor que encuentra |

Medido con `host/brain_lab.c` (300 partidas por cruce, lados y mapas alternados):

| Cruce | Gana el primero | Gana el segundo | Empates |
| --- | --- | --- | --- |
| Fácil vs Medio | 10 % | 87 % | 3 % |
| Fácil vs Difícil | 2 % | 97 % | 0 % |
| Medio vs Difícil | 23 % | 72 % | 6 % |
| Mismo nivel contra sí mismo | ~50 % | ~50 % | — |

Que el mismo nivel quede ~50/50 confirma que ningún lado tiene ventaja.

## Ejércitos variados sin cambiar la dificultad

Biddy no repite siempre el mismo ejército:

1. **Doctrinas.** Hay 7 familias: Enjambre, Muralla, Asalto, Artillería, Titán, Sombras y Equilibrio. En cada partida se sortean 3. Las que usó hace poco tienen menos probabilidad.
2. **Candidatos.** De cada doctrina se arman 12 ejércitos al azar (36 en total) y se ordenan por fuerza estimada.
3. **Banda del nivel.** El nivel solo puede elegir dentro de su banda de percentiles. Así la mezcla cambia pero la fuerza no.
4. **Novedad.** Dentro de la banda gana el ejército más distinto a los últimos 6 de Biddy.

Con 300 ejércitos seguidos por nivel nunca salió el mismo dos veces seguidas, y aparecieron 93–103 ejércitos distintos. La fuerza media sí sube de nivel a nivel: 0.53 en Fácil, 0.99 en Medio y 1.50 en Difícil.

La fuerza de cada criatura (`VALUE` en `brain.c`) sale de una regresión sobre 1 707 partidas IA contra IA con ejércitos al azar en los 7 mapas. Si cambias costes o reglas, vuelve a calcularla (abajo).

## Archivos

| Archivo | Qué hace |
| --- | --- |
| `game/ai.c` | Evalúa tableros y planea pasos. Nuevos campos del perfil: `expand`, `margin`, `power_skip`. |
| `game/brain.c/.h` | Niveles, doctrinas, ejército, despliegue, decisiones, pensamiento en segundo plano, memoria y momentos. |
| `game/ui.c` | Escena de nivel y modo contra Biddy. Biddy usa las mismas funciones que el jugador (`confirm_target`, dados, cortes). |
| `arcilla.c` | Tarea `biddy_ai` en el núcleo 0 para pensar sin congelar la pantalla. Imprime `Biddy pensó N ms` en el monitor. Guarda la memoria con `hal_store`. |
| `host/brain_lab.c` | Laboratorio: torneos, variedad, tiempos y recálculo de `VALUE`. |

## Laboratorio (Linux o WSL, desde esta carpeta)

```bash
gcc -std=c11 -O2 -Igame game/engine.c game/ai.c game/brain.c host/brain_lab.c -lm -o /tmp/lab
/tmp/lab torneo 300        # niveles entre sí
/tmp/lab ejercito 300      # variedad de ejércitos por nivel
/tmp/lab tiempo 30         # ms por decisión de cada nivel (en la PC)

# recalcular la fuerza de las criaturas después de cambiar reglas
gcc -std=c11 -O2 -Igame game/engine.c game/ai.c host/balance.c -lm -o /tmp/bal
for m in 0 1 2 3 4 5 6; do /tmp/bal 250 $m $((m+11)) /tmp/g$m.csv /tmp/u$m.csv; done
/tmp/lab valores /tmp/g?.csv    # copia la línea VALUE a brain.c
```

En la PC, Difícil tarda unos 2 ms por decisión de media, con picos de 53 ms. En la placa es más lento (falta medirlo: mira `Biddy pensó` en el monitor). Por eso Biddy piensa en su propia tarea mientras la pantalla anima "BIDDY PIENSA…".

## Preparado para la personalidad (siguiente fase)

Nada de esto cambia el juego todavía. Son los puntos donde se conectará la IA de personalidad:

- **Momentos** (`brain_moment_hook`). Biddy emite momentos de la partida: empieza la partida, mata a una criatura, pierde una, golpe fuerte, crítico, fallo crítico, captura, pierde un punto, usa un poder, gana, pierde o empata. Incluyen ronda, criaturas, marcador y nivel. La capa de frases, voz o avatar se conecta a ese gancho sin tocar el juego.
- **Estilo** (`brain_style_t`, `brain_set_style`). Criaturas favoritas, agresividad y doctrina preferida. Cambia el gusto de Biddy dentro de la banda de su nivel, nunca su fuerza.
- **Memoria** (`brain_memory_t`). Partidas jugadas y ganadas por nivel, y los últimos ejércitos. Sirve para frases como "llevas 3 victorias seguidas" o para una dificultad adaptable más adelante.
- **Perfiles** (`brain_override_profile`). Permiten probar o ajustar un nivel desde fuera.
