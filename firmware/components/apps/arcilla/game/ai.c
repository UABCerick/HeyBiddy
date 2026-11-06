#include "ai.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

const ai_profile_t AI_DEFAULT = {.objective = 0.35f, .aggression = 0.12f, .caution = 0.45f, .noise = 0.25f, .use_powers = true, .expand = 10};
void (*ai_yield_hook)(void) = NULL;

static float frand(void) { return (platform_random_byte() * 256 + platform_random_byte()) / 65536.0f; }
static int iabs(int v) { return v < 0 ? -v : v; }
static int mdist(int ax, int ay, int bx, int by) { return iabs(ax - bx) + iabs(ay - by); }

/* ---------- Valoración del tablero desde el punto de vista de un jugador ---------- */
static int attack_reach(const creature_t *c) {
    if (c->special == SP_BREATH) return c->mov + 3 + RULES.streak_max / 2;
    if (c->special == SP_CHAIN) return c->mov + RULES.chain_range;
    if (c->special == SP_RUSH) return RULES.rush_range + 1;
    if (c->ability == AB_MARKSMAN) return c->mov + c->rng;
    return c->mov + c->rng;
}
static bool shield_next_to(const game_t *s, const unit_t *u) {
    if (CREATURES[u->cid].special == SP_STONEWALL) return false;
    for (int i = 0; i < s->nunits; i++) { const unit_t *o = &s->units[i]; if (o->alive && o->player == u->player && o->walling && mdist(o->x, o->y, u->x, u->y) == 1) return true; }
    return false;
}
static float threat_on(const game_t *s, const unit_t *u) {
    float t = 0;
    for (int i = 0; i < s->nunits; i++) {
        const unit_t *e = &s->units[i];
        if (!e->alive || e->player == u->player) continue;
        const creature_t *c = &CREATURES[e->cid];
        int reach = attack_reach(c);
        if (mdist(e->x, e->y, u->x, u->y) > reach) continue;
        if (u->hidden && c->special != SP_BREATH && c->special != SP_CHAIN) continue; /* oculto: solo lo toca el daño de área */
        int diff = eff_atk(e) - eff_def(s, u); if (diff > 2) diff = 2; if (diff < -2) diff = -2;
        float base = 2.2f + diff + (c->special == SP_CRUSH ? 0.8f : 0) + (c->ability == AB_CHARGE ? 0.6f : 0) + (c->special == SP_RUSH ? 0.5f : 0);
        if (CREATURES[u->cid].ability == AB_FORTRESS) base -= 1;
        if (shield_next_to(s, u)) base -= 1;
        if (u->defending) base -= 1;
        if (base < 0.4f) base = 0.4f;
        t += 0.62f * base;
    }
    return t;
}
static float unit_term(const game_t *s, const unit_t *u, const ai_profile_t *pf) {
    const creature_t *c = &CREATURES[u->cid];
    float v = c->cost * (0.35f + 0.65f * u->hp / (float)u->max_hp);
    float th = threat_on(s, u), frac = th / (float)u->hp; if (frac > 1) frac = 1;
    v -= frac * c->cost * 0.3f * pf->caution / 0.45f;
    int dctrl = 99, denemy = 99; bool on_ctrl = false, on_shrine = false;
    for (int y = 0; y < s->h; y++) for (int x = 0; x < s->w; x++) {
        const terrain_t *t = terrain_of(s->tiles[y][x]);
        if (t->control) { int d = mdist(u->x, u->y, x, y); if (d < dctrl) dctrl = d; if (!d) on_ctrl = true; }
        if (t->shrine && x == u->x && y == u->y) on_shrine = true;
    }
    for (int i = 0; i < s->nunits; i++) { const unit_t *e = &s->units[i]; if (e->alive && e->player != u->player) { int d = mdist(e->x, e->y, u->x, u->y); if (d < denemy) denemy = d; } }
    float late = 0.6f + 0.1f * s->round;
    if (dctrl < 99) v -= pf->objective * dctrl * late / 1.4f;
    if (on_ctrl && !u->hidden) v += 1.6f * late;
    if (on_shrine) v += 0.5f;
    int keep = c->special == SP_CHAIN ? RULES.chain_range : c->ability == AB_MARKSMAN ? 3 : 0; /* los de distancia no quieren pegarse */
    if (denemy < 99) v -= pf->aggression * (denemy > keep ? denemy - keep : (c->ability == AB_MARKSMAN && denemy <= 1 ? 2 : 0));
    if (u->walling) { for (int i = 0; i < s->nunits; i++) { const unit_t *o = &s->units[i]; if (o->alive && o->player == u->player && o->id != u->id && mdist(o->x, o->y, u->x, u->y) == 1) v += 0.25f; } }
    if (c->ability == AB_SWARM || c->ability == AB_FORMATION) {
        for (int i = 0; i < s->nunits; i++) {
            const unit_t *o = &s->units[i];
            if (!o->alive || o->player != u->player || o->id == u->id) continue;
            int dx = iabs(o->x - u->x), dy = iabs(o->y - u->y);
            if (c->ability == AB_SWARM && o->cid == C_LARVA && dx <= 1 && dy <= 1 && (RULES.swarm_diag || dx + dy == 1)) v += 0.35f;
            if (c->ability == AB_FORMATION && dx + dy == 1) v += 0.3f;
        }
    }
    return v;
}
static float value_with(const game_t *s, int P, const ai_profile_t *pf) {
    if (s->phase == PH_OVER) return s->winner == P ? 1000.0f : s->winner == 1 - P ? -1000.0f : 0.0f;
    float v = 0;
    for (int i = 0; i < s->nunits; i++) { const unit_t *u = &s->units[i]; if (u->alive) v += (u->player == P ? 1 : -1) * unit_term(s, u, pf); }
    int rounds_left = RULES.max_rounds - s->round + 1;
    v += (s->vp[P] - s->vp[1 - P]) * 3.0f;
    for (int y = 0; y < s->h; y++) for (int x = 0; x < s->w; x++) {
        int o = s->control_owner[y][x]; if (o < 0) continue;
        v += (o == P ? 1 : -1) * 0.7f * rounds_left;
    }
    v += (s->mana[P] - s->mana[1 - P]) * 0.45f;
    return v;
}
float ai_value(const game_t *s, int player) { return value_with(s, player, &AI_DEFAULT); }
float ai_value_with(const game_t *s, int player, const ai_profile_t *pf) { return value_with(s, player, pf); }

