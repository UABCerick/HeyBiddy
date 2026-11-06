/*
 * Laboratorio del cerebro de Biddy (solo PC). Desde apps/arcilla/:
 *
 *   gcc -std=c11 -O2 -Igame game/engine.c game/ai.c game/brain.c host/brain_lab.c -lm -o /tmp/lab
 *
 *   /tmp/lab torneo  <partidas> [semilla]   Fácil/Medio/Difícil entre sí (cambian de lado y de mapa)
 *   /tmp/lab ejercito <partidas> [semilla]  variedad: ejércitos distintos, repeticiones, doctrinas, fuerza
 *   /tmp/lab tiempo  <partidas> [semilla]   milisegundos por decisión de cada nivel (en la PC)
 *   /tmp/lab valores <balance.csv ...>      recalcula la tabla VALUE de brain.c (regresión logística)
 *
 * Las partidas usan exactamente el camino de la placa: brain_build_army, brain_deploy_index y
 * brain_decide, y luego aplican la decisión como lo hace la interfaz.
 */
#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "ai.h"
#include "brain.h"
#include "engine.h"

uint8_t platform_random_byte(void) { return (uint8_t)(rand() >> 7); }
bool hal_load(const char *key, void *data, int len) { (void)key; (void)data; (void)len; return false; }
void hal_store(const char *key, const void *data, int len) { (void)key; (void)data; (void)len; }
void hal_think_async(void (*fn)(void)) { fn(); }

static const char *LVL_NAME[LVL_COUNT] = {"Fácil", "Medio", "Difícil"};
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1000.0 + t.tv_nsec / 1e6; }

/* Aplica una decisión igual que ui.c (los dados se tiran con d6_hw). Devuelve false si no hizo nada. */
static bool apply(game_t *g, const brain_decision_t *d) {
    switch (d->kind) {
    case BD_POWER: return use_power(g, d->power.power, d->power.unit_id, d->power.x, d->power.y, NULL);
    case BD_ACTIVATE: return activate(g, d->unit_id);
    case BD_STEP: {
        ai_step_t st = d->step;
        switch (st.type) {
        case AS_END: end_activation(g); return true;
        case AS_DEFEND: return do_defend(g);
        case AS_MOVE: return do_move(g, st.x, st.y, false);
        case AS_RETREAT: return do_move(g, st.x, st.y, true);
        case AS_ABILITY: { unit_t *u = active_unit(g); if (u && !special_is_attack(u->cid)) return do_special(g, st.x, st.y); }
            /* fallthrough */
        case AS_ATTACK: {
            attack_t p;
            if (!begin_attack(g, st.type == AS_ATTACK ? A_ATTACK : A_ABILITY, st.target, &p)) return false;
            resolve_skill(g, &p, d6_hw());
            if (p.stage == 1) { int r[2] = {d6_hw(), d6_hw()}; resolve_damage(g, &p, r); }
            return true;
        }
        }
        return false;
    }
    default: return false;
    }
}

static double DEC_MS[LVL_COUNT], DEC_MAX[LVL_COUNT]; static long DEC_N[LVL_COUNT];
/* Una partida completa. Devuelve el ganador (-1 empate). */
static int play(level_t l0, level_t l1, int map, brain_memory_t *mem) {
    static game_t g; army_t a[2]; level_t lv[2] = {l0, l1};
    for (int p = 0; p < 2; p++) { int d = brain_build_army(lv[p], &mem[lv[p]], &STYLE_NEUTRAL, &a[p]); brain_memory_push(&mem[lv[p]], &a[p], d); }
    game_new(&g, map, &a[0], &a[1]);
    while (g.phase == PH_DEPLOY) {
        target_t c[MAX_TARGETS]; int n = deploy_cells(&g, c); if (!n) { auto_place_rest(&g); break; }
        int i = brain_deploy_index(&g, lv[g.deploy_q[g.dq_head].player]);
        place_next(&g, c[i].x, c[i].y);
    }
    int guard = 0, steps = 0, last_active = -2;
    while (g.phase == PH_BATTLE && guard++ < 3000) {
        brain_decision_t d; level_t l = lv[g.current];
        double t0 = now_ms(); brain_decide(&g, l, &d); double dt = now_ms() - t0; DEC_MS[l] += dt; DEC_N[l]++; if (dt > DEC_MAX[l]) DEC_MAX[l] = dt;
        if (g.active_id != last_active) { steps = 0; last_active = g.active_id; }
        bool ok = apply(&g, &d);
        /* mismas protecciones que la interfaz: nunca se queda atorado */
        if (!ok || (d.kind == BD_STEP && ++steps > 8)) {
            if (g.active_id >= 0) end_activation(&g);
            else { int ids[MAX_UNITS]; int n = selectable_units(&g, ids); if (n) { activate(&g, ids[0]); end_activation(&g); } }
        }
    }
    return g.phase == PH_OVER ? g.winner : -1;
}

