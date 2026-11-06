/*
 * Cerebro de Biddy para Arcilla. Ver brain.h para el mapa de capas.
 *
 * Cómo se mantiene cada nivel:
 *   - Juego: cada nivel tiene su perfil de ai.c (cuánto mira hacia adelante, cuánto se equivoca,
 *     si usa bien los poderes). Medido con host/brain_lab.c (torneos entre niveles).
 *   - Ejército: se generan candidatos desde varias doctrinas, se ordenan por fuerza estimada y
 *     el nivel elige dentro de su banda de percentiles. Así varía la mezcla sin cambiar la fuerza.
 *   - Variedad: las doctrinas recientes pesan menos y entre los candidatos de la banda gana
 *     el más distinto a los últimos ejércitos (memoria guardada en la placa).
 */
#include "brain.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static float frand(void) { return (platform_random_byte() * 256 + platform_random_byte()) / 65536.0f; }
static int iabs(int v) { return v < 0 ? -v : v; }

/* ---------- Niveles ----------
 * Los números salen de host/brain_lab.c: con estos perfiles Difícil le gana a Medio y Medio a
 * Fácil con claridad (ver IA.md). Si cambian las reglas del motor, se vuelve a correr el torneo. */
const level_def_t LEVELS[LVL_COUNT] = {
    [LVL_EASY] = {
        .play = {.objective = 0.30f, .aggression = 0.10f, .caution = 0.30f, .noise = 2.0f, .use_powers = true,
                 .expand = 0, .margin = 2.0f, .power_skip = 0.40f},
        .band_lo = 0.00f, .band_hi = 0.40f, .mana_min = 14, .deploy_skill = 0.15f},
    [LVL_MEDIUM] = {
        .play = {.objective = 0.35f, .aggression = 0.12f, .caution = 0.40f, .noise = 0.9f, .use_powers = true,
                 .expand = 3, .margin = 1.0f, .power_skip = 0.15f},
        .band_lo = 0.30f, .band_hi = 0.72f, .mana_min = 17, .deploy_skill = 0.65f},
    [LVL_HARD] = {
        .play = {.objective = 0.35f, .aggression = 0.12f, .caution = 0.45f, .noise = 0.03f, .use_powers = true,
                 .expand = 140, .margin = 0.0f, .power_skip = 0.0f},   /* 140 = mira todos los primeros pasos */
        .band_lo = 0.62f, .band_hi = 1.00f, .mana_min = 19, .deploy_skill = 1.0f},
};

const brain_style_t STYLE_NEUTRAL = {.favorite = {0}, .aggression = 0, .doctrine_bias = -1};

/* ---------- Doctrinas ----------
 * Pesos por criatura, en el orden de cid_t:
 *   Larva, Guerrero, Beast, Colossus, Stalker, Shaman, Golem, Wyrmling, Cazador */
const doctrine_t DOCTRINES[DOCTRINE_COUNT] = {
    {{"Enjambre", "Swarm"},        {6, 2, 1, 0, 1, 3, 1, 0, 1}},
    {{"Muralla", "Bulwark"},       {1, 4, 0, 2, 0, 1, 5, 0, 3}},
    {{"Asalto", "Onslaught"},      {1, 2, 5, 0, 4, 0, 0, 2, 0}},
    {{"Artillería", "Artillery"},  {1, 2, 0, 0, 0, 4, 2, 3, 5}},
    {{"Titán", "Titan"},           {3, 2, 0, 6, 0, 2, 0, 0, 2}},
    {{"Sombras", "Shadows"},       {2, 0, 2, 0, 6, 1, 0, 2, 2}},
    {{"Equilibrio", "Balance"},    {2, 3, 2, 1, 2, 2, 2, 2, 2}},
};

/* Fuerza de cada criatura en un ejército: regresión logística sobre 1 707 partidas IA contra IA
 * con ejércitos al azar en los 7 mapas (host/brain_lab.c values). Es el aporte a ganar de tener
 * una más que el rival, con el presupuesto de 20 de maná ya descontado. */
