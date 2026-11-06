/* Motor de reglas 0.3 */
#include "engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Idioma ---------- */
static lang_t LANG = LANG_ES;
void eng_set_lang(lang_t l) { LANG = l; }
lang_t eng_lang(void) { return LANG; }
const char *L(tx_t t) { return (LANG == LANG_EN && t.en) ? t.en : t.es; }
#define TR(es, en) (LANG == LANG_EN ? (en) : (es))

/* ---------- Reglas ---------- */
rules_t RULES = {
    .mana_budget = 20, .max_units = 6, .ap_per_activation = 2, .max_rounds = 8, .atk_def_clamp = 2,
    .damage_table = {1, 1, 2, 2, 3, 4},
    .cover_penalty = 1, .swarm_max = 2, .ambush_bonus = 1, .income_per_round = 1,
    .harden_heal = 2, .haste_mov = 2, .surge_ap = 1,
    .power_cost = {2, 1, 3, 2},   /* Endurecer 2 · Impulso 1 · Ímpetu 3 · Reinvocar: mitad del coste + 2 */
    .power_cooldown = 0,
    .power_no_repeat = 1,
    .initiative = 1,
    .swarm_diag = 0,
    .chain_range = 3, .chain_jumps = 2,
    .burrow_range = 3, .rush_range = 4,
    .larva_max = 3, .streak_max = 2,
    .min_damage = {0, 0, 0, 1, 1, 2},
};
const tx_t QUALITY_LABEL[6] = {
    {"Fallo crítico", "Critical fail"}, {"Fallo", "Fail"}, {"Éxito parcial", "Partial success"},
    {"Éxito", "Success"}, {"Éxito mejorado", "Strong success"}, {"Éxito crítico", "Critical success"},
};

creature_t CREATURES[C_COUNT] = {
    {{"Larva", "Larva"}, 2, 2, 1, 1, 3, 1, {"Enjambre", "Swarm"}, {"Bolita de arcilla que nunca pelea sola.", "A clay pellet that never fights alone."},
     AB_SWARM, {"Enjambre", "Swarm"}, {"+1 a su tirada de ataque por cada Larva aliada a su lado (máx. +2).", "+1 to its attack roll for each allied Larva next to it (max +2)."},
     SP_BURROW, 1, {"Excavar", "Burrow"}, {"Se entierra y sale en una casilla libre a 3 o menos, aunque esté trabada.", "Burrows and pops up on a free square within 3, even when engaged."}},
    {{"Guerrero", "Warrior"}, 4, 4, 2, 2, 2, 1, {"Infantería", "Infantry"}, {"Caballero de armadura con el escudo del león.", "An armored knight bearing the lion shield."},
     AB_FORMATION, {"Formación", "Formation"}, {"+1 DEF si tiene una criatura aliada a su lado.", "+1 DEF while an allied creature is next to it."},
     SP_RUSH, 2, {"Embestida", "Charge"}, {"Corre en línea recta hasta un enemigo a 4 casillas o menos y lo golpea con +1 de daño.", "Runs in a straight line to an enemy up to 4 squares away and hits it for +1 damage."}},
    {{"Beast", "Beast"}, 6, 6, 3, 2, 3, 1, {"Asalto", "Assault"}, {"Embiste primero y pregunta después.", "Charges first, asks later."},
     AB_CHARGE, {"Carga brutal", "Brutal charge"}, {"+1 daño si se movió durante esta activación.", "+1 damage if it moved during this activation."},
     SP_SHOVE, 2, {"Derribar", "Shove"}, {"Golpea y empuja 1 casilla. Si no hay hueco, +1 de daño. Puede tirar al rival a la lava.", "Hits and pushes 1 square. If there is no room, +1 damage. Can push foes into lava."}},
    {{"Colossus", "Colossus"}, 9, 10, 4, 3, 1, 1, {"Tanque", "Tank"}, {"Una montaña de plastilina con mal genio.", "A bad-tempered mountain of clay."},
     AB_FURY, {"Furia", "Fury"}, {"+1 daño por cada Aplastamiento en activaciones seguidas (máx. +2). Se pierde si una activación no aplasta.", "+1 damage for each Crush in back-to-back activations (max +2). Lost if an activation doesn't crush."},
     SP_CRUSH, 2, {"Aplastamiento", "Crush"}, {"Ataque cuerpo a cuerpo: tira 2 dados de daño y se queda con el mayor.", "Melee attack: roll 2 damage dice and keep the higher."}},
    {{"Stalker", "Stalker"}, 5, 3, 3, 1, 3, 1, {"Asesino", "Assassin"}, {"Se escurre entre las grietas del molde.", "Slips through the cracks in the mold."},
     AB_EXECUTE, {"Ejecución", "Execution"}, {"+2 a su tirada de ataque si el objetivo no tiene aliados a su lado.", "+2 to its attack roll if the target has no allies next to it."},
     SP_SHADOW, 1, {"Sombra", "Shadow"}, {"Se vuelve transparente: nadie puede apuntarle hasta que ataque o lo toque un ataque de área. Oculto no cuenta para puntos.", "Turns see-through: nobody can target it until it attacks or area damage touches it. Hidden, it doesn't count for points."}},
    {{"Shaman", "Shaman"}, 4, 3, 1, 1, 2, 1, {"Hechicero", "Caster"}, {"Amasa maná azul entre los dedos.", "Kneads blue mana between its fingers."},
     AB_NONE, {"", ""}, {"", ""},
     SP_CHAIN, 2, {"Rayo en cadena", "Chain lightning"}, {"Rayo a un enemigo a 3 casillas o menos. Ignora ATK y DEF y salta a enemigos pegados (lado o diagonal), hasta 3, con 1 menos en cada salto.", "Bolt at an enemy within 3 squares. Ignores ATK and DEF and jumps to touching enemies (side or diagonal), up to 3, losing 1 per jump."}},
    {{"Golem", "Golem"}, 7, 7, 1, 4, 1, 1, {"Defensor", "Defender"}, {"Arcilla con piedras por dentro.", "Clay with stones inside."},
     AB_FORTRESS, {"Fortaleza y arraigo", "Fortress and roots"}, {"−1 a todo el daño que recibe. Arraigo: +1 ATK por cada activación seguida sin moverse (máx. +2).", "−1 to all damage it takes. Roots: +1 ATK for each activation in a row without moving (max +2)."},
     SP_STONEWALL, 1, {"Escudo de piedra", "Stone shield"}, {"Hasta su próxima activación, los aliados a su lado reciben −1 de daño.", "Until its next activation, allies next to it take −1 damage."}},
    {{"Wyrmling", "Wyrmling"}, 6, 4, 3, 2, 2, 1, {"Mágico", "Magic"}, {"Dragoncito que escupe esmalte hirviendo.", "A little dragon that spits boiling glaze."},
     AB_BLAZE, {"Llama creciente", "Growing flame"}, {"Cada Aliento en activaciones seguidas da +1 al acertar y +1 de alcance (máx. +2).", "Each Breath in back-to-back activations gives +1 to hit and +1 range (max +2)."},
     SP_BREATH, 2, {"Aliento mágico", "Magic breath"}, {"Ataque en línea recta hasta 3 casillas. Ignora la cobertura y alcanza a quien esté oculto.", "Straight-line attack up to 3 squares. Ignores cover and reaches hidden foes."}},
    {{"Cazador", "Hunter"}, 5, 3, 2, 1, 2, 4, {"A distancia", "Ranged"}, {"Arco de mimbre y flechas de arcilla seca.", "A wicker bow and dried-clay arrows."},
     AB_MARKSMAN, {"Tirador", "Marksman"}, {"Dispara en línea recta, también en diagonal, de 2 a 4 casillas. No puede disparar a nadie que tenga pegado.", "Shoots in a straight line, diagonals too, 2 to 4 squares away. Can't shoot anyone touching it."},
     SP_LEAP, 1, {"Salto atrás", "Leap back"}, {"Salta 2 casillas en línea recta, aunque esté trabado.", "Leaps 2 squares in a straight line, even when engaged."}},
};
const char *cname(int cid) { return cid >= 0 && cid < C_COUNT ? L(CREATURES[cid].name) : ""; }
bool special_is_attack(int cid) { special_t sp = CREATURES[cid].special; return sp == SP_RUSH || sp == SP_SHOVE || sp == SP_CRUSH || sp == SP_CHAIN || sp == SP_BREATH; }
bool special_is_instant(int cid) { special_t sp = CREATURES[cid].special; return sp == SP_SHADOW || sp == SP_STONEWALL; }

/* Impulso = movimiento, Ímpetu = acción */
const power_def_t POWERS[P_COUNT] = {
    {{"Endurecer", "Harden"}, {"Una criatura aliada recupera 2 PV y gana +1 DEF hasta el fin de la ronda.", "An allied creature heals 2 HP and gets +1 DEF until end of round."}},
    {{"Impulso", "Dash"}, {"La próxima criatura que actives gana +2 MOV en esa activación.", "The next creature you activate gets +2 MOV for that activation."}},
    {{"Ímpetu", "Momentum"}, {"La próxima criatura que actives tiene 3 PA en vez de 2.", "The next creature you activate has 3 AP instead of 2."}},
    {{"Reinvocar", "Resummon"}, {"Una criatura caída vuelve a tu zona con la mitad de sus PV. Cuesta la mitad de su coste + 2 y actúa desde la ronda siguiente.", "A fallen creature returns to your zone with half its HP. Costs half its cost + 2 and acts from next round."}},
};
int resummon_cost(int cid) { return (CREATURES[cid].cost + 1) / 2 + RULES.power_cost[P_RESUMMON]; }
int power_cost_of(power_t p, int cid) { return p == P_RESUMMON ? resummon_cost(cid) : RULES.power_cost[p]; }

static const terrain_t TERRAIN[] = {
    {'.', {"Suelo", "Ground"}, true, 1, true, 0, 0, false, false, false, false},
    {'#', {"Muro", "Wall"}, false, 0, false, 0, 0, false, false, false, false},
    {'R', {"Roca", "Rock"}, false, 0, false, 0, 0, false, false, false, false},
    {'T', {"Árbol", "Tree"}, true, 2, false, 1, 0, false, false, false, true},
    {'u', {"Ruinas", "Ruins"}, true, 1, true, 1, 0, false, false, false, false},
    {'~', {"Agua", "Water"}, false, 0, true, 0, 0, false, false, false, false},
    {'L', {"Lava", "Lava"}, true, 1, true, 0, 1, false, false, false, false},
    {':', {"Terreno difícil", "Rough ground"}, true, 2, true, 0, 0, false, false, false, false},
    {'C', {"Punto de control", "Control point"}, true, 1, true, 0, 0, true, false, false, false},
    {'S', {"Santuario de maná", "Mana shrine"}, true, 1, true, 0, 0, false, true, false, false},
    {'I', {"Hielo", "Ice"}, true, 1, true, 0, 0, false, false, true, false},
};
const terrain_t *terrain_of(char ch) {
    for (size_t i = 0; i < sizeof(TERRAIN) / sizeof(TERRAIN[0]); i++) if (TERRAIN[i].ch == ch) return &TERRAIN[i];
    return &TERRAIN[0];
}

/* Todos los mapas tienen simetría de rotación de 180°: (x,y) es igual a (w−1−x, h−1−y).
   Azul despliega en las dos filas de abajo y Naranja en las dos de arriba. */