/* ---------- Pasos y su valor esperado ---------- */
typedef struct { ai_step_t st; float v; } cand_t;
#define MAXC 140

static int gen_steps(game_t *c, ai_step_t *out) {
    unit_t *u = active_unit(c); int n = 0;
    out[n++] = (ai_step_t){AS_END, 0, 0, -1};
    if (!u || u->ap < 1) return n;
    target_t t[MAX_TARGETS]; int k;
    k = targets_for(c, A_ATTACK, t); for (int i = 0; i < k; i++) out[n++] = (ai_step_t){AS_ATTACK, t[i].x, t[i].y, t[i].unit_id};
    if (u->ap >= CREATURES[u->cid].sp_ap) {
        if (special_is_instant(u->cid)) { action_t acts[8]; int na = actions_for(c, acts); for (int i = 0; i < na; i++) if (acts[i].id == A_ABILITY && acts[i].enabled) out[n++] = (ai_step_t){AS_ABILITY, 0, 0, -1}; }
        else { k = targets_for(c, A_ABILITY, t); for (int i = 0; i < k && n < MAXC - 40; i++) out[n++] = (ai_step_t){AS_ABILITY, t[i].x, t[i].y, t[i].unit_id}; }
    }
    out[n++] = (ai_step_t){AS_DEFEND, 0, 0, -1};
    k = targets_for(c, A_MOVE, t); for (int i = 0; i < k && n < MAXC; i++) out[n++] = (ai_step_t){AS_MOVE, t[i].x, t[i].y, -1};
    k = targets_for(c, A_RETREAT, t); for (int i = 0; i < k && n < MAXC; i++) out[n++] = (ai_step_t){AS_RETREAT, t[i].x, t[i].y, -1};
    return n;
}
/* Aplica un paso de forma determinista; los ataques usan el resultado esperado. Devuelve el valor esperado. */
static float apply_step(game_t *c, ai_step_t st, int P, const ai_profile_t *pf) {
    unit_t *u = active_unit(c);
    switch (st.type) {
    case AS_END: if (u) end_activation(c); return value_with(c, P, pf);
    case AS_DEFEND: do_defend(c); return value_with(c, P, pf);
    case AS_MOVE: do_move(c, st.x, st.y, false); return value_with(c, P, pf);
    case AS_RETREAT: do_move(c, st.x, st.y, true); return value_with(c, P, pf);
    case AS_ABILITY:
        if (u && !special_is_attack(u->cid)) { do_special(c, st.x, st.y); return value_with(c, P, pf); }
        /* fallthrough */
    case AS_ATTACK: {
        if (!u) return value_with(c, P, pf);
        action_id_t a = st.type == AS_ATTACK ? A_ATTACK : A_ABILITY;
        unit_t *tg = unit_by_id(c, st.target); if (!tg) return value_with(c, P, pf);
        if (a == A_ABILITY && CREATURES[u->cid].special == SP_RUSH) { int lx, ly; if (rush_landing(c, u, tg, &lx, &ly)) { u->x = (int8_t)lx; u->y = (int8_t)ly; u->moved = true; } }
        u->hidden = false;
        odds_t o; attack_odds(c, u, tg, kind_for(c, u, a), &o);
        int cost = a == A_ATTACK ? 1 : CREATURES[u->cid].sp_ap;
        u->ap = (int8_t)(u->ap - cost);
        int hp0 = tg->hp; float ev = 0, pnone = 1.0f;
        for (int d = 1; d < 16; d++) {
            if (o.dmg_dist[d] <= 0) continue;
            pnone -= o.dmg_dist[d];
            tg->hp = (int8_t)(hp0 - d); bool dead = tg->hp <= 0; if (dead) { tg->hp = 0; tg->alive = false; }
            ev += o.dmg_dist[d] * value_with(c, P, pf);
            tg->hp = (int8_t)hp0; tg->alive = true;
        }
        ev += pnone * value_with(c, P, pf);
        /* el rayo también daña a los saltos: se suma como cambio de valor con su daño esperado */
        if (o.nchain) {
            float v0 = value_with(c, P, pf); int8_t keep[3]; bool kal[3];
            for (int k = 0; k < o.nchain; k++) { unit_t *h = unit_by_id(c, o.chain_ids[k]); keep[k] = h->hp; kal[k] = h->alive; int d = (int)lroundf(o.chain_e[k]); h->hp = (int8_t)(h->hp - d); if (h->hp <= 0) { h->hp = 0; h->alive = false; } }
            ev += value_with(c, P, pf) - v0;
            for (int k = 0; k < o.nchain; k++) { unit_t *h = unit_by_id(c, o.chain_ids[k]); h->hp = keep[k]; h->alive = kal[k]; }
        }
        /* estado "esperado" para seguir planeando */
        int exp_d = (int)lroundf(o.e_dmg);
        if (o.p_kill > 0.5f) { tg->hp = 0; tg->alive = false; } else tg->hp = (int8_t)(hp0 - exp_d > 0 ? hp0 - exp_d : 1);
        if (u->ap <= 0) end_activation(c);
        return ev - o.p_critfail * 0.6f;
    }
    }
    return value_with(c, P, pf);
}
static float best_plan(game_t *s, int P, const ai_profile_t *pf, ai_step_t *first) {
    static game_t c1, c2;
    ai_step_t steps[MAXC]; int n = gen_steps(s, steps);
    cand_t cand[MAXC]; int nc = 0; memset(cand, 0, sizeof cand);
    for (int i = 0; i < n; i++) { c1 = *s; cand[nc].st = steps[i]; cand[nc].v = apply_step(&c1, steps[i], P, pf); nc++; }
    /* expande el segundo paso para los mejores primeros pasos (cuántos: pf->expand) */
    for (int i = 1; i < nc; i++) for (int j = i; j > 0 && cand[j].v > cand[j - 1].v; j--) { cand_t k = cand[j]; cand[j] = cand[j - 1]; cand[j - 1] = k; }
    int expand = nc < pf->expand ? nc : pf->expand;
    for (int i = 0; i < expand; i++) {
        if (cand[i].st.type == AS_END) continue;
        c1 = *s; apply_step(&c1, cand[i].st, P, pf);
        if (c1.active_id < 0 || c1.active_id != s->active_id || c1.phase != PH_BATTLE) continue;
        ai_step_t s2[MAXC]; int n2 = gen_steps(&c1, s2);
        for (int j = 1; j < n2; j++) {
            c2 = c1; float v = apply_step(&c2, s2[j], P, pf);
            if (v > cand[i].v) cand[i].v = v;   /* un primer paso vale lo mejor que permite después */
        }
    }
    int bi = 0; for (int i = 1; i < nc; i++) if (cand[i].v > cand[bi].v) bi = i;
    float best = cand[bi].v; *first = cand[bi].st;
    if (pf->margin > 0) {
        /* niveles bajos: cualquier paso "casi tan bueno" puede salir, más probable cuanto mejor sea */
        float tot = 0, w[MAXC];
        for (int i = 0; i < nc; i++) { float gap = best - cand[i].v; w[i] = gap <= pf->margin ? 1.0f - gap / pf->margin + 0.15f : 0; tot += w[i]; }
        float r = frand() * tot;
        for (int i = 0; i < nc; i++) { if (w[i] <= 0) continue; r -= w[i]; if (r <= 0) { *first = cand[i].st; return cand[i].v; } }
    }
    return best;
}