static const float VALUE[C_COUNT] = {0.415f, 0.297f, 0.400f, -0.250f, 0.345f, 0.182f, -0.067f, -0.064f, 0.150f};

/* ---------- Memoria ---------- */
static brain_memory_t MEM;
static ai_profile_t PROF[LVL_COUNT];
static void build_profiles(void);
static bool PROF_READY;
void (*brain_moment_hook)(const brain_moment_t *m) = NULL;

static void build_profiles(void) {
    PROF_READY = true;
    for (int l = 0; l < LVL_COUNT; l++) {
        PROF[l] = LEVELS[l].play;
        PROF[l].aggression += MEM.style.aggression * 0.03f;
        if (PROF[l].aggression < 0) PROF[l].aggression = 0;
    }
}
static void mem_defaults(brain_memory_t *m) {
    memset(m, 0, sizeof *m);
    m->version = BRAIN_MEM_VERSION; m->level = LVL_MEDIUM; m->style = STYLE_NEUTRAL;
}
static void mem_save(void) { hal_store("brain", &MEM, sizeof MEM); }

void brain_init(void) {
    if (!hal_load("brain", &MEM, sizeof MEM) || MEM.version != BRAIN_MEM_VERSION || MEM.level >= LVL_COUNT || MEM.hist_n > BRAIN_HISTORY)
        mem_defaults(&MEM);
    build_profiles();
}
const brain_memory_t *brain_memory(void) { return &MEM; }
level_t brain_level(void) { return (level_t)MEM.level; }
void brain_set_level(level_t l) { if (l < LVL_COUNT && MEM.level != l) { MEM.level = (uint8_t)l; mem_save(); } }
void brain_set_style(const brain_style_t *s) { MEM.style = s ? *s : STYLE_NEUTRAL; build_profiles(); mem_save(); }
void brain_record_result(level_t l, bool human_won) {
    if (l >= LVL_COUNT) return;
    if (MEM.played[l] < 60000) MEM.played[l]++;
    if (human_won && MEM.human_won[l] < 60000) MEM.human_won[l]++;
    mem_save();
}
const ai_profile_t *brain_profile(level_t l) { if (!PROF_READY) build_profiles(); return &PROF[l < LVL_COUNT ? l : LVL_MEDIUM]; }
void brain_override_profile(level_t l, const ai_profile_t *p) { if (!PROF_READY) build_profiles(); if (l < LVL_COUNT) { if (p) PROF[l] = *p; else build_profiles(); } }

/* ---------- Ejército ---------- */
float brain_army_score(const army_t *a) {
    int cnt[C_COUNT] = {0}; float v = 0;
    for (int i = 0; i < a->n; i++) { cnt[a->cid[i]]++; v += VALUE[a->cid[i]]; }
    bool front = false;   /* alguien que aguante en primera línea */
    for (int c = 0; c < C_COUNT; c++) if (cnt[c] && CREATURES[c].hp >= 4 && c != C_SHAMAN && c != C_HUNTER) front = true;
    if (!front) v -= 0.4f;
    if (a->n <= 2) v -= 0.3f;
    if (cnt[C_LARVA] >= 2) v += 0.1f * (cnt[C_LARVA] - 1);     /* el Enjambre necesita compañía */
    v -= 0.04f * (RULES.mana_budget - army_cost(a));            /* maná sin gastar */
    return v;
}
static void army_counts(const army_t *a, uint8_t *cnt) { memset(cnt, 0, C_COUNT); for (int i = 0; i < a->n; i++) cnt[a->cid[i]]++; }