const map_t MAPS[MAP_COUNT] = {
    {"Ancient Ruins", 8, 8, {"Dos puntos de control en extremos opuestos, muros y ruinas.", "Two control points on opposite flanks, walls and ruins."},
     {"........", ".T.:.u..", "...#..L.", "C.S..~.R", "R.~..S.C", ".L..#...", "..u.:.T.", "........"}, {{6, 7}, {0, 1}}},
    {"Lava Pit", 8, 8, {"Un río de lava con dos puentes; los puntos de control están sobre ellos.", "A lava river with two bridges; the control points sit on them."},
     {"........", "..R..T..", ".u..S...", "LCLLLL.L", "L.LLLLCL", "...S..u.", "..T..R..", "........"}, {{6, 7}, {0, 1}}},
    {"Dark Forest", 8, 8, {"Bosque cerrado. Atacar desde un árbol da +1 (emboscada).", "Dense woods. Attacking from a tree gives +1 (ambush)."},
     {"........", ".T..T.T.", "T..T..C.", ".TS..T.T", "T.T..ST.", ".C..T..T", ".T.T..T.", "........"}, {{6, 7}, {0, 1}}},
    {"Frozen Crypt", 8, 8, {"Si terminas de moverte sobre hielo, resbalas hasta salir de él.", "If you end a move on ice, you slide until you leave it."},
     {"........", ".#.II.#.", "..IIII.C", "S.I##I..", "..I##I.S", "C.IIII..", ".#.II.#.", "........"}, {{6, 7}, {0, 1}}},
    {"Mana Temple", 8, 8, {"Cuatro puntos de control y cuatro santuarios repartidos.", "Four control points and four shrines spread out."},
     {"........", "..S..u..", "C...#..S", "..u..C..", "..C..u..", "S..#...C", "..u..S..", "........"}, {{6, 7}, {0, 1}}},
    {"Ancient Ruins XL", 10, 10, {"Versión de 10×10 para comparar ritmo e interacción.", "10×10 version to compare pacing and interaction."},
     {"..........", "..T..:..u.", ".....#....", "C..u...L..", "...S..~..R", "R..~..S...", "..L...u..C", "....#.....", ".u..:..T..", ".........."}, {{8, 9}, {0, 1}}},
    {"Mana Temple XL", 10, 10, {"Versión de 10×10 con cuatro puntos y cuatro santuarios.", "10×10 version with four points and four shrines."},
     {"..........", "..S....u..", "C....#...S", "...u...C..", "....S.....", ".....S....", "..C...u...", "S...#....C", "..u....S..", ".........."}, {{8, 9}, {0, 1}}},
};

/* ---------- Dados ---------- */
int d6(byte_source_t src, void *ctx) { uint8_t b; do { b = src(ctx); } while (b >= 252); return (b % 6) + 1; }
static uint8_t hw_src(void *ctx) { (void)ctx; return platform_random_byte(); }
int d6_hw(void) { return d6(hw_src, NULL); }

/* ---------- Ejércitos ---------- */
int army_cost(const army_t *a) { int s = 0; for (int i = 0; i < a->n; i++) s += CREATURES[a->cid[i]].cost; return s; }
add_result_t army_can_add(const army_t *a, int cid) {
    if (a->n >= RULES.max_units) return ADD_MAX;
    if (army_cost(a) + CREATURES[cid].cost > RULES.mana_budget) return ADD_MANA;
    if (cid == C_LARVA) { int k = 0; for (int i = 0; i < a->n; i++) k += a->cid[i] == C_LARVA; if (k >= RULES.larva_max) return ADD_LIMIT; }
    return ADD_OK;
}
add_result_t army_add(army_t *a, int cid) { add_result_t r = army_can_add(a, cid); if (r == ADD_OK) a->cid[a->n++] = (uint8_t)cid; return r; }
const char *add_reason(add_result_t r) {
    if (r == ADD_MAX) return TR("Máximo 6 criaturas", "6 creatures max");
    if (r == ADD_MANA) return TR("No alcanza el maná", "Not enough mana");
    if (r == ADD_LIMIT) return TR("Máximo 3 Larvas", "3 Larvas max");
    return "";
}

/* ---------- Utilidades ---------- */
static const int DIRS[4][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
static int clampi(int v, int a, int b) { return v < a ? a : v > b ? b : v; }
static bool inb(const game_t *s, int x, int y) { return x >= 0 && y >= 0 && x < s->w && y < s->h; }
static const terrain_t *terr(const game_t *s, int x, int y) { return terrain_of(s->tiles[y][x]); }
static const creature_t *cdef(const unit_t *u) { return &CREATURES[u->cid]; }
static int dist(int ax, int ay, int bx, int by) { return abs(ax - bx) + abs(ay - by); }

const unit_t *unit_at_c(const game_t *s, int x, int y) {
    for (int i = 0; i < s->nunits; i++) { const unit_t *u = &s->units[i]; if (u->alive && u->x == x && u->y == y) return u; }
    return NULL;
}
const unit_t *unit_by_id_c(const game_t *s, int id) { for (int i = 0; i < s->nunits; i++) if (s->units[i].id == id) return &s->units[i]; return NULL; }
unit_t *unit_by_id(game_t *s, int id) { return (unit_t *)unit_by_id_c(s, id); }
unit_t *active_unit(game_t *s) { return s->active_id < 0 ? NULL : unit_by_id(s, s->active_id); }

static int allies_adj(const game_t *s, const unit_t *u) {
    int n = 0;
    for (int d = 0; d < 4; d++) { const unit_t *o = unit_at_c(s, u->x + DIRS[d][0], u->y + DIRS[d][1]); if (o && o->player == u->player && o->id != u->id) n++; }
    return n;
}
static int allied_larvas_near(const game_t *s, const unit_t *u) { /* diagonales solo si RULES.swarm_diag */
    int n = 0;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (!dx && !dy) continue;
        if (dx && dy && !RULES.swarm_diag) continue;
        const unit_t *o = unit_at_c(s, u->x + dx, u->y + dy);
        if (o && o->player == u->player && o->cid == C_LARVA) n++;
    }
    return n;
}
/* Un Stalker oculto no traba a nadie ni disputa puntos */
static bool enemy_adj_at(const game_t *s, int x, int y, int player) {
    for (int d = 0; d < 4; d++) { const unit_t *o = unit_at_c(s, x + DIRS[d][0], y + DIRS[d][1]); if (o && o->player != player && !o->hidden) return true; }
    return false;
}
static bool enemy_near(const game_t *s, int x, int y, int player) {
    const unit_t *o = unit_at_c(s, x, y);
    return (o && o->player != player && !o->hidden) || enemy_adj_at(s, x, y, player);
}
static bool shielded_by_golem(const game_t *s, const unit_t *u) {
    if (cdef(u)->special == SP_STONEWALL) return false;
    for (int d = 0; d < 4; d++) { const unit_t *o = unit_at_c(s, u->x + DIRS[d][0], u->y + DIRS[d][1]); if (o && o->player == u->player && o->walling) return true; }
    return false;
}
bool is_engaged(const game_t *s, const unit_t *u) { return enemy_adj_at(s, u->x, u->y, u->player); }
/* Arraigo: el Golem que no se mueve suma su racha al ATK */
int eff_atk(const unit_t *u) { return cdef(u)->atk + u->atk_buff + (u->cid == C_GOLEM && !u->moved ? u->streak : 0); }
int breath_range(const unit_t *u) { return 3 + (u->cid == C_WYRMLING ? u->streak : 0); }
int eff_def(const game_t *s, const unit_t *u) {
    int d = cdef(u)->def + u->def_buff;
    if (cdef(u)->ability == AB_FORMATION && allies_adj(s, u) > 0) d += 1;
    return d;
}
int eff_mov(const unit_t *u) { return cdef(u)->mov + u->mov_bonus; }

static event_t *emit(game_t *s, ev_type_t type) {
    event_t *e = &s->events[(s->seq + 1) % EVENT_RING];
    memset(e, 0, sizeof(*e));
    e->seq = ++s->seq; e->round = (uint8_t)s->round; e->type = (uint8_t)type;
    e->player = -1; e->unit_id = -1; e->attacker_id = -1; e->by = -1; e->cid = -1; e->tcid = -1; e->winner = -2;
    return e;
}
const event_t *game_event(const game_t *s, uint16_t seq) {
    if (seq == 0 || seq > s->seq || s->seq - seq >= EVENT_RING) return NULL;
    const event_t *e = &s->events[seq % EVENT_RING];
    return e->seq == seq ? e : NULL;
}

/* ---------- Partida ---------- */
static void start_round(game_t *s);

void game_new(game_t *s, int map, const army_t *a0, const army_t *a1) {
    memset(s, 0, sizeof(*s));
    const map_t *m = &MAPS[map];
    s->map = map; s->w = m->w; s->h = m->h;
    for (int y = 0; y < s->h; y++) { memcpy(s->tiles[y], m->rows[y], (size_t)s->w); s->tiles[y][s->w] = 0; }
    memset(s->control_owner, -1, sizeof(s->control_owner));
    s->next_id = 1; s->round = 1; s->phase = PH_DEPLOY; s->active_id = -1; s->winner = -2;
    s->power_ready[0] = s->power_ready[1] = 1;
    s->last_power[0] = s->last_power[1] = -1;
    int i0 = 0, i1 = 0;
    while (i0 < a0->n || i1 < a1->n) {
        if (i0 < a0->n) { s->deploy_q[s->dq_len].player = 0; s->deploy_q[s->dq_len++].cid = a0->cid[i0++]; }
        if (i1 < a1->n) { s->deploy_q[s->dq_len].player = 1; s->deploy_q[s->dq_len++].cid = a1->cid[i1++]; }
    }
    s->current = s->dq_len ? s->deploy_q[0].player : 0;
    emit(s, EV_START);
}
static int zone_cells(const game_t *s, int p, target_t *out) {
    int n = 0;
    for (int r = 0; r < 2; r++) {
        int y = MAPS[s->map].deploy[p][r];
        for (int x = 0; x < s->w; x++) {
            const terrain_t *t = terr(s, x, y);
            if (t->pass && !t->hazard && !t->ice && !unit_at_c(s, x, y)) { out[n].x = (int8_t)x; out[n].y = (int8_t)y; out[n].unit_id = -1; n++; }
        }
    }
    return n;
}
int deploy_cells(const game_t *s, target_t *out) {
    if (s->phase != PH_DEPLOY || s->dq_head >= s->dq_len) return 0;
    return zone_cells(s, s->deploy_q[s->dq_head].player, out);
}
static unit_t *new_unit(game_t *s, int cid, int player, int x, int y) {
    unit_t *u = &s->units[s->nunits++];
    memset(u, 0, sizeof(*u));
    u->id = (int8_t)s->next_id++; u->cid = (uint8_t)cid; u->player = (uint8_t)player; u->x = (int8_t)x; u->y = (int8_t)y;
    u->hp = u->max_hp = (int8_t)CREATURES[cid].hp; u->alive = true;
    return u;
}
bool place_next(game_t *s, int x, int y) {
    target_t cells[MAX_TARGETS]; int n = deploy_cells(s, cells); bool ok = false;
    for (int i = 0; i < n; i++) if (cells[i].x == x && cells[i].y == y) ok = true;
    if (!ok) return false;
    int player = s->deploy_q[s->dq_head].player, cid = s->deploy_q[s->dq_head].cid; s->dq_head++;
    unit_t *u = new_unit(s, cid, player, x, y);
    event_t *e = emit(s, EV_DEPLOY); e->unit_id = u->id; e->cid = (int8_t)cid; e->player = (int8_t)player;
    if (s->dq_head >= s->dq_len) { s->phase = PH_BATTLE; start_round(s); }
    else s->current = s->deploy_q[s->dq_head].player;
    return true;
}
void auto_place_rest(game_t *s) {
    while (s->phase == PH_DEPLOY) {
        int p = s->deploy_q[s->dq_head].player;
        target_t cells[MAX_TARGETS]; int n = deploy_cells(s, cells);
        if (!n) { s->dq_head++; if (s->dq_head >= s->dq_len) { s->phase = PH_BATTLE; start_round(s); } continue; }
        int best = 0; double bk = 1e9, mid = (s->w - 1) / 2.0;
        for (int i = 0; i < n; i++) {
            double k = (p == 0 ? cells[i].y : -cells[i].y) * 10 + (cells[i].x > mid ? cells[i].x - mid : mid - cells[i].x);
            if (k < bk) { bk = k; best = i; }
        }
        place_next(s, cells[best].x, cells[best].y);
    }
}

