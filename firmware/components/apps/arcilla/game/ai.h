/*
 * IA de Proyecto Arcilla. Usa solo la API del motor (como lo haría un jugador):
 * mira el tablero, calcula probabilidades exactas de cada ataque y elige planes de hasta 2 pasos.
 * Sirve para la simulación de balance y, más adelante, para el modo contra la máquina.
 */
#pragma once
#include "engine.h"

typedef enum { AS_END, AS_MOVE, AS_RETREAT, AS_ATTACK, AS_ABILITY, AS_DEFEND } ai_step_type_t;
typedef struct { ai_step_type_t type; int8_t x, y, target; } ai_step_t;
typedef struct { bool use; power_t power; int unit_id, x, y; } ai_power_t;

typedef struct {
    float objective;   /* cuánto le atraen los puntos de control */
    float aggression;  /* cuánto le atrae acercarse a enemigos */
    float caution;     /* cuánto teme quedar expuesto */
    float noise;       /* aleatoriedad para variar partidas */
    bool use_powers;
} ai_profile_t;
extern const ai_profile_t AI_DEFAULT;

bool ai_choose_power(game_t *s, const ai_profile_t *pf, ai_power_t *out); /* antes de activar */
int ai_choose_unit(game_t *s, const ai_profile_t *pf);                   /* id de la criatura a activar */
ai_step_t ai_next_step(game_t *s, const ai_profile_t *pf);               /* siguiente paso de la criatura activa */

/* Para la simulación: juega una decisión completa (poder o activación entera) tirando los dados con d6_hw */
void ai_play_decision(game_t *s, const ai_profile_t *pf);
void ai_random_army(army_t *a);
void ai_deploy(game_t *s);
float ai_value(const game_t *s, int player);