static void gen_from_doctrine(int d, const brain_style_t *st, army_t *a) {
    memset(a, 0, sizeof *a);
    int cnt[C_COUNT] = {0};
    for (int tries = 0; tries < 12; tries++) {
        float w[C_COUNT], tot = 0;
        for (int c = 0; c < C_COUNT; c++) {
            w[c] = 0;
            if (army_can_add(a, c) != ADD_OK) continue;
            float base = DOCTRINES[d].weight[c] + 0.2f;       /* cualquiera puede aparecer, aunque poco */
            float fav = 1.0f + 0.25f * (st ? st->favorite[c] : 0); if (fav < 0.1f) fav = 0.1f;
            w[c] = base * fav * powf(0.55f, (float)cnt[c]);   /* repetir la misma criatura cuesta */
            tot += w[c];
        }
        if (tot <= 0) break;
        float r = frand() * tot; int pick = -1;
        for (int c = 0; c < C_COUNT; c++) { if (w[c] <= 0) continue; pick = c; r -= w[c]; if (r <= 0) break; }
        army_add(a, pick); cnt[pick]++;
    }
    /* orden estable para comparar ejércitos y para la revelación */
    for (int i = 1; i < a->n; i++) for (int j = i; j > 0 && a->cid[j] < a->cid[j - 1]; j--) { uint8_t k = a->cid[j]; a->cid[j] = a->cid[j - 1]; a->cid[j - 1] = k; }
}

#define POOL_DOCS 3
#define PER_DOC 12
typedef struct { army_t a; int doc; float score; } cand_t;

static int recent_rank(const brain_memory_t *m, int d) {
    /* 0 = el último ejército usó esta doctrina, 1 = el penúltimo, ... ; 99 = no reciente */
    if (!m) return 99;
    for (int k = 0; k < m->hist_n; k++) { int idx = (m->hist_head - 1 - k + BRAIN_HISTORY) % BRAIN_HISTORY; if (m->hist_doc[idx] == d) return k; }
    return 99;
}
static int novelty(const brain_memory_t *m, const army_t *a) {
    if (!m || !m->hist_n) return 6;
    uint8_t c[C_COUNT]; army_counts(a, c); int best = 99;
    for (int k = 0; k < m->hist_n; k++) {
        int idx = (m->hist_head - 1 - k + BRAIN_HISTORY) % BRAIN_HISTORY, d = 0;
        for (int i = 0; i < C_COUNT; i++) d += iabs(c[i] - m->hist[idx][i]);
        if (k < 2) d = d * 2 / 3;   /* parecerse al último o penúltimo pesa más */
        if (d < best) best = d;
    }
    return best;
}

int brain_build_army(level_t l, const brain_memory_t *mem, const brain_style_t *style, army_t *out) {
    if (l >= LVL_COUNT) l = LVL_MEDIUM;
    const level_def_t *L = &LEVELS[l];
    /* 1) doctrinas del sorteo: las recientes pesan menos */
    int docs[POOL_DOCS]; bool used[DOCTRINE_COUNT] = {0};
    for (int k = 0; k < POOL_DOCS; k++) {
        float w[DOCTRINE_COUNT], tot = 0;
        for (int d = 0; d < DOCTRINE_COUNT; d++) {
            int r = recent_rank(mem, d);
            w[d] = used[d] ? 0 : r == 0 ? 0.08f : r == 1 ? 0.25f : r < 4 ? 0.6f : 1.0f;
            if (style && style->doctrine_bias == d) w[d] *= 3;
            tot += w[d];
        }
        float r = frand() * tot; int pick = 0;
        for (int d = 0; d < DOCTRINE_COUNT; d++) { if (w[d] <= 0) continue; pick = d; r -= w[d]; if (r <= 0) break; }
        docs[k] = pick; used[pick] = true;
    }
    /* 2) candidatos y su fuerza */
    static cand_t pool[POOL_DOCS * PER_DOC]; int n = 0;
    for (int k = 0; k < POOL_DOCS; k++) for (int i = 0; i < PER_DOC; i++) {
        cand_t *c = &pool[n];
        for (int t = 0; t < 4; t++) { gen_from_doctrine(docs[k], style, &c->a); if (army_cost(&c->a) >= L->mana_min) break; }
        c->doc = docs[k]; c->score = brain_army_score(&c->a); n++;
    }
    for (int i = 1; i < n; i++) for (int j = i; j > 0 && pool[j].score < pool[j - 1].score; j--) { cand_t k = pool[j]; pool[j] = pool[j - 1]; pool[j - 1] = k; }
    /* 3) banda del nivel */
    int lo = (int)floorf(L->band_lo * n), hi = (int)ceilf(L->band_hi * n);
    if (hi > n) hi = n;
    if (lo > n - 3) lo = n - 3;
    if (hi - lo < 3) hi = lo + 3;
    /* 4) dentro de la banda, el más distinto a lo reciente (con suerte) */
    float w[POOL_DOCS * PER_DOC], tot = 0;
    for (int i = lo; i < hi; i++) {
        int nv = novelty(mem, &pool[i].a);
        w[i] = nv == 0 ? 0.02f : (float)(nv * nv);
        if (army_cost(&pool[i].a) < L->mana_min) w[i] *= 0.2f;
        tot += w[i];
    }
    float r = frand() * tot; int pick = lo;
    for (int i = lo; i < hi; i++) { pick = i; r -= w[i]; if (r <= 0) break; }
    *out = pool[pick].a;
    return pool[pick].doc;
}