/* ---------- Rondas ---------- */
static int available(const game_t *s, int p, int *ids) {
    int n = 0;
    for (int i = 0; i < s->nunits; i++) { const unit_t *u = &s->units[i]; if (u->alive && u->player == p && !u->activated) { if (ids) ids[n] = u->id; n++; } }
    return n;
}
static void start_round(game_t *s) {
    for (int i = 0; i < s->nunits; i++) if (s->units[i].alive) s->units[i].activated = false;
    emit(s, EV_ROUND);
    s->mana[0] += RULES.income_per_round; s->mana[1] += RULES.income_per_round;
    event_t *e = emit(s, EV_INCOME); e->amount = (int8_t)RULES.income_per_round;
    s->current = (s->round - 1) % 2;
    /* Variante: empieza la ronda quien va perdiendo en puntos (empate: se alterna) */
    if (RULES.initiative == 1 && s->vp[0] != s->vp[1]) s->current = s->vp[0] < s->vp[1] ? 0 : 1;
    if (!available(s, s->current, NULL)) s->current = 1 - s->current;
}
int selectable_units(const game_t *s, int *ids) {
    if (s->phase != PH_BATTLE || s->active_id >= 0) return 0;
    return available(s, s->current, ids);
}
bool activate(game_t *s, int id) {
    if (s->phase != PH_BATTLE || s->active_id >= 0) return false;
    unit_t *u = unit_by_id(s, id);
    if (!u || !u->alive || u->player != s->current || u->activated) return false;
    int p = u->player;
    u->ap = (int8_t)(RULES.ap_per_activation + s->boost_ap[p]); u->ap_start = u->ap;
    u->mov_bonus = (int8_t)s->boost_mov[p]; u->boost_ap = s->boost_ap[p]; u->boost_mov = s->boost_mov[p];
    s->boost_ap[p] = s->boost_mov[p] = 0;
    u->moved = false; u->was_defending = u->defending; u->defending = false; u->walling = false; u->used_sp = false;
    s->active_id = id;
    event_t *e = emit(s, EV_ACTIVATE); e->unit_id = u->id; e->cid = (int8_t)u->cid; e->player = (int8_t)p; e->boost_ap = (int8_t)u->boost_ap; e->boost_mov = (int8_t)u->boost_mov;
    return true;
}
bool cancel_activation(game_t *s) {
    unit_t *u = active_unit(s);
    if (!u || u->ap != u->ap_start) return false;
    s->boost_ap[u->player] = u->boost_ap; s->boost_mov[u->player] = u->boost_mov;
    u->ap = 0; u->mov_bonus = 0; u->defending = u->was_defending; s->active_id = -1;
    return true;
}
static void end_round(game_t *s);
void end_activation(game_t *s) {
    unit_t *u = active_unit(s);
    if (u) {
        u->activated = true; u->ap = 0; u->mov_bonus = 0;
        /* rachas: repetir la misma estrategia las hace crecer; cambiarla las reinicia */
        bool keep = u->cid == C_GOLEM ? !u->moved : (u->cid == C_COLOSSUS || u->cid == C_WYRMLING) ? u->used_sp : false;
        u->streak = keep ? (int8_t)(u->streak < RULES.streak_max ? u->streak + 1 : RULES.streak_max) : 0;
    }
    s->active_id = -1;
    if (s->phase != PH_BATTLE) return;
    if (u) { event_t *e = emit(s, EV_ENDACT); e->unit_id = u->id; e->cid = (int8_t)u->cid; e->player = (int8_t)u->player; }
    int other = 1 - s->current;
    if (available(s, other, NULL)) s->current = other;
    else if (!available(s, s->current, NULL)) end_round(s);
}
static void finish_if_spent(game_t *s) { unit_t *u = active_unit(s); if (u && (u->ap <= 0 || !u->alive)) end_activation(s); }

static int surviving_cost(const game_t *s, int p) {
    int c = 0; for (int i = 0; i < s->nunits; i++) if (s->units[i].alive && s->units[i].player == p) c += CREATURES[s->units[i].cid].cost;
    return c;
}
static void decide_by_points(game_t *s) {
    s->phase = PH_OVER;
    if (s->vp[0] != s->vp[1]) { s->winner = s->vp[0] > s->vp[1] ? 0 : 1; s->win_reason = WIN_POINTS; }
    else {
        int c0 = surviving_cost(s, 0), c1 = surviving_cost(s, 1);
        if (c0 != c1) { s->winner = c0 > c1 ? 0 : 1; s->win_reason = WIN_COST; }
        else { s->winner = -1; s->win_reason = WIN_DRAW; }
    }
    event_t *e = emit(s, EV_OVER); e->winner = (int8_t)s->winner;
}
static void end_round(game_t *s) {
    for (int y = 0; y < s->h; y++) for (int x = 0; x < s->w; x++) {
        const terrain_t *t = terr(s, x, y); const unit_t *u = unit_at_c(s, x, y);
        if (t->shrine && u) { s->mana[u->player] += 1; event_t *e = emit(s, EV_MANA); e->shrine = true; e->player = (int8_t)u->player; e->x = (int8_t)x; e->y = (int8_t)y; e->amount = 1; }
        if (t->control && u && !u->hidden && s->control_owner[y][x] != u->player && !enemy_adj_at(s, x, y, u->player)) {
            s->control_owner[y][x] = (int8_t)u->player;
            event_t *e = emit(s, EV_CAPTURE); e->cid = (int8_t)u->cid; e->player = (int8_t)u->player; e->x = (int8_t)x; e->y = (int8_t)y;
        }
    }
    for (int y = 0; y < s->h; y++) for (int x = 0; x < s->w; x++) {
        int o = s->control_owner[y][x]; if (o < 0) continue;
        if (enemy_near(s, x, y, o)) { event_t *e = emit(s, EV_CONTESTED); e->player = (int8_t)o; e->x = (int8_t)x; e->y = (int8_t)y; }
        else s->vp[o] += 1;
    }
    for (int i = 0; i < s->nunits; i++) { s->units[i].atk_buff = 0; s->units[i].def_buff = 0; }
    event_t *e = emit(s, EV_ROUNDEND); e->vp0 = (int8_t)s->vp[0]; e->vp1 = (int8_t)s->vp[1];
    if (s->round >= RULES.max_rounds) { decide_by_points(s); return; }
    s->round += 1;
    start_round(s);
}
static void check_elimination(game_t *s) {
    bool alive[2] = {false, false};
    for (int i = 0; i < s->nunits; i++) if (s->units[i].alive) alive[s->units[i].player] = true;
    if (alive[0] && alive[1]) return;
    s->phase = PH_OVER; s->active_id = -1;
    s->winner = alive[0] ? 0 : alive[1] ? 1 : -1; s->win_reason = s->winner < 0 ? WIN_DRAW : WIN_ELIMINATION;
    event_t *e = emit(s, EV_OVER); e->winner = (int8_t)s->winner;
}

/* ---------- Movimiento ---------- */
/* Dijkstra con zona de control; guarda de dónde se llegó a cada casilla (para el hielo). */
static int reach_impl(const game_t *s, const unit_t *u, target_t *out, int8_t par[MAX_H][MAX_W]) {
    if (is_engaged(s, u)) return 0;
    int budget = eff_mov(u), best[MAX_H][MAX_W]; bool done[MAX_H][MAX_W];
    for (int y = 0; y < MAX_H; y++) for (int x = 0; x < MAX_W; x++) { best[y][x] = 999; done[y][x] = false; if (par) par[y][x] = -1; }
    best[u->y][u->x] = 0;
    int n = 0;
    for (;;) {
        int cx = -1, cy = -1, cc = 999;
        for (int y = 0; y < s->h; y++) for (int x = 0; x < s->w; x++) if (!done[y][x] && best[y][x] < cc) { cc = best[y][x]; cx = x; cy = y; }
        if (cx < 0) break;
        done[cy][cx] = true;
        bool is_start = cx == u->x && cy == u->y;
        if (!is_start && out) { out[n].x = (int8_t)cx; out[n].y = (int8_t)cy; out[n].unit_id = -1; out[n].cost = (int8_t)cc; }
        if (!is_start) n++;
        if (!is_start && enemy_adj_at(s, cx, cy, u->player)) continue;
        for (int d = 0; d < 4; d++) {
            int nx = cx + DIRS[d][0], ny = cy + DIRS[d][1];
            if (!inb(s, nx, ny)) continue;
            const terrain_t *t = terr(s, nx, ny);
            if (!t->pass || unit_at_c(s, nx, ny)) continue;
            int nc = cc + t->cost;
            if (nc > budget || nc >= best[ny][nx]) continue;
            best[ny][nx] = nc; if (par) par[ny][nx] = (int8_t)d;
        }
    }
    return n;
}
static int reachable(const game_t *s, const unit_t *u, target_t *out) { return reach_impl(s, u, out, NULL); }
static int retreat_cells(const game_t *s, const unit_t *u, target_t *out) {
    int n = 0;
    for (int d = 0; d < 4; d++) {
        int x = u->x + DIRS[d][0], y = u->y + DIRS[d][1];
        if (inb(s, x, y) && terr(s, x, y)->pass && !unit_at_c(s, x, y)) { out[n].x = (int8_t)x; out[n].y = (int8_t)y; out[n].unit_id = -1; n++; }
    }
    return n;
}
/* Hielo: si el movimiento termina sobre hielo, sigue en la misma dirección hasta salir o chocar. */
int slide_end(const game_t *s, const unit_t *u, int x, int y, int *ex, int *ey) {
    *ex = x; *ey = y;
    if (!terr(s, x, y)->ice) return 0;
    int d;
    if (dist(u->x, u->y, x, y) == 1) {
        for (d = 0; d < 4; d++) if (u->x + DIRS[d][0] == x && u->y + DIRS[d][1] == y) break;
    } else {
        int8_t par[MAX_H][MAX_W]; reach_impl(s, u, NULL, par); d = par[y][x];
    }
    if (d < 0 || d > 3) return 0;
    int n = 0;
    while (n < 12 && terr(s, *ex, *ey)->ice) {
        int nx = *ex + DIRS[d][0], ny = *ey + DIRS[d][1];
        if (!inb(s, nx, ny) || !terr(s, nx, ny)->pass) break;
        const unit_t *o = unit_at_c(s, nx, ny); if (o && o != u) break;
        *ex = nx; *ey = ny; n++;
    }
    return n;
}
static int line_targets(const game_t *s, const unit_t *u, int range, target_t *out) {
    int n = 0;
    for (int d = 0; d < 4; d++) for (int i = 1; i <= range; i++) {
        int x = u->x + DIRS[d][0] * i, y = u->y + DIRS[d][1] * i;
        if (!inb(s, x, y)) break;
        const unit_t *o = unit_at_c(s, x, y);
        if (o) { if (o->player != u->player) { out[n].x = (int8_t)x; out[n].y = (int8_t)y; out[n].unit_id = o->id; n++; } break; }
        if (!terr(s, x, y)->los) break;
    }
    return n;
}

static const int DIRS8[8][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}, {1, 1}, {-1, 1}, {-1, -1}, {1, -1}};
/* Cazador: líneas rectas en 8 direcciones, de min a max casillas; la primera criatura corta la línea */
static int marksman_targets(const game_t *s, const unit_t *u, int lo, int hi, target_t *out) {
    int n = 0;
    for (int d = 0; d < 8; d++) for (int i = 1; i <= hi; i++) {
        int x = u->x + DIRS8[d][0] * i, y = u->y + DIRS8[d][1] * i;
        if (!inb(s, x, y)) break;
        const unit_t *o = unit_at_c(s, x, y);
        if (o) { if (o->player != u->player && !o->hidden && i >= lo) { out[n].x = (int8_t)x; out[n].y = (int8_t)y; out[n].unit_id = o->id; n++; } break; }
        if (!terr(s, x, y)->los) break;
    }
    return n;
}
bool rush_landing(const game_t *s, const unit_t *u, const unit_t *tgt, int *lx, int *ly) {
    int dx = (tgt->x > u->x) - (tgt->x < u->x), dy = (tgt->y > u->y) - (tgt->y < u->y);
    if ((dx && dy) || (!dx && !dy)) return false;
    int d = dist(u->x, u->y, tgt->x, tgt->y);
    if (d < 2 || d > RULES.rush_range) return false;
    for (int i = 1; i < d; i++) {
        int x = u->x + dx * i, y = u->y + dy * i; const terrain_t *t = terr(s, x, y);
        if (!t->pass || t->hazard || unit_at_c(s, x, y)) return false;
    }
    *lx = tgt->x - dx; *ly = tgt->y - dy; return true;
}