/* ---------- Decisiones ---------- */
ai_step_t ai_next_step(game_t *s, const ai_profile_t *pf) {
    ai_step_t st = {AS_END, 0, 0, -1};
    unit_t *u = active_unit(s); if (!u) return st;
    best_plan(s, u->player, pf, &st);
    return st;
}
int ai_choose_unit(game_t *s, const ai_profile_t *pf) {
    int ids[MAX_UNITS]; int n = selectable_units(s, ids); if (!n) return -1;
    static game_t c; int best_id = ids[0]; float best = -1e9f;
    /* para comparar criaturas basta una búsqueda más corta; el paso real se piensa completo después */
    ai_profile_t q = *pf; if (q.expand > 24) q.expand = 24; pf = &q;
    for (int i = 0; i < n; i++) {
        if (ai_yield_hook) ai_yield_hook();
        c = *s; activate(&c, ids[i]);
        ai_step_t st; float v = best_plan(&c, s->current, pf, &st) + frand() * pf->noise;
        if (v > best) { best = v; best_id = ids[i]; }
    }
    return best_id;
}
static bool can_reach_fight(const game_t *s, const unit_t *u, int extra_mov) {
    const creature_t *c = &CREATURES[u->cid];
    int reach = attack_reach(c) + extra_mov;
    for (int i = 0; i < s->nunits; i++) { const unit_t *e = &s->units[i]; if (e->alive && e->player != u->player && mdist(e->x, e->y, u->x, u->y) <= reach) return true; }
    return false;
}
bool ai_choose_power(game_t *s, const ai_profile_t *pf, ai_power_t *out) {
    out->use = false;
    if (!pf->use_powers) return false;
    power_state_t ps[P_COUNT]; if (!powers_for(s, ps)) return false;
    float best = 0; target_t t[MAX_TARGETS];
    if (ps[P_RESUMMON].enabled) {
        int n = power_targets(s, P_RESUMMON, t);
        for (int i = 0; i < n; i++) {
            if (!t[i].affordable) continue;
            float v = CREATURES[t[i].x].cost * 0.55f - t[i].cost * 0.35f;
            if (v > best && v > 0.6f) { target_t cells[MAX_TARGETS]; int nc = resummon_cells(s, cells); if (nc) { best = v; *out = (ai_power_t){true, P_RESUMMON, t[i].unit_id, cells[nc / 2].x, cells[nc / 2].y}; } }
        }
    }
    if (ps[P_HARDEN].enabled) {
        int n = power_targets(s, P_HARDEN, t);
        for (int i = 0; i < n; i++) {
            const unit_t *u = unit_by_id_c(s, t[i].unit_id); float th = threat_on(s, u);
            int heal = u->max_hp - u->hp; if (heal > 2) heal = 2;
            float v = (heal / (float)u->max_hp) * CREATURES[u->cid].cost * 0.65f + (th > 0 ? 0.8f : 0) - RULES.power_cost[P_HARDEN] * 0.45f;
            if (v > best && v > 0.5f) { best = v; *out = (ai_power_t){true, P_HARDEN, u->id, 0, 0}; }
        }
    }
    int ids[MAX_UNITS]; int n = selectable_units(s, ids);
    if (ps[P_SURGE].enabled) {
        for (int i = 0; i < n; i++) {
            const unit_t *u = unit_by_id_c(s, ids[i]);
            if (!can_reach_fight(s, u, 0)) continue;
            float v = CREATURES[u->cid].atk * 0.6f + 0.6f - RULES.power_cost[P_SURGE] * 0.45f;
            if (v > best && v > 0.5f) { best = v; *out = (ai_power_t){true, P_SURGE, -1, 0, 0}; }
        }
    }
    if (ps[P_HASTE].enabled) {
        for (int i = 0; i < n; i++) {
            const unit_t *u = unit_by_id_c(s, ids[i]);
            if (can_reach_fight(s, u, 0) || !can_reach_fight(s, u, RULES.haste_mov)) continue;
            float v = 1.2f - RULES.power_cost[P_HASTE] * 0.45f;
            if (v > best && v > 0.4f) { best = v; *out = (ai_power_t){true, P_HASTE, -1, 0, 0}; }
        }
    }
    if (out->use && frand() < pf->noise * 0.2f + pf->power_skip) out->use = false;
    return out->use;
}

