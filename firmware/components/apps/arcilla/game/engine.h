/*
 * Motor de reglas de Proyecto Arcilla (título provisional), versión 0.3.
 * C puro, sin dependencias de la placa: compila igual en la PC (pruebas, simulación) y en el ESP32-S3.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_W 10
#define MAX_H 10
#define MAX_ARMY 6
#define MAX_UNITS (MAX_ARMY * 2)
#define EVENT_RING 96

/* ---------- Idioma ---------- */
typedef enum { LANG_ES = 0, LANG_EN = 1 } lang_t;
typedef struct { const char *es, *en; } tx_t;
void eng_set_lang(lang_t l);
lang_t eng_lang(void);
const char *L(tx_t t);

/* ---------- Reglas configurables (la simulación de balance las modifica) ---------- */
typedef struct {
    int mana_budget, max_units, ap_per_activation, max_rounds, atk_def_clamp;
    int damage_table[6];
    int cover_penalty, swarm_max, ambush_bonus, income_per_round;
    int harden_heal, haste_mov, surge_ap;
    int power_cost[4];          /* Endurecer, Impulso, Ímpetu, Reinvocar (extra sobre la mitad del coste) */
    int power_cooldown;         /* rondas completas sin poder después de usar uno (0 = uno por ronda) */
    int power_no_repeat;        /* 1 = no se puede usar el mismo poder dos veces seguidas */
    int initiative;             /* 0 = se alterna cada ronda · 1 = empieza quien va perdiendo en puntos */
    int swarm_diag;             /* 1 = el Enjambre cuenta Larvas en diagonal */
    int chain_range, chain_jumps; /* Rayo en cadena: alcance del primer objetivo y saltos extra */
    int burrow_range, rush_range;   /* Excavar y Embestida: casillas */
    int larva_max, streak_max;      /* Larvas por ejército · tope de las rachas */
    int min_damage[6];          /* por calidad */
} rules_t;
extern rules_t RULES;

typedef enum { Q_CRITFAIL, Q_FAIL, Q_PARTIAL, Q_SUCCESS, Q_IMPROVED, Q_CRIT } quality_t;
extern const tx_t QUALITY_LABEL[6];

/* ---------- Criaturas ---------- */
typedef enum { C_LARVA, C_SOLDIER, C_BEAST, C_COLOSSUS, C_STALKER, C_SHAMAN, C_GOLEM, C_WYRMLING, C_HUNTER, C_COUNT } cid_t;
/* Pasiva: siempre activa. Especial: una acción propia que cuesta PA. */
typedef enum { AB_NONE, AB_SWARM, AB_FORMATION, AB_CHARGE, AB_EXECUTE, AB_FORTRESS, AB_MARKSMAN, AB_FURY, AB_BLAZE } ability_t;
typedef enum { SP_BURROW, SP_RUSH, SP_SHOVE, SP_CRUSH, SP_SHADOW, SP_CHAIN, SP_STONEWALL, SP_BREATH, SP_LEAP } special_t;
typedef struct {
    tx_t name;
    uint8_t cost, hp, atk, def, mov, rng;
    tx_t role, desc;
    ability_t ability; tx_t ab_name, ab_text;              /* pasiva (AB_NONE si no tiene) */
    special_t special; uint8_t sp_ap; tx_t sp_name, sp_text; /* especial */
} creature_t;
extern creature_t CREATURES[C_COUNT];
const char *cname(int cid);
bool special_is_attack(int cid);   /* el especial es un ataque (lleva dados) */
bool special_is_instant(int cid);  /* el especial no necesita objetivo */

/* ---------- Poderes del invocador ---------- */
typedef enum { P_HARDEN, P_HASTE, P_SURGE, P_RESUMMON, P_COUNT } power_t;
typedef struct { tx_t name, text; } power_def_t;
extern const power_def_t POWERS[P_COUNT];
int power_cost_of(power_t p, int cid); /* cid solo cuenta para Reinvocar */
int resummon_cost(int cid);