/* ---------- Acciones ---------- */
static int push_unit(target_t *out, int n, const unit_t *o) { out[n].x = o->x; out[n].y = o->y; out[n].unit_id = o->id; return n + 1; }
int targets_for(game_t *s, action_id_t a, target_t *out) {
    unit_t *u = active_unit(s); if (!u) return 0;
    const creature_t *c = cdef(u); int n = 0;
    switch (a) {
    case A_MOVE: return reachable(s, u, out);
    case A_RETREAT: return is_engaged(s, u) ? retreat_cells(s, u, out) : 0;
    case A_ATTACK:
        if (c->ability == AB_MARKSMAN) return marksman_targets(s, u, 2, c->rng, out);
        for (int i = 0; i < s->nunits; i++) { const unit_t *o = &s->units[i]; if (o->alive && !o->hidden && o->player != u->player && dist(o->x, o->y, u->x, u->y) <= c->rng) n = push_unit(out, n, o); }
        return n;
    case A_ABILITY:
        switch (c->special) {
        case SP_CRUSH: case SP_SHOVE:
            for (int i = 0; i < s->nunits; i++) { const unit_t *o = &s->units[i]; if (o->alive && !o->hidden && o->player != u->player && dist(o->x, o->y, u->x, u->y) == 1) n = push_unit(out, n, o); }
            return n;
        case SP_BREATH: return line_targets(s, u, breath_range(u), out); /* el aliento alcanza también a los ocultos */
        case SP_CHAIN:
            for (int i = 0; i < s->nunits; i++) { const unit_t *o = &s->units[i]; if (o->alive && !o->hidden && o->player != u->player && dist(o->x, o->y, u->x, u->y) <= RULES.chain_range) n = push_unit(out, n, o); }
            return n;
        case SP_RUSH: {
            if (is_engaged(s, u)) return 0;
            int lx, ly;
            for (int i = 0; i < s->nunits; i++) { const unit_t *o = &s->units[i]; if (o->alive && !o->hidden && o->player != u->player && rush_landing(s, u, o, &lx, &ly)) n = push_unit(out, n, o); }
            return n; }
        case SP_BURROW:
            for (int y = 0; y < s->h; y++) for (int x = 0; x < s->w; x++) {
                int d = dist(x, y, u->x, u->y); const terrain_t *t = terr(s, x, y);
                if (d < 1 || d > RULES.burrow_range || !t->pass || t->hazard || unit_at_c(s, x, y)) continue;
                out[n].x = (int8_t)x; out[n].y = (int8_t)y; out[n].unit_id = -1; n++;
            }
            return n;
        case SP_LEAP:
            for (int d = 0; d < 8; d++) {
                int x = u->x + DIRS8[d][0] * 2, y = u->y + DIRS8[d][1] * 2;
                if (!inb(s, x, y)) continue;
                const terrain_t *t = terr(s, x, y);
                if (!t->pass || t->hazard || unit_at_c(s, x, y)) continue;
                out[n].x = (int8_t)x; out[n].y = (int8_t)y; out[n].unit_id = -1; n++;
            }
            return n;
        default: return 0;
        }
    default: return 0;
    }
}
int actions_for(game_t *s, action_t *out) {
    unit_t *u = active_unit(s); if (!u) return 0;
    const creature_t *c = cdef(u); bool eng = is_engaged(s, u); int n = 0;
    target_t tmp[MAX_TARGETS];
    const char *no_ap = TR("Sin PA", "No AP");
#define ADD(ID, NAME, AP, OK, REASON) do { out[n].id = (ID); out[n].name = (NAME); out[n].ap = (uint8_t)(AP); out[n].enabled = (OK); out[n].reason = (OK) ? "" : (REASON); n++; } while (0)
    bool m_ok = u->ap >= 1 && !eng && reachable(s, u, tmp) > 0;
    ADD(A_MOVE, TR("Mover", "Move"), 1, m_ok, u->ap < 1 ? no_ap : eng ? TR("Trabado: usa Retirada", "Engaged: use Retreat") : TR("No hay casillas libres", "No free squares"));
    bool a_ok = u->ap >= 1 && targets_for(s, A_ATTACK, tmp) > 0;
    const char *no_tgt = c->ability == AB_MARKSMAN ? TR("Nadie a tiro (2 a 4 en línea)", "Nobody in range (2 to 4 in line)") : TR("Nadie a su lado", "Nobody next to it");
    ADD(A_ATTACK, c->ability == AB_MARKSMAN ? TR("Disparar", "Shoot") : TR("Atacar", "Attack"), 1, a_ok, u->ap < 1 ? no_ap : no_tgt);
    {
        static char need[32];
        snprintf(need, sizeof need, TR("Necesita %d PA", "Needs %d AP"), c->sp_ap);
        bool cond; const char *why;
        if (c->special == SP_SHADOW) { cond = !u->hidden; why = TR("Ya está oculto", "Already hidden"); }
        else if (c->special == SP_STONEWALL) { cond = !u->walling; why = TR("El escudo ya está puesto", "The shield is already up"); }
        else { cond = targets_for(s, A_ABILITY, tmp) > 0; why = c->special == SP_RUSH && eng ? TR("Trabado: no puede embestir", "Engaged: can't charge") : TR("Sin objetivos válidos", "No valid targets"); }
        ADD(A_ABILITY, L(c->sp_name), c->sp_ap, u->ap >= c->sp_ap && cond, u->ap < c->sp_ap ? need : why);
    }
    ADD(A_DEFEND, TR("Guardia", "Guard"), 1, u->ap >= 1, no_ap);
    bool r_ok = u->ap >= 1 && eng && retreat_cells(s, u, tmp) > 0;
    ADD(A_RETREAT, TR("Retirada", "Retreat"), 1, r_ok, u->ap < 1 ? no_ap : !eng ? TR("Solo si un enemigo está a su lado", "Only when an enemy is next to it") : TR("Sin salida", "No way out"));
    ADD(A_INSPECT, TR("Ver ficha", "Inspect"), 0, true, "");
    ADD(A_END, TR("Terminar", "End"), 0, true, "");
#undef ADD
    return n;
}
static bool spend(unit_t *u, int n) { if (u->ap < n) return false; u->ap = (int8_t)(u->ap - n); return true; }
static void collect_orb(game_t *s, unit_t *u) {
    uint8_t v = s->orbs[u->y][u->x];
    if (v) {
        s->orbs[u->y][u->x] = 0; s->mana[u->player] += v;
        event_t *e = emit(s, EV_MANA); e->cid = (int8_t)u->cid; e->player = (int8_t)u->player; e->x = u->x; e->y = u->y; e->amount = (int8_t)v;
    }
}
static void reveal(game_t *s, unit_t *u) {
    if (!u->hidden) return;
    u->hidden = false;
    event_t *e = emit(s, EV_REVEAL); e->unit_id = u->id; e->cid = (int8_t)u->cid; e->player = (int8_t)u->player; e->x = u->x; e->y = u->y;
}
static void apply_damage(game_t *s, unit_t *u, int amount, bool lava, const unit_t *att) {
    reveal(s, u);
    u->hp = (int8_t)(u->hp - amount);
    event_t *e = emit(s, EV_DAMAGE);
    e->unit_id = u->id; e->cid = (int8_t)u->cid; e->player = (int8_t)u->player; e->amount = (int8_t)amount; e->lava = lava;
    e->attacker_id = att ? att->id : -1; e->by = att ? (int8_t)att->player : -1; e->x = u->x; e->y = u->y;
    if (u->hp <= 0) {
        u->hp = 0; u->alive = false;
        int drop = (CREATURES[u->cid].cost + 1) / 2;
        s->orbs[u->y][u->x] = (uint8_t)(s->orbs[u->y][u->x] + drop);
        event_t *d = emit(s, EV_DEATH);
        d->unit_id = u->id; d->player = (int8_t)u->player; d->by = att ? (int8_t)att->player : -1; d->x = u->x; d->y = u->y; d->cid = (int8_t)u->cid; d->drop = (int8_t)drop;
        check_elimination(s);
    }
}
bool do_move(game_t *s, int x, int y, bool retreat) {
    unit_t *u = active_unit(s); if (!u) return false;
    target_t t[MAX_TARGETS]; int n = targets_for(s, retreat ? A_RETREAT : A_MOVE, t); bool ok = false;
    for (int i = 0; i < n; i++) if (t[i].x == x && t[i].y == y) ok = true;
    if (!ok || u->ap < 1) return false;
    int ex, ey; int slid = slide_end(s, u, x, y, &ex, &ey);
    spend(u, 1);
    int fx = u->x, fy = u->y;
    u->x = (int8_t)ex; u->y = (int8_t)ey;
    if (!retreat) u->moved = true;
    event_t *e = emit(s, EV_MOVE); e->unit_id = u->id; e->cid = (int8_t)u->cid; e->player = (int8_t)u->player;
    e->fx = (int8_t)fx; e->fy = (int8_t)fy; e->x = (int8_t)ex; e->y = (int8_t)ey; e->kind = retreat; e->slid = slid > 0;
    collect_orb(s, u);
    const terrain_t *tt = terr(s, ex, ey);
    if (tt->hazard) apply_damage(s, u, tt->hazard, true, NULL);
    finish_if_spent(s);
    return true;
}
bool do_special(game_t *s, int x, int y) {
    unit_t *u = active_unit(s); if (!u) return false;
    const creature_t *c = cdef(u);
    if (special_is_attack(u->cid) || u->ap < c->sp_ap) return false;
    if (c->special == SP_SHADOW) {
        if (u->hidden) return false;
        spend(u, c->sp_ap); u->hidden = true;
        event_t *e = emit(s, EV_HIDE); e->unit_id = u->id; e->cid = (int8_t)u->cid; e->player = (int8_t)u->player; e->x = u->x; e->y = u->y;
        finish_if_spent(s); return true;
    }
    if (c->special == SP_STONEWALL) {
        if (u->walling) return false;
        spend(u, c->sp_ap); u->walling = true;
        event_t *e = emit(s, EV_WALL); e->unit_id = u->id; e->cid = (int8_t)u->cid; e->player = (int8_t)u->player; e->x = u->x; e->y = u->y;
        finish_if_spent(s); return true;
    }
    /* Excavar o Salto atrás: casilla destino */
    target_t t[MAX_TARGETS]; int n = targets_for(s, A_ABILITY, t); bool ok = false;
    for (int i = 0; i < n; i++) if (t[i].x == x && t[i].y == y) ok = true;
    if (!ok) return false;
    spend(u, c->sp_ap);
    int fx = u->x, fy = u->y; u->x = (int8_t)x; u->y = (int8_t)y; u->moved = true;
    event_t *e = emit(s, EV_MOVE); e->unit_id = u->id; e->cid = (int8_t)u->cid; e->player = (int8_t)u->player;
    e->fx = (int8_t)fx; e->fy = (int8_t)fy; e->x = (int8_t)x; e->y = (int8_t)y; e->kind = c->special == SP_BURROW ? 2 : 3;
    collect_orb(s, u);
    finish_if_spent(s); return true;
}
bool do_defend(game_t *s) {
    unit_t *u = active_unit(s); if (!u || !spend(u, 1)) return false;
    u->defending = true;
    event_t *e = emit(s, EV_DEFEND); e->unit_id = u->id; e->cid = (int8_t)u->cid; e->player = (int8_t)u->player;
    finish_if_spent(s); return true;
}
/* Rayo en cadena: desde el último alcanzado salta al enemigo pegado (lado o diagonal) con menos PV que aún no recibió el rayo */
int chain_targets(const game_t *s, const unit_t *att, const unit_t *first, int8_t *ids) {
    int n = 0; const unit_t *last = first; int8_t hit[4] = {first->id, -1, -1, -1};
    for (int k = 0; k < RULES.chain_jumps && k < 3; k++) {
        const unit_t *best = NULL;
        for (int i = 0; i < s->nunits; i++) {
            const unit_t *o = &s->units[i];
            if (!o->alive || o->player == att->player) continue;
            if (abs(o->x - last->x) > 1 || abs(o->y - last->y) > 1) continue;
            bool done = false; for (int j = 0; j <= n; j++) if (hit[j] == o->id) done = true;
            if (done) continue;
            if (!best || o->hp < best->hp || (o->hp == best->hp && o->id < best->id)) best = o;
        }
        if (!best) break;
        ids[n] = best->id; hit[n + 1] = best->id; n++; last = best;
    }
    return n;
}
int chain_hop_damage(const game_t *s, int base, int hop, const unit_t *tgt) {
    int d = base - hop;
    if (cdef(tgt)->ability == AB_FORTRESS) d -= 1;
    if (shielded_by_golem(s, tgt)) d -= 1;
    if (tgt->defending) d -= 1;
    return d > 0 ? d : 0;
}