static void torneo(int N) {
    brain_memory_t mem[LVL_COUNT]; memset(mem, 0, sizeof mem);
    printf("Torneo: %d partidas por cruce (lados y mapas alternados)\n", N);
    for (int a = 0; a < LVL_COUNT; a++) for (int b = a; b < LVL_COUNT; b++) {
        int wa = 0, wb = 0, dr = 0;
        for (int i = 0; i < N; i++) {
            bool swap = i % 2; int map = (i / 2) % MAP_COUNT;
            int w = swap ? play(b, a, map, mem) : play(a, b, map, mem);
            if (w < 0) dr++; else if ((w == 0) != swap) wa++; else wb++;
        }
        printf("  %-8s vs %-8s  gana %-8s %5.1f%%  · %-8s %5.1f%%  · empates %4.1f%%\n", LVL_NAME[a], LVL_NAME[b],
               LVL_NAME[a], 100.0 * wa / N, LVL_NAME[b], 100.0 * wb / N, 100.0 * dr / N);
    }
    for (int l = 0; l < LVL_COUNT; l++) printf("  %s: %.2f ms por decisión (PC)\n", LVL_NAME[l], DEC_MS[l] / (DEC_N[l] ? DEC_N[l] : 1));
}

static void ejercito(int N) {
    static const char *CN[C_COUNT] = {"Larva", "Guerrero", "Beast", "Colossus", "Stalker", "Shaman", "Golem", "Wyrmling", "Cazador"};
    for (int l = 0; l < LVL_COUNT; l++) {
        brain_memory_t mem; memset(&mem, 0, sizeof mem);
        int same_prev = 0, docs[DOCTRINE_COUNT] = {0}, cre[C_COUNT] = {0}, distinct = 0; double score = 0, cost = 0, units = 0;
        static army_t seen[4000]; int nseen = 0; army_t prev = {0};
        for (int i = 0; i < N; i++) {
            army_t a; int d = brain_build_army((level_t)l, &mem, &STYLE_NEUTRAL, &a); brain_memory_push(&mem, &a, d);
            docs[d]++; score += brain_army_score(&a); cost += army_cost(&a); units += a.n;
            for (int k = 0; k < a.n; k++) cre[a.cid[k]]++;
            if (i && prev.n == a.n && !memcmp(prev.cid, a.cid, a.n)) same_prev++;
            bool found = false; for (int k = 0; k < nseen; k++) if (seen[k].n == a.n && !memcmp(seen[k].cid, a.cid, a.n)) { found = true; break; }
            if (!found && nseen < 4000) { seen[nseen++] = a; distinct++; }
            prev = a;
            if (i < 5) { printf("  %s #%d %-11s (%2d maná):", LVL_NAME[l], i + 1, DOCTRINES[d].name.es, army_cost(&a)); for (int k = 0; k < a.n; k++) printf(" %s", CN[a.cid[k]]); printf("\n"); }
        }
        printf("%s: %d ejércitos · %d distintos · igual al anterior %d · fuerza media %.2f · maná medio %.1f · criaturas %.1f\n   doctrinas:",
               LVL_NAME[l], N, distinct, same_prev, score / N, cost / N, units / N);
        for (int d = 0; d < DOCTRINE_COUNT; d++) printf(" %s %d%%", DOCTRINES[d].name.es, 100 * docs[d] / N);
        printf("\n   criaturas:"); for (int c = 0; c < C_COUNT; c++) printf(" %s %.2f", CN[c], cre[c] / (double)N); printf("\n");
    }
}