/* ---------- Terreno y mapas ---------- */
typedef struct { char ch; tx_t name; bool pass; uint8_t cost; bool los; uint8_t cover, hazard; bool control, shrine, ice, ambush; } terrain_t;
const terrain_t *terrain_of(char ch);
typedef struct { const char *name; uint8_t w, h; tx_t blurb; const char *rows[MAX_H]; uint8_t deploy[2][2]; } map_t;
#define MAP_COUNT 7
extern const map_t MAPS[MAP_COUNT];

/* ---------- Dados ---------- */
typedef uint8_t (*byte_source_t)(void *ctx);
int d6(byte_source_t src, void *ctx);
int d6_hw(void);
uint8_t platform_random_byte(void);

/* ---------- Ejércitos ---------- */
typedef struct { uint8_t n; uint8_t cid[MAX_ARMY]; } army_t;
typedef enum { ADD_OK, ADD_MAX, ADD_MANA, ADD_LIMIT } add_result_t;
int army_cost(const army_t *a);
add_result_t army_can_add(const army_t *a, int cid);
add_result_t army_add(army_t *a, int cid);
const char *add_reason(add_result_t r);

/* ---------- Estado ---------- */
typedef struct {
    int8_t id; uint8_t cid, player;
    int8_t x, y, hp, max_hp, ap, ap_start;
    bool alive, activated, moved, defending, was_defending;
    bool hidden, walling;   /* Sombra del Stalker · Escudo de piedra del Golem */
    int8_t streak; bool used_sp; /* racha de Golem, Colossus y Wyrmling */
    int8_t atk_buff, def_buff, mov_bonus;
    uint8_t boost_ap, boost_mov;
} unit_t;

typedef enum {
    EV_START, EV_DEPLOY, EV_ROUND, EV_INCOME, EV_ACTIVATE, EV_ENDACT, EV_MOVE, EV_MANA, EV_CAPTURE, EV_CONTESTED,
    EV_ROUNDEND, EV_OVER, EV_ATTACK, EV_SKILLROLL, EV_DMGROLL, EV_MISS, EV_BLOCK, EV_DAMAGE, EV_DEATH, EV_DEFEND,
    EV_BUFF, EV_POWER, EV_CHAIN, EV_HIDE, EV_REVEAL, EV_WALL
} ev_type_t;
/* EV_MOVE.kind: 0 mover · 1 retirada · 2 excavar · 3 salto · 4 embestida · 5 empujado */
typedef struct {
    uint16_t seq; uint8_t round, type;
    int8_t player, unit_id, attacker_id, by, cid, tcid;
    int8_t x, y, fx, fy;
    int8_t amount, value, mod, total, quality, power, healed, drop, winner, kind;
    int8_t rolls[2], nrolls, boost_ap, boost_mov, vp0, vp1;
    bool crit, ability, lava, shrine, slid;
} event_t;

typedef enum { PH_DEPLOY, PH_BATTLE, PH_OVER } phase_t;
typedef enum { WIN_NONE, WIN_ELIMINATION, WIN_POINTS, WIN_COST, WIN_DRAW } win_reason_t;

typedef struct {
    int map, w, h;
    char tiles[MAX_H][MAX_W + 1];
    unit_t units[MAX_UNITS]; int nunits; int next_id;
    int round; phase_t phase; int current;
    int vp[2], mana[2];
    int8_t control_owner[MAX_H][MAX_W];
    uint8_t orbs[MAX_H][MAX_W];
    int power_ready[2];      /* primera ronda en la que el invocador puede volver a usar un poder */
    int8_t last_power[2];    /* último poder usado: no se puede repetir dos veces seguidas */
    uint8_t boost_ap[2], boost_mov[2];
    int active_id;
    int winner; win_reason_t win_reason;
    struct { uint8_t player, cid; } deploy_q[MAX_UNITS]; int dq_head, dq_len;
    event_t events[EVENT_RING]; uint16_t seq;
} game_t;

typedef struct { int8_t x, y, unit_id, cost; bool affordable; } target_t;
#define MAX_TARGETS 100

void game_new(game_t *s, int map, const army_t *a0, const army_t *a1);
int deploy_cells(const game_t *s, target_t *out);
bool place_next(game_t *s, int x, int y);
void auto_place_rest(game_t *s);
const event_t *game_event(const game_t *s, uint16_t seq);