/* ---------- Poderes del invocador ---------- */
static int fallen(const game_t *s, int p, target_t *out) {
    int n = 0;
    for (int i = 0; i < s->nunits; i++) {
        const unit_t *u = &s->units[i];
        if (u->alive || u->player != p) continue;
        if (out) { out[n].unit_id = u->id; out[n].cost = (int8_t)resummon_cost(u->cid); out[n].affordable = s->mana[p] >= resummon_cost(u->cid); out[n].x = (int8_t)u->cid; out[n].y = 0; }
        n++;
    }
    return n;
}
int powers_for(game_t *s, power_state_t *out) {
    if (s->phase != PH_BATTLE || s->active_id >= 0) return 0;
    int p = s->current, mana = s->mana[p];
    bool cooling = s->round < s->power_ready[p];
    bool boosted = s->boost_ap[p] || s->boost_mov[p];
    static char need[P_COUNT][40], cool[44];
    if (RULES.power_cooldown == 0) snprintf(cool, sizeof cool, "%s", TR("Ya usaste un poder esta ronda", "You already used a power this round"));
    else snprintf(cool, sizeof cool, TR("Recargando: vuelve en la ronda %d", "Recharging: back in round %d"), s->power_ready[p]);
    target_t tmp[MAX_TARGETS];
    for (int i = 0; i < P_COUNT; i++) {
        int cost = RULES.power_cost[i]; bool ok = true; const char *reason = "";
        if (i == P_RESUMMON) {
            int nf = fallen(s, p, tmp); cost = -1;
            for (int k = 0; k < nf; k++) if (cost < 0 || tmp[k].cost < cost) cost = tmp[k].cost;
            if (!nf) { ok = false; reason = TR("No tienes criaturas caídas", "You have no fallen creatures"); }
            else if (!zone_cells(s, p, tmp)) { ok = false; reason = TR("Tu zona de despliegue está llena", "Your deployment zone is full"); }
        }
        if (RULES.power_no_repeat && i == s->last_power[p]) { ok = false; reason = TR("No puedes repetir el mismo poder seguido", "You can't use the same power twice in a row"); }
        if ((i == P_HASTE || i == P_SURGE) && boosted) { ok = false; reason = TR("Ya tienes un refuerzo preparado", "A boost is already waiting"); }
        if (ok && cost >= 0 && mana < cost) { ok = false; snprintf(need[i], 40, TR("Necesitas %d de maná", "Needs %d mana"), cost); reason = need[i]; }
        if (cooling) { ok = false; reason = cool; }
        out[i].id = (power_t)i; out[i].cost = cost; out[i].enabled = ok; out[i].reason = reason;
    }
    return P_COUNT;
}
int power_targets(game_t *s, power_t id, target_t *out) {
    int p = s->current, n = 0;
    if (id == P_HARDEN) { for (int i = 0; i < s->nunits; i++) { const unit_t *u = &s->units[i]; if (u->alive && u->player == p) n = push_unit(out, n, u); } return n; }
    if (id == P_RESUMMON) return fallen(s, p, out);
    return 0;
}
int resummon_cells(game_t *s, target_t *out) { return zone_cells(s, s->current, out); }
bool use_power(game_t *s, power_t id, int unit_id, int x, int y, const char **reason) {
    static const char *dummy; if (!reason) reason = &dummy;
    power_state_t ps[P_COUNT];
    if (!powers_for(s, ps)) { *reason = TR("Ahora no puedes usar poderes", "You cannot use powers now"); return false; }
    if (!ps[id].enabled) { *reason = ps[id].reason; return false; }
    int p = s->current, cost = ps[id].cost;
    int8_t uid = -1, ucid = -1, ux = 0, uy = 0, healed = 0;
    if (id == P_HARDEN) {
        unit_t *u = unit_by_id(s, unit_id);
        if (!u || !u->alive || u->player != p) { *reason = TR("Objetivo inválido", "Invalid target"); return false; }
        int h = u->max_hp - u->hp; if (h > RULES.harden_heal) h = RULES.harden_heal;
        u->hp = (int8_t)(u->hp + h); u->def_buff++;
        uid = u->id; ucid = (int8_t)u->cid; healed = (int8_t)h; ux = u->x; uy = u->y;
    } else if (id == P_HASTE) {
        s->boost_mov[p] = (uint8_t)(s->boost_mov[p] + RULES.haste_mov);
    } else if (id == P_SURGE) {
        s->boost_ap[p] = (uint8_t)(s->boost_ap[p] + RULES.surge_ap);
    } else {
        unit_t *u = unit_by_id(s, unit_id);
        if (!u || u->alive || u->player != p) { *reason = TR("Esa criatura no ha caído", "That creature has not fallen"); return false; }
        cost = resummon_cost(u->cid);
        if (s->mana[p] < cost) { *reason = TR("Falta maná", "Not enough mana"); return false; }
        target_t cells[MAX_TARGETS]; int n = zone_cells(s, p, cells); bool ok = false;
        for (int i = 0; i < n; i++) if (cells[i].x == x && cells[i].y == y) ok = true;
        if (!ok) { *reason = TR("Casilla fuera de tu zona", "Square outside your zone"); return false; }
        u->alive = true; u->x = (int8_t)x; u->y = (int8_t)y; u->hp = (int8_t)((u->max_hp + 1) / 2); u->activated = true; u->moved = false;
        u->defending = false; u->atk_buff = u->def_buff = u->mov_bonus = 0; u->ap = 0;
        uid = u->id; ucid = (int8_t)u->cid; ux = u->x; uy = u->y;
    }
    s->mana[p] -= cost;
    s->power_ready[p] = s->round + 1 + RULES.power_cooldown;
    s->last_power[p] = (int8_t)id;
    event_t *e = emit(s, EV_POWER);
    e->power = (int8_t)id; e->player = (int8_t)p; e->amount = (int8_t)cost;
    e->unit_id = uid; e->cid = ucid; e->healed = healed; e->x = ux; e->y = uy;
    return true;
}

