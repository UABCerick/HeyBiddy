/*
 * Laboratorio de balance: partidas IA contra IA con ejércitos al azar.
 * Uso: balance <partidas> <mapa> <semilla> <salida_partidas.csv> <salida_unidades.csv> [ajustes...]
 * Ajustes: golem.cost=8  golem.atk=1  cooldown=0  pcost.2=4  noai_powers  aggr=0.2  ...
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ai.h"
#include "engine.h"

uint8_t platform_random_byte(void) { return (uint8_t)(rand() >> 7); }

static const char *CNAMES[C_COUNT] = {"larva", "soldier", "beast", "colossus", "stalker", "shaman", "golem", "wyrmling", "hunter"};
static ai_profile_t PF; static int RANDOM_P1 = 0;
static int rnd(int n) { return rand() % n; }
static void random_decision(game_t *g) {
    int ids[MAX_UNITS]; int n = selectable_units(g, ids); if (!n) return; activate(g, ids[rnd(n)]);
    int id = g->active_id;
    for (int k = 0; k < 6 && g->phase == PH_BATTLE && g->active_id == id; k++) {
        action_t acts[8]; int na = actions_for(g, acts); int en[8], ne = 0; for (int i = 0; i < na; i++) if (acts[i].enabled && acts[i].id != A_INSPECT) en[ne++] = i;
        action_t a = acts[en[rnd(ne)]];
        if (a.id == A_END) { end_activation(g); break; } if (a.id == A_DEFEND) { do_defend(g); continue; }
        target_t t[MAX_TARGETS]; int nt = targets_for(g, a.id, t); target_t tt = nt ? t[rnd(nt)] : (target_t){0};
        if (a.id == A_ABILITY && !special_is_attack(active_unit(g)->cid)) { do_special(g, tt.x, tt.y); continue; }
        if (a.id == A_MOVE || a.id == A_RETREAT) { do_move(g, tt.x, tt.y, a.id == A_RETREAT); continue; }
        attack_t p; if (!begin_attack(g, a.id, tt.unit_id, &p)) { end_activation(g); break; }
        resolve_skill(g, &p, d6_hw()); if (p.stage == 1) { int r[2] = {d6_hw(), d6_hw()}; resolve_damage(g, &p, r); }
    }
    if (g->phase == PH_BATTLE && g->active_id == id) end_activation(g);
}

static void apply_tweak(const char *t) {
    char key[32]; int val; char field[16];
    if (sscanf(t, "cooldown=%d", &val) == 1) { RULES.power_cooldown = val; return; }
    if (sscanf(t, "income=%d", &val) == 1) { RULES.income_per_round = val; return; }
    if (sscanf(t, "swarm=%d", &val) == 1) { RULES.swarm_max = val; return; }
    if (sscanf(t, "rounds=%d", &val) == 1) { RULES.max_rounds = val; return; }
    if (sscanf(t, "budget=%d", &val) == 1) { RULES.mana_budget = val; return; }
    if (sscanf(t, "init=%d", &val) == 1) { RULES.initiative = val; return; }
    if (sscanf(t, "chain=%d", &val) == 1) { RULES.chain_range = val; return; }
    if (sscanf(t, "jumps=%d", &val) == 1) { RULES.chain_jumps = val; return; }
    if (sscanf(t, "diag=%d", &val) == 1) { RULES.swarm_diag = val; return; }
    if (sscanf(t, "streak=%d", &val) == 1) { RULES.streak_max = val; return; }
    if (sscanf(t, "larvamax=%d", &val) == 1) { RULES.larva_max = val; return; }
    int k2, v2;
    if (sscanf(t, "pcost.%d=%d", &k2, &v2) == 2 && k2 >= 0 && k2 < 4) { RULES.power_cost[k2] = v2; return; }
    if (sscanf(t, "aggr=%d", &val) == 1) { PF.aggression = val / 100.0f; return; }
    if (!strcmp(t, "noai_powers")) { PF.use_powers = false; return; }
    if (!strcmp(t, "random1")) { RANDOM_P1 = 1; return; }
    if (sscanf(t, "%15[a-z].%15[a-z]=%d", key, field, &val) == 3) {
        for (int c = 0; c < C_COUNT; c++) if (!strcmp(key, CNAMES[c])) {
            if (!strcmp(field, "cost")) CREATURES[c].cost = (uint8_t)val;
            else if (!strcmp(field, "hp")) CREATURES[c].hp = (uint8_t)val;
            else if (!strcmp(field, "atk")) CREATURES[c].atk = (uint8_t)val;
            else if (!strcmp(field, "def")) CREATURES[c].def = (uint8_t)val;
            else if (!strcmp(field, "mov")) CREATURES[c].mov = (uint8_t)val;
            return;
        }
    }
    fprintf(stderr, "ajuste desconocido: %s\n", t); exit(2);
}

typedef struct { int dealt, taken, kills, acts, attacks; } ustat_t;

int main(int argc, char **argv) {
    if (argc < 6) { fprintf(stderr, "uso: balance N mapa semilla partidas.csv unidades.csv [ajustes]\n"); return 1; }
    int N = atoi(argv[1]), map = atoi(argv[2]); unsigned seed = (unsigned)atoi(argv[3]);
    FILE *fg = fopen(argv[4], "w"), *fu = fopen(argv[5], "w");
    PF = AI_DEFAULT;
    for (int i = 6; i < argc; i++) apply_tweak(argv[i]);
    srand(seed);
    fprintf(fg, "game,map,winner,reason,rounds,first_contact,attacks,kills0,kills1,dmg0,dmg1,vp0,vp1,contested,decisions");
    for (int p = 0; p < 2; p++) for (int k = 0; k < P_COUNT; k++) fprintf(fg, ",pw%d_%d", p, k);
    for (int p = 0; p < 2; p++) for (int c = 0; c < C_COUNT; c++) fprintf(fg, ",a%d_%s", p, CNAMES[c]);
    fprintf(fg, "\n");
    fprintf(fu, "game,map,player,cid,won,survived,dealt,taken,kills,acts,attacks\n");
    static game_t g;
    for (int gi = 0; gi < N; gi++) {
        army_t a[2]; ai_random_army(&a[0]); ai_random_army(&a[1]);
        game_new(&g, map, &a[0], &a[1]); ai_deploy(&g);
        ustat_t us[MAX_UNITS + 1]; memset(us, 0, sizeof us); int last_hit[MAX_UNITS + 1] = {0};
        int first_contact = 0, attacks = 0, kills[2] = {0, 0}, dmg[2] = {0, 0}, contested = 0, pw[2][P_COUNT] = {{0}}, decisions = 0;
        uint16_t seen = g.seq;
        while (g.phase == PH_BATTLE && decisions < 400) {
            if (RANDOM_P1 && g.current == 1 && g.active_id < 0) random_decision(&g); else ai_play_decision(&g, &PF);
            decisions++;
            while (seen < g.seq) {
                const event_t *e = game_event(&g, ++seen); if (!e) continue;
                switch (e->type) {
                case EV_ATTACK: attacks++; if (!first_contact) first_contact = e->round; us[e->unit_id].attacks++; break;
                case EV_ACTIVATE: us[e->unit_id].acts++; break;
                case EV_DAMAGE:
                    us[e->unit_id].taken += e->amount;
                    if (e->attacker_id >= 0) { us[e->attacker_id].dealt += e->amount; dmg[e->by] += e->amount; last_hit[e->unit_id] = e->attacker_id; }
                    break;
                case EV_DEATH: if (e->by >= 0) { kills[e->by]++; if (last_hit[e->unit_id] > 0) us[last_hit[e->unit_id]].kills++; } break;
                case EV_CONTESTED: contested++; break;
                case EV_POWER: pw[e->player][e->power]++; break;
                default: break;
                }
            }
        }
        fprintf(fg, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", gi, map, g.winner, (int)g.win_reason, g.round, first_contact, attacks, kills[0], kills[1], dmg[0], dmg[1], g.vp[0], g.vp[1], contested, decisions);
        for (int p = 0; p < 2; p++) for (int k = 0; k < P_COUNT; k++) fprintf(fg, ",%d", pw[p][k]);
        for (int p = 0; p < 2; p++) { int cnt[C_COUNT] = {0}; for (int i = 0; i < a[p].n; i++) cnt[a[p].cid[i]]++; for (int c = 0; c < C_COUNT; c++) fprintf(fg, ",%d", cnt[c]); }
        fprintf(fg, "\n");
        for (int i = 0; i < g.nunits; i++) {
            const unit_t *u = &g.units[i]; const ustat_t *t = &us[u->id];
            fprintf(fu, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", gi, map, u->player, u->cid, g.winner == u->player, u->alive, t->dealt, t->taken, t->kills, t->acts, t->attacks);
        }
    }
    fclose(fg); fclose(fu);
    return 0;
}
