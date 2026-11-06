/*
 * Cerebro de Biddy para Arcilla: el rival de la máquina.
 *
 * Capas (de abajo hacia arriba):
 *   ai.c     — evalúa tableros y planea pasos. No sabe de niveles.
 *   brain.c  — niveles (fácil, medio, difícil), ejército de Biddy, despliegue, pensamiento en
 *              segundo plano, memoria entre partidas y "momentos" para la futura personalidad.
 *   ui.c     — muestra lo que Biddy decide como si lo tocara un jugador.
 *
 * Todo es C portable: compila igual en la placa y en la PC (host/brain_lab.c).
 */
#pragma once
#include "ai.h"
#include "engine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---------- Niveles ---------- */
typedef enum { LVL_EASY, LVL_MEDIUM, LVL_HARD, LVL_COUNT } level_t;

typedef struct {
    ai_profile_t play;      /* cómo juega (pesos, búsqueda, errores) */
    float band_lo, band_hi; /* banda de calidad del ejército, en percentiles 0..1 de los candidatos */
    uint8_t mana_min;       /* maná mínimo que gasta en su ejército */
    float deploy_skill;     /* 0 = coloca al azar · 1 = siempre la mejor casilla */
} level_def_t;
extern const level_def_t LEVELS[LVL_COUNT];

/* ---------- Estilo: lo que la futura IA de personalidad podrá cambiar ----------
 * No cambia el nivel: solo el "gusto" de Biddy dentro de la banda de su nivel. */
typedef struct {
    int8_t favorite[C_COUNT];  /* -3..+3: cuánto le gusta cada criatura al armar ejército */
    int8_t aggression;         /* -3..+3: se suma a la agresividad del nivel (×0.03) */
    int8_t doctrine_bias;      /* -1 = ninguna · si no, doctrina preferida */
} brain_style_t;
extern const brain_style_t STYLE_NEUTRAL;

/* ---------- Doctrinas: familias de ejército para que nunca arme siempre lo mismo ---------- */
#define DOCTRINE_COUNT 7
typedef struct { tx_t name; uint8_t weight[C_COUNT]; } doctrine_t;
extern const doctrine_t DOCTRINES[DOCTRINE_COUNT];

/* ---------- Memoria (se guarda en la placa entre partidas) ---------- */
#define BRAIN_HISTORY 6
#define BRAIN_MEM_VERSION 1
typedef struct {
    uint8_t version, level;
    uint8_t hist_n, hist_head;
    uint8_t hist[BRAIN_HISTORY][C_COUNT];    /* criaturas de los últimos ejércitos de Biddy */
    uint8_t hist_doc[BRAIN_HISTORY];         /* y su doctrina */
    uint16_t played[LVL_COUNT], human_won[LVL_COUNT];
    brain_style_t style;
} brain_memory_t;

void brain_init(void);                       /* carga la memoria (hal_load) */
const brain_memory_t *brain_memory(void);
level_t brain_level(void);
void brain_set_level(level_t l);             /* y lo guarda */
void brain_set_style(const brain_style_t *s);/* gancho para la personalidad */
void brain_record_result(level_t l, bool human_won);

/* ---------- Ejército y despliegue ---------- */
/* Arma el ejército de Biddy: variado, sin repetir los últimos, y con la fuerza de su nivel.
   Devuelve la doctrina usada. Lo anota en la memoria. */
int brain_pick_army(level_t l, army_t *out);
/* Igual, pero sin tocar la memoria (para el laboratorio). */
int brain_build_army(level_t l, const brain_memory_t *mem, const brain_style_t *style, army_t *out);
float brain_army_score(const army_t *a);
void brain_memory_push(brain_memory_t *m, const army_t *a, int doctrine);  /* anota un ejército en el historial */     /* fuerza estimada (tabla de host/brain_lab.c) */
/* Índice de la casilla de deploy_cells() donde Biddy coloca su siguiente criatura. */
int brain_deploy_index(const game_t *g, level_t l);

/* ---------- Decisiones ---------- */
typedef enum { BD_NONE, BD_POWER, BD_ACTIVATE, BD_STEP } brain_kind_t;
typedef struct { brain_kind_t kind; ai_power_t power; int unit_id; ai_step_t step; } brain_decision_t;
/* Decide de forma síncrona. Sin criatura activa: poder o qué criatura activar. Con criatura activa: su paso. */
void brain_decide(const game_t *g, level_t l, brain_decision_t *out);
/* En segundo plano: copia el juego y piensa (hal_think_async). brain_poll da el resultado una vez. */
bool brain_request(const game_t *g, level_t l);
bool brain_busy(void);
bool brain_poll(brain_decision_t *out);
void brain_cancel(void);                     /* descarta lo que esté pensando (salir de la partida) */
const ai_profile_t *brain_profile(level_t l);/* perfil del nivel con el estilo aplicado */
void brain_override_profile(level_t l, const ai_profile_t *p); /* pruebas y personalidad; NULL = volver al del nivel */

/* ---------- Momentos: lo que Biddy "siente" durante la partida ----------
 * Hoy solo se emiten; la futura capa de personalidad (frases, voz, avatar) se conecta con
 * brain_moment_hook sin tocar el juego. */
typedef enum {
    MO_GAME_START, MO_MY_TURN, MO_I_KILLED, MO_I_LOST_UNIT, MO_BIG_HIT, MO_CRIT, MO_CRIT_FAIL,
    MO_CAPTURE, MO_LOST_POINT, MO_POWER, MO_I_WIN, MO_I_LOSE, MO_DRAW, MO_COUNT
} moment_t;
typedef struct { moment_t type; int round, cid, other_cid, amount; int vp_me, vp_human; level_t level; } brain_moment_t;
extern void (*brain_moment_hook)(const brain_moment_t *m);
/* La interfaz le pasa cada evento del motor; el cerebro decide si es un momento. */
void brain_observe(const game_t *g, const event_t *e, int biddy_player, level_t l);

/* ---------- Lo que pide a la plataforma ---------- */
bool hal_load(const char *key, void *data, int len);       /* true si había datos de ese tamaño */
void hal_store(const char *key, const void *data, int len);
void hal_think_async(void (*fn)(void));                    /* corre fn en segundo plano (o ya mismo) */

#ifdef __cplusplus
}
#endif