/* ---------- Combate ---------- */
quality_t skill_quality(int natural, int total) {
    if (natural == 1) return Q_CRITFAIL;
    if (natural == 6) return Q_CRIT;
    if (total <= 2) return Q_FAIL;
    if (total == 3) return Q_PARTIAL;
    if (total == 4) return Q_SUCCESS;
    return Q_IMPROVED;
}
static void part(part_t *p, const char *what, int val) { snprintf(p->what, sizeof p->what, "%s", what); p->why[0] = 0; p->val = val; p->is_set = false; }
int skill_mods(const game_t *s, const unit_t *att, const unit_t *tgt, attack_kind_t kind, part_t *m) {
    int n = 0; const creature_t *a = cdef(att);
    if (a->ability == AB_SWARM) {
        int near = allied_larvas_near(s, att), k = near > RULES.swarm_max ? RULES.swarm_max : near;
        if (k) {
            part(&m[n], L(a->ab_name), k);
            if (LANG == LANG_EN) snprintf(m[n].why, 48, "%d allied Larva%s nearby (max +%d)", near, near > 1 ? "s" : "", RULES.swarm_max);
            else snprintf(m[n].why, 48, "%d Larva%s aliada%s cerca (máx. +%d)", near, near > 1 ? "s" : "", near > 1 ? "s" : "", RULES.swarm_max);
            n++;
        }
    }
    if (kind == K_BREATH && a->ability == AB_BLAZE && att->streak > 0) { part(&m[n], L(a->ab_name), att->streak); snprintf(m[n].why, 48, TR("%d Alientos seguidos", "%d Breaths in a row"), att->streak); n++; }
    if (a->ability == AB_EXECUTE && allies_adj(s, tgt) == 0) { part(&m[n], L(a->ab_name), 2); snprintf(m[n].why, 48, "%s", TR("el objetivo no tiene aliados al lado", "the target has no allies beside it")); n++; }
    if (terr(s, att->x, att->y)->ambush && RULES.ambush_bonus) { part(&m[n], TR("Emboscada", "Ambush"), RULES.ambush_bonus); snprintf(m[n].why, 48, "%s", TR("ataca desde un árbol", "attacks from a tree")); n++; }
    const terrain_t *t = terr(s, tgt->x, tgt->y);
    if (t->cover && kind != K_BREATH) { part(&m[n], TR("Cobertura", "Cover"), -RULES.cover_penalty); snprintf(m[n].why, 48, TR("el objetivo está en %s", "the target stands in %s"), L(t->name)); n++; }
    return n;
}
void compute_damage(const game_t *s, const unit_t *att, const unit_t *tgt, attack_kind_t kind, quality_t q, const int *rolls, int nr, attack_t *out) {
    (void)kind;
    part_t *P = out->parts; int n = 0;
    int base = 0;
    for (int i = 0; i < nr; i++) { int v = RULES.damage_table[rolls[i] - 1]; if (v > base) base = v; }
    part(&P[n], nr > 1 ? TR("Dados de daño", "Damage dice") : TR("Dado de daño", "Damage die"), base);
    bool chain = kind == K_CHAIN;
    if (nr > 1) snprintf(P[n].why, 48, TR("salieron %d y %d; cuenta el mayor", "rolled %d and %d; the higher counts"), rolls[0], rolls[1]);
    else snprintf(P[n].why, 48, TR("salió %d (tabla 1·1·2·2·3·4)", "rolled %d (table 1·1·2·2·3·4)"), rolls[0]);
    n++;
    if (chain) { part(&P[n], TR("Rayo", "Lightning"), 0); snprintf(P[n].why, 48, "%s", TR("ignora ATK y DEF", "ignores ATK and DEF")); n++; }
    else {
        int atk = eff_atk(att), def = eff_def(s, tgt), raw_diff = atk - def;
        int diff = clampi(raw_diff, -RULES.atk_def_clamp, RULES.atk_def_clamp);
        part(&P[n], TR("Ataque contra defensa", "Attack vs defense"), diff);
        char extra[24] = "";
        if (raw_diff != diff) snprintf(extra, sizeof extra, TR(" (tope ±%d)", " (cap ±%d)"), RULES.atk_def_clamp);
        snprintf(P[n].why, 48, "ATK %d − DEF %d%s", atk, def, extra);
        n++;
    }
    if (kind == K_CRUSH && att->cid == C_COLOSSUS && att->streak > 0) { part(&P[n], TR("Furia", "Fury"), att->streak); snprintf(P[n].why, 48, TR("%d Aplastamientos seguidos", "%d Crushes in a row"), att->streak); n++; }
    if (kind == K_RUSH) { part(&P[n], L(cdef(att)->sp_name), 1); snprintf(P[n].why, 48, "%s", TR("llega corriendo", "comes in at a run")); n++; }
    if (kind == K_SHOVE) {
        int dx = tgt->x - att->x, dy = tgt->y - att->y, px = tgt->x + dx, py = tgt->y + dy;
        bool blocked = !inb(s, px, py) || !terr(s, px, py)->pass || unit_at_c(s, px, py);
        if (blocked) { part(&P[n], TR("Choque", "Slam"), 1); snprintf(P[n].why, 48, "%s", TR("no hay hueco para empujarlo", "no room to push it")); n++; }
    }
    if (cdef(att)->ability == AB_CHARGE && att->moved) { part(&P[n], L(cdef(att)->ab_name), 1); snprintf(P[n].why, 48, "%s", TR("se movió antes de atacar", "moved before attacking")); n++; }
    int v = 0; for (int i = 0; i < n; i++) v += P[i].val;
    if (q == Q_PARTIAL) { int half = (v > 0 ? v : 0) / 2; part(&P[n], TR("Éxito parcial", "Partial success"), half - v); snprintf(P[n].why, 48, TR("se queda la mitad: %d → %d", "keeps half: %d → %d"), v, half); n++; v = half; }
    if (q == Q_IMPROVED) { part(&P[n], TR("Éxito mejorado", "Strong success"), 1); snprintf(P[n].why, 48, "%s", TR("la tirada llegó a 5 o más", "the roll reached 5 or more")); n++; v += 1; }
    if (q == Q_CRIT) { part(&P[n], TR("Crítico", "Critical"), 2); snprintf(P[n].why, 48, "%s", TR("sacó un 6 natural", "rolled a natural 6")); n++; v += 2; }
    out->chain_base = v;
    if (cdef(tgt)->ability == AB_FORTRESS) { part(&P[n], L(cdef(tgt)->ab_name), -1); snprintf(P[n].why, 48, "%s", TR("el Golem resiste el golpe", "the Golem shrugs it off")); n++; v -= 1; }
    if (shielded_by_golem(s, tgt)) { part(&P[n], TR("Escudo de piedra", "Stone shield"), -1); snprintf(P[n].why, 48, "%s", TR("un Golem aliado lo cubre", "an allied Golem covers it")); n++; v -= 1; }
    if (tgt->defending) { part(&P[n], TR("En guardia", "On guard"), -1); snprintf(P[n].why, 48, "%s", TR("el objetivo se defendió", "the target was guarding")); n++; v -= 1; }
    int min = RULES.min_damage[q];
    out->raw = v; out->min = min;
    if (v < min && min == 0) { part(&P[n], TR("Sin daño negativo", "No negative damage"), -v); snprintf(P[n].why, 48, "%s", TR("el daño nunca baja de 0", "damage never goes below 0")); n++; v = 0; }
    if (v < min) { part(&P[n], TR("Mínimo garantizado", "Guaranteed minimum"), min - v); snprintf(P[n].why, 48, q == Q_CRIT ? TR("un crítico siempre hace %d", "a critical always deals %d") : TR("un éxito siempre hace %d", "a success always deals %d"), min); n++; v = min; }
    out->nparts = n; out->final_dmg = v;
}
attack_kind_t kind_for(const game_t *s, const unit_t *att, action_id_t a) {
    (void)s;
    if (a != A_ABILITY) return K_ATTACK;
    switch (cdef(att)->special) {
    case SP_CRUSH: return K_CRUSH; case SP_BREATH: return K_BREATH; case SP_CHAIN: return K_CHAIN;
    case SP_RUSH: return K_RUSH; case SP_SHOVE: return K_SHOVE; default: break;
    }
    return K_ATTACK;
}
void attack_odds(const game_t *s, const unit_t *att, const unit_t *tgt, attack_kind_t kind, odds_t *o) {
    memset(o, 0, sizeof *o);
    part_t m[4]; int nm = skill_mods(s, att, tgt, kind, m), mod = 0;
    for (int i = 0; i < nm; i++) mod += m[i].val;
    attack_t tmp; int nd = kind == K_CRUSH ? 2 : 1;
    if (kind == K_CHAIN) o->nchain = chain_targets(s, att, tgt, o->chain_ids);
    for (int r = 1; r <= 6; r++) {
        quality_t q = skill_quality(r, r + mod);
        if (q == Q_CRITFAIL) { o->p_critfail += 1.0f / 6; continue; }
        if (q == Q_FAIL) continue;
        o->p_hit += 1.0f / 6;
        int combos = nd == 2 ? 36 : 6;
        for (int c = 0; c < combos; c++) {
            int rolls[2] = {c % 6 + 1, c / 6 + 1};
            compute_damage(s, att, tgt, kind, q, rolls, nd, &tmp);
            float p = 1.0f / 6 / combos;
            int d = tmp.final_dmg > 15 ? 15 : tmp.final_dmg;
            o->dmg_dist[d] += p; o->e_dmg += p * (tmp.final_dmg < tgt->hp ? tmp.final_dmg : tgt->hp);
            if (tmp.final_dmg >= tgt->hp) o->p_kill += p;
            if (kind == K_CHAIN) for (int k = 0; k < 3; k++) { const unit_t *h = unit_by_id_c(s, o->chain_ids[k]); if (k < o->nchain && h) { int d = chain_hop_damage(s, tmp.chain_base, k + 1, h); o->chain_e[k] += p * (d < h->hp ? d : h->hp); } }
        }
    }
}
bool begin_attack(game_t *s, action_id_t a, int target_id, attack_t *p) {
    memset(p, 0, sizeof(*p));
    unit_t *u = active_unit(s); if (!u) return false;
    const creature_t *c = cdef(u);
    if (a != A_ATTACK && !(a == A_ABILITY && special_is_attack(u->cid))) return false;
    attack_kind_t kind = kind_for(s, u, a);
    target_t t[MAX_TARGETS]; int n = targets_for(s, a, t); bool ok = false;
    for (int i = 0; i < n; i++) if (t[i].unit_id == target_id) ok = true;
    if (!ok) return false;
    if (!spend(u, kind == K_ATTACK ? 1 : c->sp_ap)) return false;
    unit_t *tg = unit_by_id(s, target_id);
    reveal(s, u); /* atacar descubre al Stalker */
    if (kind == K_CRUSH || kind == K_BREATH) u->used_sp = true;
    if (kind == K_RUSH) {
        int lx, ly; rush_landing(s, u, tg, &lx, &ly);
        event_t *m = emit(s, EV_MOVE); m->unit_id = u->id; m->cid = (int8_t)u->cid; m->player = (int8_t)u->player;
        m->fx = u->x; m->fy = u->y; m->x = (int8_t)lx; m->y = (int8_t)ly; m->kind = 4;
        u->x = (int8_t)lx; u->y = (int8_t)ly; u->moved = true; collect_orb(s, u);
    }
    p->ok = true; p->kind = kind; p->attacker_id = u->id; p->target_id = target_id; p->stage = 0;
    p->nmods = skill_mods(s, u, tg, kind, p->mods); p->damage_dice = kind == K_CRUSH ? 2 : 1;
    event_t *e = emit(s, EV_ATTACK); e->unit_id = u->id; e->cid = (int8_t)u->cid; e->tcid = (int8_t)tg->cid; e->player = (int8_t)u->player; e->ability = kind != K_ATTACK;
    return true;
}
void resolve_skill(game_t *s, attack_t *p, int roll) {
    if (p->stage != 0) return;
    unit_t *att = unit_by_id(s, p->attacker_id);
    int mod = 0; for (int i = 0; i < p->nmods; i++) mod += p->mods[i].val;
    p->skill_roll = roll; p->skill_total = roll + mod; p->quality = skill_quality(roll, p->skill_total);
    event_t *e = emit(s, EV_SKILLROLL); e->player = (int8_t)att->player; e->value = (int8_t)roll; e->mod = (int8_t)mod; e->total = (int8_t)p->skill_total; e->quality = (int8_t)p->quality;
    if (p->quality == Q_CRITFAIL || p->quality == Q_FAIL) {
        if (p->quality == Q_CRITFAIL) att->ap = 0;
        p->stage = 2;
        event_t *m = emit(s, EV_MISS); m->unit_id = att->id; m->cid = (int8_t)att->cid; m->player = (int8_t)att->player; m->crit = p->quality == Q_CRITFAIL;
        finish_if_spent(s);
    } else p->stage = 1;
}
void resolve_damage(game_t *s, attack_t *p, const int *rolls) {
    if (p->stage != 1) return;
    unit_t *att = unit_by_id(s, p->attacker_id), *tg = unit_by_id(s, p->target_id);
    compute_damage(s, att, tg, p->kind, p->quality, rolls, p->damage_dice, p);
    for (int i = 0; i < p->damage_dice; i++) p->rolls[i] = rolls[i];
    event_t *e = emit(s, EV_DMGROLL); e->player = (int8_t)att->player; e->nrolls = (int8_t)p->damage_dice; e->rolls[0] = (int8_t)rolls[0]; e->rolls[1] = (int8_t)(p->damage_dice > 1 ? rolls[1] : 0); e->amount = (int8_t)p->final_dmg;
    /* el rayo elige sus saltos antes de que caiga nadie */
    p->nchain = p->kind == K_CHAIN ? chain_targets(s, att, tg, p->chain_ids) : 0;
    for (int k = 0; k < p->nchain; k++) p->chain_dmg[k] = (int8_t)chain_hop_damage(s, p->chain_base, k + 1, unit_by_id(s, p->chain_ids[k]));
    if (p->kind == K_CHAIN) { event_t *c = emit(s, EV_CHAIN); c->unit_id = tg->id; c->player = (int8_t)att->player; c->fx = att->x; c->fy = att->y; c->x = tg->x; c->y = tg->y; }
    int8_t lx = tg->x, ly = tg->y;
    if (p->final_dmg > 0) apply_damage(s, tg, p->final_dmg, false, att);
    else { event_t *b = emit(s, EV_BLOCK); b->unit_id = tg->id; b->cid = (int8_t)tg->cid; b->player = (int8_t)tg->player; b->x = tg->x; b->y = tg->y; }
    for (int k = 0; k < p->nchain && s->phase == PH_BATTLE; k++) {
        unit_t *h = unit_by_id(s, p->chain_ids[k]); if (!h || !h->alive) continue;
        event_t *c = emit(s, EV_CHAIN); c->unit_id = h->id; c->player = (int8_t)att->player; c->fx = lx; c->fy = ly; c->x = h->x; c->y = h->y; c->amount = p->chain_dmg[k];
        lx = h->x; ly = h->y;
        if (p->chain_dmg[k] > 0) apply_damage(s, h, p->chain_dmg[k], false, att);
        else { event_t *b = emit(s, EV_BLOCK); b->unit_id = h->id; b->cid = (int8_t)h->cid; b->player = (int8_t)h->player; b->x = h->x; b->y = h->y; }
    }
    /* Derribar: si sigue en pie y hay hueco, sale empujado (hielo: resbala; lava: quema) */
    if (p->kind == K_SHOVE && tg->alive && s->phase == PH_BATTLE) {
        int dx = tg->x - att->x, dy = tg->y - att->y, px = tg->x + dx, py = tg->y + dy;
        if (inb(s, px, py) && terr(s, px, py)->pass && !unit_at_c(s, px, py)) {
            int fx = tg->x, fy = tg->y;
            while (terr(s, px, py)->ice) { int nx = px + dx, ny = py + dy; if (!inb(s, nx, ny) || !terr(s, nx, ny)->pass || unit_at_c(s, nx, ny)) break; px = nx; py = ny; }
            tg->x = (int8_t)px; tg->y = (int8_t)py;
            event_t *m = emit(s, EV_MOVE); m->unit_id = tg->id; m->cid = (int8_t)tg->cid; m->player = (int8_t)tg->player;
            m->fx = (int8_t)fx; m->fy = (int8_t)fy; m->x = (int8_t)px; m->y = (int8_t)py; m->kind = 5;
            collect_orb(s, tg);
            const terrain_t *tt = terr(s, px, py);
            if (tt->hazard) apply_damage(s, tg, tt->hazard, true, att);
        }
    }
    p->stage = 2;
    finish_if_spent(s);
}

/* ---------- Texto de eventos (monitor serie) ---------- */
void event_text(const event_t *e, char *b, int len) {
#define N(c) cname(c)
    const char *team = e->player == 0 ? TR("Azul", "Blue") : TR("Naranja", "Orange");
    switch (e->type) {
    case EV_START: snprintf(b, len, "%s", TR("Comienza la batalla", "Battle starts")); break;
    case EV_DEPLOY: snprintf(b, len, TR("%s entra al tablero", "%s joins the board"), N(e->cid)); break;
    case EV_ROUND: snprintf(b, len, TR("Comienza la ronda %d", "Round %d begins"), e->round); break;
    case EV_INCOME: snprintf(b, len, TR("Cada invocador recibe %d de maná", "Each summoner gets %d mana"), e->amount); break;
    case EV_ACTIVATE: snprintf(b, len, TR("%s se activa", "%s activates"), N(e->cid)); break;
    case EV_ENDACT: snprintf(b, len, TR("%s termina", "%s ends"), N(e->cid)); break;
    case EV_MOVE: {
        static const tx_t K[6] = {{"%s avanza", "%s moves"}, {"%s se retira", "%s retreats"}, {"%s se entierra y reaparece", "%s burrows and pops up"},
                                  {"%s salta hacia atrás", "%s leaps back"}, {"%s embiste", "%s charges"}, {"%s sale empujado", "%s is pushed"}};
        snprintf(b, len, e->slid && !e->kind ? TR("%s resbala en el hielo", "%s slides on the ice") : L(K[e->kind < 6 ? e->kind : 0]), N(e->cid)); break; }
    case EV_MANA:
        if (e->shrine) snprintf(b, len, TR("Santuario: +1 maná para %s", "Shrine: +1 mana for %s"), team);
        else snprintf(b, len, TR("%s recoge %d de maná residual", "%s picks up %d residual mana"), N(e->cid), e->amount);
        break;
    case EV_CAPTURE: snprintf(b, len, TR("%s conquista un punto de control", "%s captures a control point"), N(e->cid)); break;
    case EV_CONTESTED: snprintf(b, len, TR("Punto de %s disputado: no puntúa", "%s's point is contested"), team); break;
    case EV_ROUNDEND: snprintf(b, len, TR("Fin de la ronda %d · VP %d–%d", "End of round %d · VP %d–%d"), e->round, e->vp0, e->vp1); break;
    case EV_OVER: snprintf(b, len, "%s", e->winner < 0 ? TR("Empate", "Draw") : (e->winner == 0 ? TR("Gana Azul", "Blue wins") : TR("Gana Naranja", "Orange wins"))); break;
    case EV_ATTACK: snprintf(b, len, TR("%s ataca a %s", "%s attacks %s"), N(e->cid), N(e->tcid)); break;
    case EV_SKILLROLL: snprintf(b, len, TR("Habilidad: %d %+d = %d → %s", "Skill: %d %+d = %d → %s"), e->value, e->mod, e->total, L(QUALITY_LABEL[e->quality])); break;
    case EV_DMGROLL: snprintf(b, len, TR("Daño: %d → %d", "Damage: %d → %d"), e->rolls[0], e->amount); break;
    case EV_MISS: snprintf(b, len, e->crit ? TR("%s tropieza", "%s stumbles") : TR("%s falla", "%s misses"), N(e->cid)); break;
    case EV_BLOCK: snprintf(b, len, TR("%s aguanta sin daño", "%s takes no damage"), N(e->cid)); break;
    case EV_DAMAGE: snprintf(b, len, TR("%s recibe %d de daño", "%s takes %d damage"), N(e->cid), e->amount); break;
    case EV_DEATH: snprintf(b, len, TR("%s vuelve a ser arcilla", "%s turns back into clay"), N(e->cid)); break;
    case EV_DEFEND: snprintf(b, len, TR("%s se pone en guardia", "%s stands on guard"), N(e->cid)); break;
    case EV_BUFF: snprintf(b, len, "%s", TR("Refuerzo", "Buff")); break;
    case EV_CHAIN: snprintf(b, len, TR("El rayo alcanza (%d,%d)", "The bolt strikes (%d,%d)"), e->x, e->y); break;
    case EV_HIDE: snprintf(b, len, TR("%s se vuelve transparente", "%s fades from sight"), N(e->cid)); break;
    case EV_REVEAL: snprintf(b, len, TR("%s queda al descubierto", "%s is revealed"), N(e->cid)); break;
    case EV_WALL: snprintf(b, len, TR("%s levanta su escudo de piedra", "%s raises its stone shield"), N(e->cid)); break;
    case EV_POWER: snprintf(b, len, TR("Invocador %s: %s", "%s summoner: %s"), team, L(POWERS[e->power].name)); break;
    default: snprintf(b, len, "?"); break;
    }
#undef N
}