int selectable_units(const game_t *s, int *ids);
unit_t *unit_by_id(game_t *s, int id);
const unit_t *unit_by_id_c(const game_t *s, int id);
unit_t *active_unit(game_t *s);
const unit_t *unit_at_c(const game_t *s, int x, int y);
bool activate(game_t *s, int id);
bool cancel_activation(game_t *s);
void end_activation(game_t *s);

typedef enum { A_MOVE, A_ATTACK, A_ABILITY, A_DEFEND, A_RETREAT, A_INSPECT, A_END } action_id_t;
typedef struct { action_id_t id; uint8_t ap; bool enabled; const char *name; const char *reason; } action_t;
int actions_for(game_t *s, action_t *out);
int targets_for(game_t *s, action_id_t a, target_t *out);
bool do_move(game_t *s, int x, int y, bool retreat);
bool do_defend(game_t *s);
bool is_engaged(const game_t *s, const unit_t *u);
int eff_atk(const unit_t *u);
int eff_def(const game_t *s, const unit_t *u);
int eff_mov(const unit_t *u);
int breath_range(const unit_t *u);
int slide_end(const game_t *s, const unit_t *u, int x, int y, int *ex, int *ey); /* dónde termina tras el hielo */
bool do_special(game_t *s, int x, int y);   /* especiales que no son ataque: Excavar, Salto, Sombra, Escudo */
bool rush_landing(const game_t *s, const unit_t *u, const unit_t *tgt, int *x, int *y);

typedef struct { power_t id; int cost; bool enabled; const char *reason; } power_state_t;
int powers_for(game_t *s, power_state_t *out);
int power_targets(game_t *s, power_t id, target_t *out);
int resummon_cells(game_t *s, target_t *out);
bool use_power(game_t *s, power_t id, int unit_id, int x, int y, const char **reason);

/* ---------- Combate ---------- */
typedef enum { K_ATTACK, K_CRUSH, K_BREATH, K_CHAIN, K_RUSH, K_SHOVE } attack_kind_t;
/* Una línea del desglose: qué es, de dónde sale y cuánto suma */
typedef struct { char what[40]; char why[48]; int val; bool is_set; } part_t;
typedef struct {
    bool ok; attack_kind_t kind; int attacker_id, target_id;
    int stage;
    part_t mods[4]; int nmods;
    int damage_dice;
    int skill_roll, skill_total; quality_t quality;
    part_t parts[10]; int nparts; int raw, min, final_dmg; int rolls[2];
    int chain_base;                       /* Rayo: daño antes de lo que resta cada objetivo */
    int nchain; int8_t chain_ids[3]; int8_t chain_dmg[3]; /* saltos del rayo, con el daño de cada uno */
} attack_t;
bool begin_attack(game_t *s, action_id_t a, int target_id, attack_t *p);
void resolve_skill(game_t *s, attack_t *p, int roll);
void resolve_damage(game_t *s, attack_t *p, const int *rolls);
int skill_mods(const game_t *s, const unit_t *att, const unit_t *tgt, attack_kind_t kind, part_t *m);
void compute_damage(const game_t *s, const unit_t *att, const unit_t *tgt, attack_kind_t kind, quality_t q, const int *rolls, int n, attack_t *out);
quality_t skill_quality(int natural, int total);

/* Probabilidades exactas de un ataque (para la IA y la vista previa) */
typedef struct { float p_hit, e_dmg, p_kill, p_critfail; float dmg_dist[16]; int nchain; int8_t chain_ids[3]; float chain_e[3]; } odds_t;
int chain_targets(const game_t *s, const unit_t *att, const unit_t *first, int8_t *ids); /* a quién saltaría el rayo */
int chain_hop_damage(const game_t *s, int base, int hop, const unit_t *tgt);
void attack_odds(const game_t *s, const unit_t *att, const unit_t *tgt, attack_kind_t kind, odds_t *o);
attack_kind_t kind_for(const game_t *s, const unit_t *att, action_id_t a);

void event_text(const event_t *e, char *buf, int len);

typedef void (*test_print_t)(bool pass, const char *name, const char *detail);
int run_tests(test_print_t print, int *total);

#ifdef __cplusplus
}
#endif