/* ---------- Simulación ---------- */
static void exec_step(game_t *s, ai_step_t st) {
    switch (st.type) {
    case AS_END: end_activation(s); break;
    case AS_DEFEND: do_defend(s); break;
    case AS_MOVE: do_move(s, st.x, st.y, false); break;
    case AS_RETREAT: do_move(s, st.x, st.y, true); break;
    case AS_ABILITY: {
        unit_t *u = active_unit(s);
        if (u && !special_is_attack(u->cid)) { do_special(s, st.x, st.y); break; }
    }   /* fallthrough */
    case AS_ATTACK: {
        attack_t p;
        if (!begin_attack(s, st.type == AS_ATTACK ? A_ATTACK : A_ABILITY, st.target, &p)) { end_activation(s); break; }
        resolve_skill(s, &p, d6_hw());
        if (p.stage == 1) { int r[2] = {d6_hw(), d6_hw()}; resolve_damage(s, &p, r); }
        break;
    }
    }
}
void ai_play_decision(game_t *s, const ai_profile_t *pf) {
    if (s->phase != PH_BATTLE) return;
    if (s->active_id < 0) {
        ai_power_t pw;
        if (ai_choose_power(s, pf, &pw)) {
            if (use_power(s, pw.power, pw.unit_id, pw.x, pw.y, NULL)) return;
        }
        int id = ai_choose_unit(s, pf);
        if (id < 0 || !activate(s, id)) return;
    }
    int guard = 0, id = s->active_id;
    while (s->phase == PH_BATTLE && s->active_id == id && guard++ < 8) {
        ai_step_t st = ai_next_step(s, pf);
        exec_step(s, st);
    }
    if (s->phase == PH_BATTLE && s->active_id == id) end_activation(s);
}
void ai_random_army(army_t *a) {
    memset(a, 0, sizeof *a);
    for (int tries = 0; tries < 60; tries++) {
        int cid = platform_random_byte() % C_COUNT;
        army_add(a, cid);
        bool any = false; for (int c = 0; c < C_COUNT; c++) if (army_can_add(a, c) == ADD_OK) any = true;
        if (!any) break;
    }
}
void ai_deploy(game_t *s) {
    while (s->phase == PH_DEPLOY) {
        target_t cells[MAX_TARGETS]; int n = deploy_cells(s, cells);
        if (!n) { auto_place_rest(s); break; }
        int i = platform_random_byte() % n;
        place_next(s, cells[i].x, cells[i].y);
    }
}