static void valores(int argc, char **argv) {
    /* lee los CSV de host/balance.c: columnas winner (3.ª) y a0_*, a1_* al final */
    static float X[20000][C_COUNT]; static int Y[20000]; int n = 0;
    for (int f = 0; f < argc; f++) {
        FILE *fp = fopen(argv[f], "r"); if (!fp) continue; char line[2048];
        if (!fgets(line, sizeof line, fp)) { fclose(fp); continue; }
        while (fgets(line, sizeof line, fp) && n < 20000) {
            int col[64], nc = 0; char *t = strtok(line, ",\n"); while (t && nc < 64) { col[nc++] = atoi(t); t = strtok(NULL, ",\n"); }
            if (nc < 2 * C_COUNT + 3 || col[2] < 0) continue;
            for (int c = 0; c < C_COUNT; c++) X[n][c] = (float)(col[nc - 2 * C_COUNT + c] - col[nc - C_COUNT + c]);
            Y[n++] = col[2] == 0;
        }
        fclose(fp);
    }
    double b[C_COUNT] = {0}, b0 = 0;
    for (int it = 0; it < 4000; it++) {
        double g[C_COUNT] = {0}, g0 = 0;
        for (int i = 0; i < n; i++) { double z = b0; for (int c = 0; c < C_COUNT; c++) z += b[c] * X[i][c]; double e = 1 / (1 + exp(-z)) - Y[i]; g0 += e; for (int c = 0; c < C_COUNT; c++) g[c] += e * X[i][c]; }
        b0 -= 0.05 * g0 / n; for (int c = 0; c < C_COUNT; c++) b[c] -= 0.05 * (g[c] / n + 0.002 * b[c]);
    }
    printf("%d partidas\nstatic const float VALUE[C_COUNT] = {", n);
    for (int c = 0; c < C_COUNT; c++) printf("%.3ff%s", b[c], c < C_COUNT - 1 ? ", " : "};\n");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "uso: lab torneo|ejercito|tiempo <partidas> [semilla] · lab valores <csv...>\n"); return 1; }
    if (!strcmp(argv[1], "valores")) { valores(argc - 2, argv + 2); return 0; }
    int N = atoi(argv[2]); srand(argc > 3 ? (unsigned)atoi(argv[3]) : 1);
    if (!strcmp(argv[1], "torneo")) torneo(N);
    else if (!strcmp(argv[1], "ejercito")) ejercito(N);
    else if (!strcmp(argv[1], "duelo")) {
        /* duelo N semilla nivel_rival expand margin noise caution aggression objective: perfil de prueba (como Difícil) contra un nivel */
        ai_profile_t p = LEVELS[LVL_HARD].play; level_t rival = (level_t)atoi(argv[4]);
        if (argc > 5) p.expand = atoi(argv[5]);
        if (argc > 6) p.margin = (float)atof(argv[6]);
        if (argc > 7) p.noise = (float)atof(argv[7]);
        if (argc > 8) p.caution = (float)atof(argv[8]);
        if (argc > 9) p.aggression = (float)atof(argv[9]);
        if (argc > 10) p.objective = (float)atof(argv[10]);
        brain_override_profile(LVL_HARD, &p);
        brain_memory_t mem[LVL_COUNT]; memset(mem, 0, sizeof mem); int w = 0, l = 0;
        for (int i = 0; i < N; i++) { bool sw = i % 2; int r = sw ? play(rival, LVL_HARD, (i / 2) % MAP_COUNT, mem) : play(LVL_HARD, rival, (i / 2) % MAP_COUNT, mem);
            if (r >= 0) { if ((r == 0) != sw) w++; else l++; } }
        printf("prueba %d · rival %d · empates %d\n", w, l, N - w - l);
    }
    else if (!strcmp(argv[1], "tiempo")) {
        brain_memory_t mem[LVL_COUNT]; memset(mem, 0, sizeof mem);
        for (int i = 0; i < N; i++) for (int l = 0; l < LVL_COUNT; l++) play((level_t)l, (level_t)l, i % MAP_COUNT, mem);
        for (int l = 0; l < LVL_COUNT; l++) printf("%s: %.3f ms por decisión, máximo %.1f ms (%ld decisiones)\n", LVL_NAME[l], DEC_MS[l] / DEC_N[l], DEC_MAX[l], DEC_N[l]);
    }
    return 0;
}