void brain_memory_push(brain_memory_t *m, const army_t *a, int doctrine) {
    army_counts(a, m->hist[m->hist_head]); m->hist_doc[m->hist_head] = (uint8_t)doctrine;
    m->hist_head = (uint8_t)((m->hist_head + 1) % BRAIN_HISTORY);
    if (m->hist_n < BRAIN_HISTORY) m->hist_n++;
}
int brain_pick_army(level_t l, army_t *out) {
    int d = brain_build_army(l, &MEM, &MEM.style, out);
    brain_memory_push(&MEM, out, d);
    mem_save();
    return d;
}

/* ---------- Despliegue ---------- */
static bool is_ranged(int cid) { return cid == C_HUNTER || cid == C_SHAMAN; }
int brain_deploy_index(const game_t *g, level_t l) {
    target_t cells[MAX_TARGETS]; int n = deploy_cells(g, cells);
    if (n <= 0) return 0;
    if (l >= LVL_COUNT) l = LVL_MEDIUM;
    if (frand() > LEVELS[l].deploy_skill) return (int)(frand() * n) % n;
    int p = g->deploy_q[g->dq_head].player, cid = g->deploy_q[g->dq_head].cid;
    const map_t *m = &MAPS[g->map];
    float enemy_row = (m->deploy[1 - p][0] + m->deploy[1 - p][1]) / 2.0f;
    int best = 0; float bv = -1e9f;
    for (int i = 0; i < n; i++) {
        int x = cells[i].x, y = cells[i].y; float v = 0;
        const terrain_t *t = terrain_of(g->tiles[y][x]);
        if (t->hazard) v -= 5;
        int dc = 99;
        for (int yy = 0; yy < g->h; yy++) for (int xx = 0; xx < g->w; xx++) if (terrain_of(g->tiles[yy][xx])->control) { int d = iabs(xx - x) + iabs(yy - y); if (d < dc) dc = d; }
        if (dc < 99) v -= 0.35f * dc;
        float fwd = -fabsf(y - enemy_row);   /* mayor = más cerca del rival */
        v += is_ranged(cid) ? -0.6f * fwd : 0.35f * fwd;
        if (is_ranged(cid) && t->cover) v += 0.8f;
        v -= 0.08f * fabsf(x - (g->w - 1) / 2.0f);
        for (int k = 0; k < g->nunits; k++) {
            const unit_t *u = &g->units[k];
            if (!u->alive || u->player != p || u->x < 0) continue;
            int d = iabs(u->x - x) + iabs(u->y - y);
            if (d == 1) { v += 0.12f; if (cid == C_LARVA && u->cid == C_LARVA) v += 0.6f; if (cid == C_GOLEM || u->cid == C_GOLEM) v += 0.3f; if (cid == C_SOLDIER) v += 0.2f; }
        }
        v += frand() * 0.05f;   /* desempate */
        if (v > bv) { bv = v; best = i; }
    }
    return best;
}