/* ---------- Pruebas ---------- */
static game_t T_G;
static game_t *setup_map(int map, int n0, const int *c0, const int (*p0)[2], int n1, const int *c1, const int (*p1)[2]) {
    army_t a0 = {0}, a1 = {0};
    for (int i = 0; i < n0; i++) army_add(&a0, c0[i]);
    for (int i = 0; i < n1; i++) army_add(&a1, c1[i]);
    game_new(&T_G, map, &a0, &a1);
    int i0 = 0, i1 = 0;
    while (T_G.phase == PH_DEPLOY) {
        int p = T_G.deploy_q[T_G.dq_head].player;
        const int *pos = p == 0 ? p0[i0++] : p1[i1++];
        if (!place_next(&T_G, pos[0], pos[1])) return NULL;
    }
    return &T_G;
}
#define setup(...) setup_map(0, __VA_ARGS__)
static uint8_t seq_src(void *ctx) { int *b = (int *)ctx; return (uint8_t)((*b)++ % 256); }
static void end_both(game_t *s) { /* activa y termina una unidad de cada lado */
    int ids[MAX_UNITS];
    for (int k = 0; k < 2 && s->phase == PH_BATTLE; k++) { if (selectable_units(s, ids)) { activate(s, ids[0]); end_activation(s); } }
}

int run_tests(test_print_t pr, int *total) {
    int pass = 0, tot = 0; char det[48];
    lang_t keep = LANG;
#define TEST(ES, EN, COND) do { bool _ok = (COND); tot++; if (_ok) pass++; if (pr) pr(_ok, TR(ES, EN), det); det[0] = 0; } while (0)
    det[0] = 0;
    { army_t a = {0}; army_add(&a, C_COLOSSUS); army_add(&a, C_COLOSSUS); army_add(&a, C_LARVA); add_result_t r = army_add(&a, C_LARVA);
      TEST("No se puede gastar más de 20 de maná", "You cannot spend more than 20 mana", army_cost(&a) == RULES.mana_budget && r != ADD_OK && a.n == 3); }
    { army_t a = {0}; for (int i = 0; i < 3; i++) { army_add(&a, C_LARVA); army_add(&a, C_SOLDIER); } add_result_t r = army_add(&a, C_LARVA);
      TEST("Máximo 6 criaturas por ejército", "Max 6 creatures per army", a.n == 6 && r == ADD_MAX); }
    { int b = 0, n[7] = {0}; for (int i = 0; i < 252; i++) n[d6(seq_src, &b)]++; bool ok = true; for (int f = 1; f <= 6; f++) ok &= n[f] == 42;
      TEST("El dado no tiene sesgo de módulo (42 por cara)", "No modulo bias (42 per face)", ok); }
    { int n[7] = {0}, N = 60000; for (int i = 0; i < N; i++) n[d6_hw()]++; double e = N / 6.0, chi = 0; for (int f = 1; f <= 6; f++) chi += (n[f] - e) * (n[f] - e) / e;
      snprintf(det, sizeof det, "chi2 = %.2f", chi);
      TEST("D6 uniforme en 60 000 tiradas (chi2 < 20.5)", "Uniform D6 over 60,000 rolls (chi2 < 20.5)", chi < 20.52); }
    { int bad = -1;
      for (int mi = 0; mi < MAP_COUNT && bad < 0; mi++) {
          const map_t *m = &MAPS[mi];
          for (int y = 0; y < m->h; y++) { if ((int)strlen(m->rows[y]) != m->w) bad = mi; for (int x = 0; x < m->w; x++) if (m->rows[y][x] != m->rows[m->h - 1 - y][m->w - 1 - x]) bad = mi; }
      }
      if (bad >= 0) snprintf(det, sizeof det, "%s", MAPS[bad].name);
      TEST("Los 7 mapas son simétricos", "All 7 maps are symmetric", bad < 0); }
    { int bad = -1;
      for (int mi = 0; mi < MAP_COUNT && bad < 0; mi++) {
          const map_t *m = &MAPS[mi]; bool seen[MAX_H][MAX_W] = {{0}}; int qx[100], qy[100], h = 0, t = 0, cnt = 1, total_p = 0;
          int sy = m->deploy[0][1]; seen[sy][0] = true; qx[t] = 0; qy[t++] = sy;
          while (h < t) { int x = qx[h], y = qy[h++]; for (int d = 0; d < 4; d++) { int nx = x + DIRS[d][0], ny = y + DIRS[d][1]; if (nx < 0 || ny < 0 || nx >= m->w || ny >= m->h || seen[ny][nx] || !terrain_of(m->rows[ny][nx])->pass) continue; seen[ny][nx] = true; cnt++; qx[t] = nx; qy[t++] = ny; } }
          for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) total_p += terrain_of(m->rows[y][x])->pass;
          if (cnt != total_p) bad = mi;
      }
      if (bad >= 0) snprintf(det, sizeof det, "%s", MAPS[bad].name);
      TEST("Todas las casillas transitables están conectadas", "All walkable squares are connected", bad < 0); }
    { int c0[] = {C_SOLDIER}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{0, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[0].x = 3; s->units[0].y = 3; activate(s, s->units[0].id);
      target_t r[MAX_TARGETS]; int n = reachable(s, &s->units[0], r); bool wall = false, water = false, ok = false;
      for (int i = 0; i < n; i++) { if (r[i].x == 3 && r[i].y == 2) wall = true; if (r[i].x == 5 && r[i].y == 3) water = true; if (r[i].x == 4 && r[i].y == 3) ok = true; }
      TEST("No se puede entrar en muros ni en agua", "Walls and water cannot be entered", !wall && !water && ok); }
    { int c0[] = {C_SOLDIER}, c1[] = {C_LARVA}; int p0[][2] = {{2, 7}}, p1[][2] = {{0, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[1].x = 2; s->units[1].y = 6; activate(s, s->units[0].id);
      action_t a[8]; int n = actions_for(s, a); bool mv = true, rt = false, at = false;
      for (int i = 0; i < n; i++) { if (a[i].id == A_MOVE) mv = a[i].enabled; if (a[i].id == A_RETREAT) rt = a[i].enabled; if (a[i].id == A_ATTACK) at = a[i].enabled; }
      TEST("Zona de control: trabado solo puede retirarse", "Zone of control: engaged units can only retreat", !mv && rt && at); }
    { int c0[] = {C_SOLDIER}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); activate(s, s->units[0].id); do_move(s, 0, 6, false); do_move(s, 0, 5, false); bool r = do_move(s, 0, 4, false);
      TEST("Al gastar los 2 PA pasa el turno", "Spending 2 AP passes the turn", s->units[0].activated && s->active_id < 0 && s->current == 1 && !r); }
    { int c0[] = {C_COLOSSUS}, c1[] = {C_SOLDIER}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[1].x = 0; s->units[1].y = 6; activate(s, s->units[0].id);
      attack_t p; begin_attack(s, A_ABILITY, s->units[1].id, &p); resolve_skill(s, &p, 4); int rolls[2] = {2, 6}; resolve_damage(s, &p, rolls);
      snprintf(det, sizeof det, TR("daño %d", "damage %d"), p.final_dmg);
      TEST("Aplastamiento usa el dado mayor y deja maná", "Crush keeps the higher die and drops mana", p.final_dmg == 6 && !s->units[1].alive && s->orbs[6][0] == 2 && s->phase == PH_OVER && s->winner == 0); }
    { int c0[] = {C_LARVA}, c1[] = {C_GOLEM}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); attack_t d; int r6[1] = {6}; compute_damage(s, &s->units[0], &s->units[1], K_ATTACK, Q_SUCCESS, r6, 1, &d);
      snprintf(det, sizeof det, TR("daño %d", "damage %d"), d.final_dmg);
      TEST("Fortaleza del Golem reduce el daño", "Golem's Fortress reduces damage", d.final_dmg == 1); }
    { int c0[] = {C_LARVA}, c1[] = {C_GOLEM}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); bool all = true; attack_t d;
      for (int q = Q_PARTIAL; q <= Q_CRIT; q++) for (int r = 1; r <= 6; r++) {
          int rr[1] = {r}; compute_damage(s, &s->units[0], &s->units[1], K_ATTACK, (quality_t)q, rr, 1, &d);
          int sum = 0; for (int i = 0; i < d.nparts; i++) sum += d.parts[i].val;
          if (sum != d.final_dmg) all = false;
      }
      TEST("Las líneas del desglose siempre suman el daño final", "Breakdown lines always add up to the final damage", all); }
    { int c0[] = {C_WYRMLING}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[0].x = 3; s->units[0].y = 0; s->units[1].x = 3; s->units[1].y = 3; activate(s, s->units[0].id);
      target_t t[MAX_TARGETS]; bool blocked = targets_for(s, A_ABILITY, t) == 0; s->units[1].y = 1;
      TEST("El aliento necesita línea despejada", "Breath needs a clear line", blocked && targets_for(s, A_ABILITY, t) == 1); }
    { int c0[] = {C_LARVA}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); end_both(s);
      TEST("La ronda termina cuando todos se activaron", "The round ends when every unit has activated", s->round == 2 && s->current == 1); }
    { int c0[] = {C_LARVA, C_LARVA}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}, {1, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(2, c0, p0, 1, c1, p1); s->units[2].x = 1; s->units[2].y = 6; s->units[1].x = 0; s->units[1].y = 6; /* aliado en diagonal, enemigo arriba */
      part_t m[4]; int n = skill_mods(s, &s->units[0], &s->units[1], K_ATTACK, m);
      int nd = n; s->units[2].x = 0; s->units[2].y = 6; s->units[1].x = 1; s->units[1].y = 6; /* ahora el aliado está al lado */
      n = skill_mods(s, &s->units[0], &s->units[1], K_ATTACK, m);
      TEST("Enjambre solo cuenta Larvas al lado", "Swarm only counts Larvas beside it", nd == 0 && n >= 1 && m[0].val == 1); }
    { int c0[] = {C_SHAMAN}, c1[] = {C_SOLDIER, C_LARVA, C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}, {6, 0}, {5, 0}};
      game_t *s = setup(1, c0, p0, 3, c1, p1);
      s->units[0].x = 3; s->units[0].y = 6; s->units[1].x = 3; s->units[1].y = 4; s->units[2].x = 4; s->units[2].y = 3; s->units[3].x = 5; s->units[3].y = 2;
      activate(s, s->units[0].id); attack_t p;
      bool ok = begin_attack(s, A_ABILITY, s->units[1].id, &p); resolve_skill(s, &p, 4); int r[1] = {5}; resolve_damage(s, &p, r);
      snprintf(det, sizeof det, "%d/%d/%d", s->units[1].hp, s->units[2].hp, s->units[3].hp);
      TEST("El rayo salta en cadena y pierde 1 por salto", "The bolt chains and loses 1 per jump", ok && p.nchain == 2 && s->units[1].hp == 1 && !s->units[2].alive && s->units[3].hp == 1); }
    { int c0[] = {C_LARVA}, c1[] = {C_LARVA}; int p0[][2] = {{3, 7}}, p1[][2] = {{3, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[1].x = 3; s->units[1].y = 6; activate(s, s->units[0].id);
      bool eng = is_engaged(s, &s->units[0]); bool ok = do_special(s, 0, 7);
      TEST("Excavar saca a la Larva del combate", "Burrow gets the Larva out of a fight", eng && ok && s->units[0].x == 0 && s->units[0].y == 7 && s->units[0].ap == 1); }
    { int c0[] = {C_SOLDIER}, c1[] = {C_LARVA}; int p0[][2] = {{7, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[1].x = 7; s->units[1].y = 4; activate(s, s->units[0].id); attack_t p;
      bool ok = begin_attack(s, A_ABILITY, s->units[1].id, &p); resolve_skill(s, &p, 4); int r[1] = {3}; resolve_damage(s, &p, r);
      snprintf(det, sizeof det, "(%d,%d) %d", s->units[0].x, s->units[0].y, p.final_dmg);
      TEST("Embestida corre hasta el enemigo y pega +1", "Charge runs up to the foe and hits +1", ok && s->units[0].y == 5 && p.final_dmg == 4 && !s->units[1].alive); }
    { int c0[] = {C_BEAST}, c1[] = {C_SOLDIER}; int p0[][2] = {{1, 7}}, p1[][2] = {{1, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[1].x = 1; s->units[1].y = 6; activate(s, s->units[0].id); attack_t p;
      bool ok = begin_attack(s, A_ABILITY, s->units[1].id, &p); resolve_skill(s, &p, 4); int r[1] = {1}; resolve_damage(s, &p, r);
      snprintf(det, sizeof det, "(%d,%d) PV %d", s->units[1].x, s->units[1].y, s->units[1].hp);
      TEST("Derribar empuja, y a la lava quema", "Shove pushes, and lava burns", ok && s->units[1].y == 5 && s->units[1].hp == 1); }
    { int c0[] = {C_STALKER}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{0, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[0].x = 0; s->units[0].y = 3; s->units[1].x = 1; s->units[1].y = 3;
      activate(s, s->units[0].id); bool ok = do_special(s, 0, 0); end_activation(s);
      activate(s, s->units[1].id); target_t t[MAX_TARGETS]; int nt = targets_for(s, A_ATTACK, t); end_activation(s);
      TEST("Sombra: nadie le apunta y no conquista puntos", "Shadow: untargetable and captures no points", ok && s->units[0].hidden && nt == 0 && s->control_owner[3][0] == -1 && s->round == 2); }
    { int c0[] = {C_GOLEM, C_SOLDIER}, c1[] = {C_LARVA}; int p0[][2] = {{3, 7}, {4, 7}}, p1[][2] = {{4, 0}};
      game_t *s = setup(2, c0, p0, 1, c1, p1); s->units[1].x = 4; s->units[1].y = 6; /* orden de despliegue: Golem, Larva, Guerrero */
      attack_t a, b; int r[1] = {6};
      compute_damage(s, &s->units[1], &s->units[2], K_ATTACK, Q_CRIT, r, 1, &a);
      activate(s, s->units[0].id); do_special(s, 0, 0);
      compute_damage(s, &s->units[1], &s->units[2], K_ATTACK, Q_CRIT, r, 1, &b);
      snprintf(det, sizeof det, "%d -> %d", a.final_dmg, b.final_dmg);
      TEST("Escudo de piedra resta 1 a los aliados pegados", "Stone shield takes 1 off adjacent allies", s->units[0].walling && b.final_dmg == a.final_dmg - 1); }
    { int c0[] = {C_HUNTER}, c1[] = {C_LARVA, C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{0, 0}, {1, 0}};
      game_t *s = setup(1, c0, p0, 2, c1, p1); s->units[1].x = 1; s->units[1].y = 7; s->units[2].x = 3; s->units[2].y = 4;
      activate(s, s->units[0].id); target_t t[MAX_TARGETS]; int nt = targets_for(s, A_ATTACK, t);
      bool diag = nt == 1 && t[0].unit_id == s->units[2].id; bool ok = do_special(s, 0, 5);
      TEST("Cazador: dispara en diagonal, no a quien tiene pegado, y salta 2", "Hunter: shoots diagonally, not at adjacent foes, leaps 2", diag && ok && s->units[0].y == 5); }
    { army_t a = {0}; for (int i = 0; i < 4; i++) army_add(&a, C_LARVA);
      TEST("Máximo 3 Larvas por ejército", "Max 3 Larvas per army", a.n == 3 && army_can_add(&a, C_LARVA) == ADD_LIMIT); }
    { int c0[] = {C_GOLEM}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); int a0 = eff_atk(&s->units[0]);
      for (int k = 0; k < 4; k++) { int i = s->units[0].player == s->current ? 0 : 1; activate(s, s->units[i].id); end_activation(s); } /* dos rondas sin moverse */
      if (s->current != 0) { activate(s, s->units[1].id); end_activation(s); }
      int a2 = eff_atk(&s->units[0]);
      activate(s, s->units[0].id); target_t t[MAX_TARGETS]; int n = targets_for(s, A_MOVE, t); do_move(s, t[0].x, t[0].y, false);
      int a3 = eff_atk(&s->units[0]);
      snprintf(det, sizeof det, "%d %d %d", a0, a2, a3);
      TEST("Arraigo: el Golem quieto gana ATK y lo pierde al moverse", "Roots: a still Golem gains ATK, loses it on moving", n > 0 && a2 == a0 + 2 && a3 == a0); }
    { int c0[] = {C_COLOSSUS}, c1[] = {C_GOLEM}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[1].x = 0; s->units[1].y = 6;
      int r[2] = {6, 6}; attack_t a, b;
      compute_damage(s, &s->units[0], &s->units[1], K_CRUSH, Q_SUCCESS, r, 2, &a);
      s->units[0].streak = 2; compute_damage(s, &s->units[0], &s->units[1], K_CRUSH, Q_SUCCESS, r, 2, &b);
      TEST("Furia: el Colossus pega +2 tras dos Aplastamientos seguidos", "Fury: Colossus hits +2 after two Crushes in a row", b.final_dmg == a.final_dmg + 2); }
    { int c0[] = {C_WYRMLING}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[0].x = 3; s->units[0].y = 7; s->units[1].x = 3; s->units[1].y = 3;
      activate(s, s->units[0].id); target_t t[MAX_TARGETS]; int n0 = targets_for(s, A_ABILITY, t);
      s->units[0].streak = 2; int n2 = targets_for(s, A_ABILITY, t); part_t m[4]; int nm = skill_mods(s, &s->units[0], &s->units[1], K_BREATH, m);
      TEST("Llama creciente: más alcance y +2 al acertar", "Growing flame: more range and +2 to hit", n0 == 0 && n2 == 1 && nm >= 1 && m[0].val == 2); }
    { int c0[] = {C_LARVA}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[0].x = 0; s->units[0].y = 3; end_both(s);
      TEST("Un punto sin enemigos cerca da 1 VP", "An uncontested point gives 1 VP", s->vp[0] == 1 && s->vp[1] == 0 && s->control_owner[3][0] == 0); }
    { int c0[] = {C_GOLEM}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->units[0].x = 0; s->units[0].y = 3; end_both(s);
      s->units[1].x = 0; s->units[1].y = 2; end_both(s);
      snprintf(det, sizeof det, "VP %d-%d", s->vp[0], s->vp[1]);
      TEST("Un punto disputado no da VP a nadie", "A contested point gives no VP", s->vp[0] == 1 && s->vp[1] == 0 && s->control_owner[3][0] == 0); }
    { int c0[] = {C_LARVA}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); bool r1 = s->mana[0] == 1 && s->mana[1] == 1; end_both(s);
      TEST("Cada ronda da 1 de maná a cada invocador", "Each round gives each summoner 1 mana", r1 && s->mana[0] == 2 && s->mana[1] == 2); }
    { int c0[] = {C_SOLDIER}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->mana[0] = 10; s->units[0].hp = 1;
      bool a = use_power(s, P_HARDEN, s->units[0].id, 0, 0, NULL); bool b = use_power(s, P_SURGE, -1, 0, 0, NULL); activate(s, s->units[0].id);
      snprintf(det, sizeof det, "PV %d · PA %d", s->units[0].hp, s->units[0].ap);
      TEST("Los poderes no gastan PA y hay uno por ronda", "Powers cost no AP, one per round", a && !b && s->units[0].hp == 3 && s->units[0].ap == 2 && s->mana[0] == 8); }
    { int c0[] = {C_SOLDIER}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->mana[0] = 20;
      use_power(s, P_HASTE, -1, 0, 0, NULL); end_both(s);   /* ronda 1: Azul usa Impulso */
      power_state_t ps[P_COUNT]; int keep_cur = s->current; s->current = 0; powers_for(s, ps);
      bool same_blocked = !ps[P_HASTE].enabled, other_ok = ps[P_HARDEN].enabled; s->current = keep_cur;
      snprintf(det, sizeof det, TR("ronda %d", "round %d"), s->round);
      TEST("Un poder por ronda, sin repetir el mismo seguido", "One power per round, never the same twice in a row", same_blocked && other_ok && s->round == 2); }
    { TEST("Costos: Impulso 1, Endurecer 2, Ímpetu 3, Reinvocar más caro", "Costs: Dash 1, Harden 2, Momentum 3, Resummon pricier",
           power_cost_of(P_HASTE, 0) == 1 && power_cost_of(P_HARDEN, 0) == 2 && power_cost_of(P_SURGE, 0) == 3 && resummon_cost(C_LARVA) == 3 && resummon_cost(C_COLOSSUS) == 7); }
    { int c0[] = {C_SOLDIER}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->mana[0] = 3; use_power(s, P_SURGE, -1, 0, 0, NULL); activate(s, s->units[0].id);
      do_move(s, 0, 6, false); do_move(s, 0, 5, false);
      TEST("Ímpetu da 3 PA a la siguiente criatura", "Momentum gives the next creature 3 AP", s->units[0].ap == 1 && s->active_id == s->units[0].id); }
    { int c0[] = {C_GOLEM}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(1, c0, p0, 1, c1, p1); s->mana[0] = 2; target_t r[MAX_TARGETS];
      activate(s, s->units[0].id); int before = reachable(s, &s->units[0], r); cancel_activation(s);
      use_power(s, P_HASTE, -1, 0, 0, NULL); activate(s, s->units[0].id); int after = reachable(s, &s->units[0], r);
      snprintf(det, sizeof det, "%d -> %d", before, after);
      TEST("Impulso suma 2 MOV en la siguiente activación", "Dash adds 2 MOV", after > before && eff_mov(&s->units[0]) == 3); }
    { int c0[] = {C_BEAST, C_LARVA}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}, {7, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup(2, c0, p0, 1, c1, p1); unit_t *b = &s->units[0];
      b->hp = 0; b->alive = false; s->mana[0] = 5;
      bool r = use_power(s, P_RESUMMON, b->id, 3, 7, NULL);
      snprintf(det, sizeof det, "PV %d", b->hp);
      TEST("Reinvocar devuelve una criatura con media vida", "Resummon brings a creature back at half HP", r && b->alive && b->hp == 3 && b->activated && s->mana[0] == 0); }
    { int c0[] = {C_SOLDIER}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup_map(2, 1, c0, p0, 1, c1, p1); s->units[0].x = 1; s->units[0].y = 1; s->units[1].x = 2; s->units[1].y = 1;
      part_t m[4]; int n = skill_mods(s, &s->units[0], &s->units[1], K_ATTACK, m); bool amb = false; for (int i = 0; i < n; i++) if (m[i].val == 1) amb = true;
      TEST("Atacar desde un árbol da +1 (emboscada)", "Attacking from a tree gives +1 (ambush)", amb); }
    { int c0[] = {C_SOLDIER}, c1[] = {C_LARVA}; int p0[][2] = {{0, 7}}, p1[][2] = {{7, 0}};
      game_t *s = setup_map(3, 1, c0, p0, 1, c1, p1); s->units[0].x = 2; s->units[0].y = 0; activate(s, s->units[0].id);
      do_move(s, 2, 2, false);
      snprintf(det, sizeof det, "(%d,%d)", s->units[0].x, s->units[0].y);
      TEST("El hielo hace resbalar hasta salir", "Ice slides you until you leave it", s->units[0].x == 2 && s->units[0].y == 6); }
#undef TEST
    LANG = keep;
    if (total) *total = tot;
    return pass;
}