/* ---------- Decisiones ---------- */
void brain_decide(const game_t *g, level_t l, brain_decision_t *out) {
    static game_t w;
    memset(out, 0, sizeof *out); out->kind = BD_NONE; out->unit_id = -1;
    if (g->phase != PH_BATTLE) return;
    const ai_profile_t *pf = brain_profile(l);
    w = *g;
    if (w.active_id < 0) {
        ai_power_t pw;
        if (ai_choose_power(&w, pf, &pw)) {
            /* se prueba en una copia: si el motor lo rechaza, Biddy activa una criatura */
            w = *g;
            if (use_power(&w, pw.power, pw.unit_id, pw.x, pw.y, NULL)) { out->kind = BD_POWER; out->power = pw; return; }
        }
        w = *g;
        int id = ai_choose_unit(&w, pf);
        if (id >= 0) { out->kind = BD_ACTIVATE; out->unit_id = id; }
        return;
    }
    out->kind = BD_STEP; out->unit_id = w.active_id; out->step = ai_next_step(&w, pf);
}

/* Pensamiento en segundo plano: una sola tarea a la vez. */
static game_t JOB; static level_t JOB_L; static brain_decision_t RESULT;
static volatile int STATE;          /* 0 libre · 1 pensando · 2 listo */
static volatile bool CANCEL;
static void worker(void) {
    brain_decision_t d; brain_decide(&JOB, JOB_L, &d);
    RESULT = d;
    __sync_synchronize();   /* el resultado queda escrito antes de avisar (dos núcleos) */
    STATE = CANCEL ? 0 : 2; CANCEL = false;
}
bool brain_request(const game_t *g, level_t l) {
    if (STATE == 1) return false;
    JOB = *g; JOB_L = l; CANCEL = false; STATE = 1;
    hal_think_async(worker);
    return true;
}
bool brain_busy(void) { return STATE == 1; }
bool brain_poll(brain_decision_t *out) { if (STATE != 2) return false; __sync_synchronize(); *out = RESULT; STATE = 0; return true; }
void brain_cancel(void) { if (STATE == 1) CANCEL = true; else STATE = 0; }

/* ---------- Momentos ---------- */
static void emit(moment_t t, const game_t *g, int me, level_t l, int cid, int other, int amount) {
    if (!brain_moment_hook) return;
    brain_moment_t m = {t, g->round, cid, other, amount, g->vp[me], g->vp[1 - me], l};
    brain_moment_hook(&m);
}
void brain_observe(const game_t *g, const event_t *e, int me, level_t l) {
    if (!brain_moment_hook || !e) return;
    switch (e->type) {
    case EV_START: emit(MO_GAME_START, g, me, l, -1, -1, 0); break;
    case EV_ROUND: break;
    case EV_DEATH:
        if (e->player == me) emit(MO_I_LOST_UNIT, g, me, l, e->cid, -1, 0);
        else if (e->by == me) emit(MO_I_KILLED, g, me, l, e->cid, -1, 0);
        break;
    case EV_DAMAGE: if (e->by == me && e->amount >= 4) emit(MO_BIG_HIT, g, me, l, e->cid, e->tcid, e->amount); break;
    case EV_SKILLROLL:
        if (e->player == me && e->quality == Q_CRIT) emit(MO_CRIT, g, me, l, e->cid, e->tcid, 0);
        if (e->player == me && e->quality == Q_CRITFAIL) emit(MO_CRIT_FAIL, g, me, l, e->cid, e->tcid, 0);
        break;
    case EV_CAPTURE: emit(e->player == me ? MO_CAPTURE : MO_LOST_POINT, g, me, l, -1, -1, 0); break;
    case EV_POWER: if (e->player == me) emit(MO_POWER, g, me, l, -1, -1, e->power); break;
    case EV_OVER: emit(e->winner == me ? MO_I_WIN : e->winner < 0 ? MO_DRAW : MO_I_LOSE, g, me, l, -1, -1, 0); break;
    default: break;
    }
}
