/* Interfaz del juego: traducción de ui.js del prototipo web a C sobre gfx.c. */
#include "ui.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gfx.h"
#include "texts.h"
#include "art_data.h"
#include "hiart_data.h"
#include "brain.h"

#define F_XS (&FONT_XS)
#define F_S (&FONT_S)
#define F_M (&FONT_M)
#define F_T1 (&FONT_T1)
#define F_T2 (&FONT_T2)
#define F_T3 (&FONT_T3)
#define CX 180
#define CY 180

/* ---------- Colores ---------- */
#define C_INK RGB(0xef, 0xe6, 0xda)
#define C_DIM RGB(0x8e, 0x83, 0x99)
#define C_FAINT RGB(0x3a, 0x30, 0x43)
#define C_BLUE RGB(0x5b, 0x9c, 0xf0)
#define C_BLUED RGB(0x2f, 0x5f, 0xb3)
#define C_BLUEL RGB(0xa9, 0xcd, 0xff)
#define C_ORANGE RGB(0xf0, 0x91, 0x3f)
#define C_ORANGED RGB(0xb8, 0x5a, 0x1f)
#define C_ORANGEL RGB(0xff, 0xc5, 0x8f)
#define C_MANA RGB(0xb4, 0x8c, 0xff)
#define C_GOOD RGB(0x7f, 0xd1, 0x8b)
#define C_BAD RGB(0xef, 0x5b, 0x4c)
#define C_GOLD RGB(0xf2, 0xc8, 0x4b)
#define C_OUT RGB(0x1a, 0x12, 0x20)
#define C_CLAY RGB(0xd9, 0x9a, 0x6c)
#define C_VEIL RGB(0x0c, 0x09, 0x0f)
typedef struct { uint16_t b, d, l; } team_col_t;
static const team_col_t TEAM[2] = {{C_BLUE, C_BLUED, C_BLUEL}, {C_ORANGE, C_ORANGED, C_ORANGEL}};
static uint16_t qcol(quality_t q) {
    switch (q) { case Q_CRITFAIL: case Q_FAIL: return C_BAD; case Q_PARTIAL: return C_ORANGEL; case Q_IMPROVED: return C_GOOD; case Q_CRIT: return C_GOLD; default: return C_INK; }
}

/* ---------- Sprites 12×12 (los mismos del prototipo) ---------- */
static const char *ICON[P_COUNT][9] = {
    {"xxxxxxxxx", "x.......x", "x..xxx..x", "x..xxx..x", "x.......x", ".x.....x.", "..x...x..", "...x.x...", "....x...."},
    {"x...x....", ".x...x...", "..x...x..", "...x...x.", "....x...x", "...x...x.", "..x...x..", ".x...x...", "x...x...."},
    {".....xxx.", "....xxx..", "...xxx...", "..xxxxxx.", "....xxx..", "...xxx...", "..xx.....", ".xx......", "x........"},
    {"...xxx...", ".xx...xx.", "x.......x", "x...x...x", "x..xxx..x", "x...x...x", "x.......x", ".xx...xx.", "...xxx..."},
};
static const uint16_t ICON_COL[P_COUNT] = {RGB(0xcf, 0xc6, 0xb8), C_GOOD, C_GOLD, C_MANA};
typedef enum { SM_NORMAL, SM_FLASH, SM_SIL, SM_GHOST } spr_mode_t;
/* Paletas del arte con los colores de cada equipo (2 = neutral, para banderas sin dueño) */
static uint16_t TPAL[3][ART_NPAL];
static void build_palettes(void) {
    static const uint8_t RAMP[3][4][3] = {{{30, 60, 130}, {60, 110, 200}, {120, 170, 240}, {200, 225, 255}},
                                          {{130, 50, 20}, {210, 100, 40}, {245, 160, 90}, {255, 215, 170}},
                                          {{110, 106, 120}, {200, 196, 206}, {236, 232, 240}, {255, 255, 255}}};
    for (int t = 0; t < 3; t++) {
        memcpy(TPAL[t], ART_PAL, sizeof ART_PAL);
        for (int k = 0; k < 4; k++) TPAL[t][ART_TEAM0 + k] = RGB(RAMP[t][k][0], RAMP[t][k][1], RAMP[t][k][2]);
    }
}
/* Sprite de tablero (16 px). sc se mide como antes, sobre 12 px, para no mover los diseños. Naranja mira a la izquierda. */
static void sprite(int cid, int ti, float x, float y, float sc, spr_mode_t mode, uint8_t alpha) {
    const art_t *a = &ART_UNIT[cid];
    int m = mode == SM_FLASH ? 2 : (mode == SM_SIL || mode == SM_GHOST) ? 1 : 0;
    uint16_t sil = mode == SM_GHOST ? RGB(0x6b, 0x62, 0x76) : 0xFFFF;
    const hiart_t *h = HI_MINI[cid][ti];
    if (h) { /* figura detallada: se ajusta a la caja de 12*sc, centrada y apoyada abajo */
        float box = 12 * sc, s2 = box / (h->h > h->w ? h->h : h->w);
        gfx_art(h->px, h->w, h->h, x + (box - h->w * s2) / 2, y + box - h->h * s2, s2, false, h->pal, alpha, m, sil);
        return;
    }
    gfx_art(a->px, a->w, a->h, x, y, sc * 12.0f / 16.0f, ti == 1, TPAL[ti], alpha, m, sil);
}
static void panel(int x, int y, int w, int h, uint16_t edge);
/* Figura detallada completa, ajustada a una caja (x, y, w, h), apoyada abajo. false si no hay arte detallado. */
static bool portrait_full(int cid, int ti, float x, float y, float w, float h, uint8_t alpha, int mode) {
    const hiart_t *a = HI_FULL[cid][ti]; if (!a) return false;
    float s = fminf(w / a->w, h / a->h);
    gfx_art(a->px, a->w, a->h, x + (w - a->w * s) / 2, y + h - a->h * s, s, false, a->pal, alpha, mode, 0xFFFF);
    return true;
}
/* Medio cuerpo (de la cabeza a la cintura) con la altura pedida; x es el centro */
static bool portrait_half(int cid, int ti, float cx, float y, float height, uint8_t alpha, int mode) {
    const hiart_t *a = HI_FULL[cid][ti]; if (!a) return false;
    int sh = a->h * HI_CROP[cid] / 100; float s = height / sh;
    gfx_art_sub(a->px, a->w, a->h, 0, 0, a->w, sh, cx - a->w * s / 2, y, s, false, a->pal, alpha, mode, 0xFFFF);
    return true;
}
/* Retrato de busto (32 px) */
static void bust(int cid, int ti, float x, float y, float sc, bool flip, uint8_t alpha, int mode) {
    const art_t *a = &ART_BUST[cid];
    gfx_art(a->px, a->w, a->h, x, y, sc, flip, TPAL[ti], alpha, mode, 0xFFFF);
}
static void icon(int id, int x, int y, int sc, uint8_t alpha) {
    for (int pass = 0; pass < 2; pass++) for (int j = 0; j < 9; j++) for (int i = 0; i < 9; i++)
        if (ICON[id][j][i] == 'x') gfx_fill_a(x + i * sc + (pass ? 0 : 2), y + j * sc + (pass ? 0 : 2), sc, sc, pass ? ICON_COL[id] : C_OUT, alpha);
}

/* ---------- Estado ---------- */
typedef enum { SC_SPLASH, SC_MENU, SC_OPTIONS, SC_RULES, SC_PRIVACY, SC_BUILDER, SC_REVEAL, SC_MAP, SC_DEPLOY, SC_BATTLE, SC_VICTORY, SC_LEVEL } scene_t;
typedef enum { M_SELECT, M_POWER, M_PTARGET, M_PFALLEN, M_PCELL, M_PAUSE, M_ACTION, M_TARGET, M_INSPECT, M_DICE, M_BUSY } ui_mode_t;
typedef enum { AN_MOVE, AN_POP, AN_LUNGE, AN_HURT, AN_FLOAT, AN_DEATH, AN_HAPTIC, AN_BURST, AN_BOLT } anim_type_t;
typedef struct { bool on, deferred, done; anim_type_t type; int id, delay; uint32_t t0, dur; int x, y, fx, fy, cid, p; char text[28]; uint16_t color; } anim_t;
typedef struct { float x, y, vx, vy, life; uint16_t c; int s; } particle_t;
#define MAX_ANIMS 48
#define MAX_PARTICLES 220

typedef struct { float x, y, a; int face; } trail_t;
static const texts_t *T = &TXT_ES;
static struct {
    scene_t scene; uint32_t t0, busy_until;
    int menu_idx, opt_idx, rules_page;
    army_t armies[2]; int b_player, b_idx; char msg[64]; uint32_t msg_t, add_flash;
    int priv_key, priv_color; int map_idx;
    game_t g; bool has_game; int deploy_idx;
    ui_mode_t mode; int sel_idx, act_idx, nact; action_t actions[8];
    target_t targets[MAX_TARGETS]; int ntargets, tgt_idx; action_id_t action_id;
    power_state_t powers[P_COUNT]; int pow_idx;
    target_t fallen[MAX_UNITS]; int nfallen, fallen_idx; target_t fallen_pick;
    attack_t pending;
    struct {
        int stage, phase, n, page, hop, ntrail, flash; bool maxed;
        float intensity, spin, ang[2], ang_start[2], ang_end[2], height;
        uint32_t t0, last_t, roll_t0, roll_dur, last_face, result_t, max_t, click_t, contact_t, flash_t;
        int values[2], faces[2], bounces, last_q[2];
        trail_t trail[2][5];
    } dice;
    int inspect_id, pause_idx;
    struct { bool on, lng; char text[40], sub[200]; int team; uint32_t t0; } banner;
    uint16_t last_seq; int last_current, last_round;
    anim_t anims[MAX_ANIMS]; particle_t parts[MAX_PARTICLES];
    uint32_t shake_t0; float shake_mag;
    int dmg[2], kills[2], rolls, crits[2];
    bool holding; uint32_t hold_start;
    uint32_t after_action_at, victory_at;
    bool haptic_on; int tests_pass, tests_total;
    uint32_t reveal_burst, vic_burst;
    struct { bool on; uint32_t t0; int cid, p, tcid, tp; bool has_t; int kind, x, y, target; action_id_t act; } cut; /* corte de especial */
    uint32_t cam_t;
    /* contra Biddy: el jugador 1 lo maneja brain.c */
    bool vs; level_t lvl; int doctrine, lvl_idx; bool recorded;
    struct { int state; uint32_t at, t0; brain_decision_t d; int steps, active; int clicks, want; } ai;
} U;
enum { AI_IDLE, AI_THINK, AI_SHOW_ACTION, AI_SHOW_TARGET };
#define BIDDY 1

static uint32_t NOW;
static const char *tname(int p) { return U.vs && p == BIDDY ? T->biddy : T->team[p]; }
static const char *tword(int p) { return U.vs && p == BIDDY ? T->biddy_word : T->team_word[p]; }
static bool ai_turn(void) { return U.vs && U.scene == SC_BATTLE && U.g.phase == PH_BATTLE && U.g.current == BIDDY; }
static void go(scene_t s) { U.scene = s; U.t0 = NOW; }
static void flash_msg(const char *m) { snprintf(U.msg, sizeof U.msg, "%s", m); U.msg_t = NOW; }
static void haptic(haptic_t k) { if (U.haptic_on) hal_haptic(k); }
static bool blink(uint32_t ms) { return (NOW / ms) % 2 == 1; }
static int stepi(int i, int n, input_t ev) { if (n <= 0) return 0; return (i + (ev == IN_R ? 1 : -1) + n) % n; }
static void set_lang(lang_t l) { eng_set_lang(l); T = l == LANG_EN ? &TXT_EN : &TXT_ES; }

/* ---------- Pequeños dibujos ---------- */
static void txt(const char *s, int x, int y, const font_t *f, uint16_t c) { gfx_text(s, x, y, f, c, AL_CENTER, true); }
static void txtl(const char *s, int x, int y, const font_t *f, uint16_t c) { gfx_text(s, x, y, f, c, AL_LEFT, true); }
static void gem(int x, int y, uint16_t c) { gfx_fill(x - 3, y - 4, 7, 9, C_OUT); gfx_fill(x - 2, y - 3, 5, 7, c); gfx_fill(x - 1, y - 2, 2, 2, RGB(0xe3, 0xd3, 0xff)); }
static void flag(int x, int y, uint16_t c) { gfx_fill(x - 3, y - 5, 1, 10, C_INK); gfx_fill(x - 2, y - 5, 6, 4, c); }
static void pips(int x, int y, int n, int max, uint16_t on, uint16_t off, int sz) {
    for (int i = 0; i < max; i++) { gfx_fill(x + i * (sz + 2) - 1, y - 1, sz + 2, sz + 2, C_OUT); gfx_fill(x + i * (sz + 2), y, sz, sz, i < n ? on : off); }
}
static void dots(int n, int idx, int y, uint16_t on) {
    int x0 = CX - (n * 8) / 2;
    for (int i = 0; i < n; i++) gfx_fill(x0 + i * 8, y, 5, 5, i == idx ? on : C_FAINT);
}
static void hp_bar(int x, int y, int w, int hp, int max, uint16_t c) {
    gfx_fill(x - 1, y - 1, w + 2, 5, C_OUT); gfx_fill(x, y, w, 3, RGB(0x3b, 0x2a, 0x2a));
    uint16_t col = hp * 2 > max ? c : hp * 4 > max ? C_GOLD : C_BAD;
    int fw = w * hp / max; if (fw < 1) fw = 1;
    gfx_fill(x, y, fw, 3, col);
}
static void veil(void) { gfx_background(); gfx_fill_a(0, 0, SCR, SCR, C_VEIL, 140); }
static void list_menu(const char *const *items, int n, int idx, int y0, int gap) {
    for (int i = 0; i < n; i++) {
        int y = y0 + i * gap; bool on = i == idx;
        if (on) { gfx_fill(70, y - 16, 220, 32, C_FAINT); gfx_fill(70, y - 16, 3, 32, C_GOLD); }
        txt(items[i], CX, y, on ? F_T1 : F_M, on ? C_INK : C_DIM);
    }
}
#define FMT(buf, ...) (snprintf(buf, sizeof buf, __VA_ARGS__), buf)

/* ---------- Animaciones ---------- */
static anim_t *add_anim(anim_type_t type, int id, uint32_t dur, int delay, bool defer) {
    for (int i = 0; i < MAX_ANIMS; i++) if (!U.anims[i].on) {
        anim_t *a = &U.anims[i]; memset(a, 0, sizeof *a);
        a->on = true; a->type = type; a->id = id; a->dur = dur; a->delay = delay;
        if (U.scene == SC_BATTLE && U.mode == M_DICE && defer) a->deferred = true; else a->t0 = NOW + delay;
        return a;
    }
    static anim_t dummy; return &dummy;
}
static anim_t *anim_for(anim_type_t type, int id) {
    for (int i = 0; i < MAX_ANIMS; i++) { anim_t *a = &U.anims[i]; if (a->on && !a->deferred && a->type == type && a->id == id && NOW >= a->t0 && NOW < a->t0 + a->dur) return a; }
    return NULL;
}
static void flush_deferred(void) {
    for (int i = 0; i < MAX_ANIMS; i++) if (U.anims[i].on && U.anims[i].deferred) { U.anims[i].deferred = false; U.anims[i].t0 = NOW + U.anims[i].delay; }
    U.busy_until = NOW + 700;
}
static float frand(void) { return (float)rand() / (float)RAND_MAX; }
static void burst(float x, float y, int n, const uint16_t *cols, int ncols, float power) {
    for (int k = 0; k < n; k++) for (int i = 0; i < MAX_PARTICLES; i++) if (U.parts[i].life <= 0) {
        float a = frand() * 6.2832f, v = (0.6f + frand()) * power;
        U.parts[i] = (particle_t){x, y, cosf(a) * v, sinf(a) * v - power * 0.6f, 1.0f, cols[k % ncols], 2 + (rand() % 2)};
        break;
    }
}
/* Tablero: casillas de 32 px (arte de 16 px al doble) y una cámara que se mueve sobre el mapa.
 * CAMX/CAMY es el punto del mapa que queda en el centro de la vista. */
#define CELL 40
#define SCENERY 4   /* casillas de paisaje alrededor del tablero (se ven al mover el mapa) */
#define VIEW_CX 180
#define VIEW_CY 184
static int BX = 52, BY = 56;
static float CAMX, CAMY, CAM_TX, CAM_TY;
static int cam_fx = -1, cam_fy = -1; static bool cam_manual;
#define SC(n) ((n) * CELL / 26)
static void cam_clamp(float *x, float *y) {
    float W = U.g.w * CELL, H = U.g.h * CELL;
    float mx = W > 200 ? (W - 200) / 2 + 24 : 24, my = H > 190 ? (H - 190) / 2 + 30 : 30;
    mx += CELL * 2; my += CELL * 2;   /* se puede asomar al paisaje de alrededor */
    if (*x < W / 2 - mx) *x = W / 2 - mx;
    if (*x > W / 2 + mx) *x = W / 2 + mx;
    if (*y < H / 2 - my) *y = H / 2 - my;
    if (*y > H / 2 + my) *y = H / 2 + my;
}
static void cam_origin(void) { BX = (int)lroundf(VIEW_CX - CAMX); BY = (int)lroundf(VIEW_CY - CAMY); }
static void layout_board(int w, int h) { CAMX = CAM_TX = w * CELL / 2.0f; CAMY = CAM_TY = h * CELL / 2.0f; cam_fx = cam_fy = -1; cam_manual = false; cam_origin(); }
/* Que la casilla enfocada quede dentro de la zona segura (lejos de los paneles y del borde redondo) */
static void cam_focus(int x, int y) {
    if (x < 0) return;
    if (x != cam_fx || y != cam_fy) { cam_manual = false; cam_fx = x; cam_fy = y; }
    if (cam_manual) return;
    float wx = x * CELL + CELL / 2.0f, wy = y * CELL + CELL / 2.0f;
    float sx = VIEW_CX + wx - CAM_TX, sy = VIEW_CY + wy - CAM_TY;
    const float L0 = 84, R0 = 276, T0 = 96, B0 = 272;
    if (sx < L0) CAM_TX -= L0 - sx;
    if (sx > R0) CAM_TX += sx - R0;
    if (sy < T0) CAM_TY -= T0 - sy;
    if (sy > B0) CAM_TY += sy - B0;
    cam_clamp(&CAM_TX, &CAM_TY);
}
static void cam_update(float dt) {
    float k = fminf(1.0f, dt * 9.0f);
    CAMX += (CAM_TX - CAMX) * k; CAMY += (CAM_TY - CAMY) * k;
    cam_origin();
}
void ui_drag(int dx, int dy) {
    if (U.scene != SC_BATTLE && U.scene != SC_DEPLOY) return;
    CAM_TX -= dx; CAM_TY -= dy; cam_clamp(&CAM_TX, &CAM_TY); CAMX = CAM_TX; CAMY = CAM_TY; cam_manual = true; cam_origin();
}
void ui_drag_end(void) {}
static float spr_sc(void) { return CELL / 12.0f; }
static void cell_xy(int x, int y, int *px, int *py) { *px = BX + x * CELL; *py = BY + y * CELL; }
static void float_text(int x, int y, const char *t, uint16_t c, uint32_t dur, int delay, bool defer) {
    anim_t *a = add_anim(AN_FLOAT, -1, dur, delay, defer); a->x = x; a->y = y; a->color = c; strncpy(a->text, t, sizeof a->text - 1); a->text[sizeof a->text - 1] = 0;
}

static void pump(void) {
    game_t *g = &U.g;
    static int extra = 0, chain_k = 0; /* el rayo reparte sus golpes uno tras otro */
    while (U.last_seq < g->seq) {
        const event_t *e = game_event(g, ++U.last_seq);
        if (!e) continue;
        if (U.vs) brain_observe(g, e, BIDDY, U.lvl);   /* momentos para la futura personalidad */
        char b[64];
        switch (e->type) {
        case EV_MOVE: {
            if (e->kind == 2) { /* Excavar: polvo donde se hunde, y sale de la tierra en el destino */
                anim_t *b1 = add_anim(AN_BURST, -1, 1, 0, false); b1->x = e->fx; b1->y = e->fy; b1->p = -1;
                add_anim(AN_POP, e->unit_id, 320, 280, false);
                anim_t *b2 = add_anim(AN_BURST, -1, 1, 280, false); b2->x = e->x; b2->y = e->y; b2->p = -1;
                break;
            }
            bool pushed = e->kind == 5;
            anim_t *a = add_anim(AN_MOVE, e->unit_id, e->kind == 4 ? 260 : e->kind == 3 ? 460 : pushed ? 260 : e->slid ? 600 : 440, pushed ? 300 : 0, pushed);
            a->fx = e->fx; a->fy = e->fy; a->x = e->x; a->y = e->y; a->p = e->kind;
            if (e->kind == 4) { anim_t *b = add_anim(AN_BURST, -1, 1, 0, false); b->x = e->fx; b->y = e->fy; b->p = -1; }
            break; }
        case EV_HIDE: float_text(e->x, e->y, T->hidden_lbl, RGB(0xcf, 0xc6, 0xb8), 1100, 0, false); { anim_t *b = add_anim(AN_BURST, -1, 1, 0, false); b->x = e->x; b->y = e->y; b->p = -2; } break;
        case EV_REVEAL: float_text(e->x, e->y, T->f_reveal, C_GOLD, 1100, 0, true); break;
        case EV_WALL: float_text(e->x, e->y, T->f_wall, RGB(0xcf, 0xc6, 0xb8), 1100, 0, false); break;
        case EV_DEPLOY: add_anim(AN_POP, e->unit_id, 300, 0, false); break;
        case EV_ATTACK: extra = 0; chain_k = 0; break;
        case EV_CHAIN: { extra = chain_k * 230; chain_k++; anim_t *a = add_anim(AN_BOLT, -1, 320, extra, true); a->fx = e->fx; a->fy = e->fy; a->x = e->x; a->y = e->y; break; }
        case EV_DAMAGE:
            if (e->by >= 0) U.dmg[e->by] += e->amount;
            if (e->attacker_id >= 0 && !chain_k) { anim_t *a = add_anim(AN_LUNGE, e->attacker_id, 260, 0, true); a->x = e->x; a->y = e->y; }
            add_anim(AN_HURT, e->unit_id, 380, 140 + extra, true);
            float_text(e->x, e->y, FMT(b, "-%d", e->amount), C_BAD, 1000, 140 + extra, true);
            add_anim(AN_HAPTIC, HAP_HIT, 1, 140 + extra, true);
            break;
        case EV_BLOCK: float_text(e->x, e->y, "0", C_DIM, 900, extra, true); break;
        case EV_DEATH: { U.kills[e->by >= 0 ? e->by : 1 - e->player]++; anim_t *a = add_anim(AN_DEATH, e->unit_id, 700, 420 + extra, true); a->cid = e->cid; a->p = e->player; a->x = e->x; a->y = e->y; break; }
        case EV_SKILLROLL: U.rolls++; if (e->quality == Q_CRIT) U.crits[e->player]++; break;
        case EV_DMGROLL: U.rolls += e->nrolls; break;
        case EV_MANA: float_text(e->x, e->y, FMT(b, T->f_mana, e->amount), C_MANA, 1100, 0, false); break;
        case EV_CAPTURE: float_text(e->x, e->y, T->f_captured, TEAM[e->player].l, 1300, 0, false); break;
        case EV_CONTESTED: float_text(e->x, e->y, T->f_contested, C_BAD, 1300, 0, false); break;
        case EV_BUFF: float_text(e->x, e->y, T->f_atk, C_GOOD, 1000, 0, false); break;
        case EV_POWER: {
            U.banner.on = true; U.banner.lng = true; U.banner.team = e->player; U.banner.t0 = NOW;
            snprintf(U.banner.text, sizeof U.banner.text, "%s", L(POWERS[e->power].name));
            for (char *c = U.banner.text; *c; c++) if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
            snprintf(U.banner.sub, sizeof U.banner.sub, "%s", L(POWERS[e->power].text));
            if (e->power == P_HARDEN) float_text(e->x, e->y, FMT(b, "+%d %s +1 DEF", e->healed, T->hp), C_GOOD, 1400, 900, false);
            if (e->power == P_RESUMMON) { add_anim(AN_POP, e->unit_id, 500, 900, false); anim_t *a = add_anim(AN_BURST, -1, 1, 900, false); a->x = e->x; a->y = e->y; a->p = e->player; }
            break; }
        default: break;
        }
    }
}

/* ---------- Partida y modos ---------- */
static void enter_select(void);
static void after_action(void);
static void start_battle(int map) {
    game_new(&U.g, map, &U.armies[0], &U.armies[1]); U.has_game = true; layout_board(U.g.w, U.g.h);
    U.last_seq = 0; memset(U.anims, 0, sizeof U.anims); memset(U.parts, 0, sizeof U.parts);
    U.dmg[0] = U.dmg[1] = U.kills[0] = U.kills[1] = U.rolls = U.crits[0] = U.crits[1] = 0;
    U.ai.state = AI_IDLE; U.ai.at = 0; U.recorded = false; brain_cancel();
    pump(); U.deploy_idx = 0; go(SC_DEPLOY);
}
static void after_deploy(void) {
    pump();
    if (U.g.phase == PH_BATTLE) { go(SC_BATTLE); U.last_current = -1; U.last_round = 0; U.sel_idx = 0; enter_select(); }
    else { target_t c[MAX_TARGETS]; int n = deploy_cells(&U.g, c); if (U.deploy_idx >= n) U.deploy_idx = n - 1; if (U.deploy_idx < 0) U.deploy_idx = 0; }
}
static int sel_list(int *ids) { return selectable_units(&U.g, ids); }
static void to_victory(void) {
    if (U.scene == SC_VICTORY) return;
    if (U.vs && !U.recorded) { U.recorded = true; brain_record_result(U.lvl, U.g.winner == 1 - BIDDY); }
    haptic(HAP_WIN); go(SC_VICTORY);
}
static void enter_select(void) {
    game_t *g = &U.g;
    if (g->phase == PH_OVER) { to_victory(); return; }
    U.mode = M_SELECT;
    if (g->current != U.last_current || g->round != U.last_round) {
        U.banner.on = true; U.banner.lng = false; U.banner.team = g->current; U.banner.t0 = NOW;
        snprintf(U.banner.text, sizeof U.banner.text, T->turn, tname(g->current));
        if (g->round != U.last_round) snprintf(U.banner.sub, sizeof U.banner.sub, T->round_of, g->round, RULES.max_rounds); else U.banner.sub[0] = 0;
        U.busy_until = NOW + 500;
        U.last_current = g->current; U.last_round = g->round; U.sel_idx = 0;
    }
    int ids[MAX_UNITS]; int n = sel_list(ids);
    if (U.sel_idx > n) U.sel_idx = n;
    if (U.sel_idx < 0) U.sel_idx = 0;
}
static void enter_action(void) {
    U.mode = M_ACTION;
    U.nact = actions_for(&U.g, U.actions);
    if (U.act_idx >= U.nact || !U.actions[U.act_idx].enabled) {
        U.act_idx = U.nact - 1;
        for (int i = 0; i < U.nact; i++) if (U.actions[i].enabled) { U.act_idx = i; break; }
    }
}
static void after_action(void) {
    pump();
    if (U.g.phase == PH_OVER) { U.victory_at = NOW + 900; U.mode = M_BUSY; return; }
    if (U.g.active_id < 0) enter_select(); else enter_action();
}
static void sort_around(int cx, int cy, target_t *t, int n) {
    for (int i = 1; i < n; i++) for (int j = i; j > 0; j--) {
        float a = fmodf(atan2f(t[j - 1].y - cy, t[j - 1].x - cx) + 3.14159f * 2.5f, 6.28318f);
        float b = fmodf(atan2f(t[j].y - cy, t[j].x - cx) + 3.14159f * 2.5f, 6.28318f);
        int da = abs(t[j - 1].x - cx) + abs(t[j - 1].y - cy), db = abs(t[j].x - cx) + abs(t[j].y - cy);
        if (a > b + 1e-4f || (fabsf(a - b) <= 1e-4f && da > db)) { target_t k = t[j]; t[j] = t[j - 1]; t[j - 1] = k; } else break;
    }
}
static void open_powers(void) {
    powers_for(&U.g, U.powers);
    if (!U.powers[U.pow_idx].enabled) { U.pow_idx = 0; for (int i = 0; i < P_COUNT; i++) if (U.powers[i].enabled) { U.pow_idx = i; break; } }
    U.mode = M_POWER;
}
static void power_done(bool ok, const char *reason) {
    if (!ok) { haptic(HAP_NO); flash_msg(reason); return; }
    haptic(HAP_MAGIC); pump(); U.busy_until = NOW + 900; U.sel_idx = 0; U.mode = M_SELECT;
}

/* ---------- Dados ---------- */
/* La fuerza y el giro solo cambian la animación: el número lo decide d6_hw() al soltar. */
#define CHARGE_LIMIT 10000
#define DIE_SIZE 52
#define PI_F 3.14159265f
static float dice_x(int i) { return U.dice.n > 1 ? CX - 38 + 76 * i : CX; }
static void start_dice(int stage) {
    U.mode = M_DICE;
    memset(&U.dice, 0, sizeof U.dice);
    U.dice.stage = stage; U.dice.phase = 0; U.dice.t0 = U.dice.last_t = NOW; U.dice.n = stage == 0 ? 1 : U.pending.damage_dice;
    for (int i = 0; i < 2; i++) U.dice.ang[i] = (frand() - 0.5f) * 0.4f;
}
static void dice_click(int dir) {
    /* Cada clic de la perilla empuja el giro y carga el lanzamiento */
    U.dice.spin += dir * 2.4f; if (U.dice.spin > 26) U.dice.spin = 26; if (U.dice.spin < -26) U.dice.spin = -26;
    U.dice.intensity += 0.07f; U.dice.click_t = NOW;
    uint16_t cols[3] = {C_GOLD, TEAM[U.g.current].l, 0xFFFF};
    for (int i = 0; i < U.dice.n; i++) {
        float a = frand() * 2 * PI_F;
        burst(dice_x(i) + cosf(a) * 32, CY + 4 + sinf(a) * 32, 2 + (int)(U.dice.intensity * 3), cols, 3, 1.0f + U.dice.intensity * 1.8f);
    }
    if (U.dice.intensity >= 1) {
        U.dice.intensity = 1;
        if (!U.dice.maxed) {
            U.dice.maxed = true; U.dice.max_t = NOW; U.shake_t0 = NOW; U.shake_mag = 4;
            uint16_t c2[3] = {C_BAD, C_GOLD, 0xFFFF}; burst(CX, CY + 4, 36, c2, 3, 3.5f); haptic(HAP_MAGIC);
        }
    }
}
static void release_dice(void) {
    if (U.dice.intensity < 0.12f) U.dice.intensity = 0.12f;
    float k = U.dice.intensity;
    for (int i = 0; i < U.dice.n; i++) {
        U.dice.values[i] = d6_hw(); U.dice.faces[i] = 1 + rand() % 6;
        float dir = U.dice.spin < 0 ? -1.0f : 1.0f; if (i) dir = -dir;
        float omega = (7 + k * 16 + fabsf(U.dice.spin) * 0.7f) * dir;
        U.dice.ang_start[i] = U.dice.ang[i];
        U.dice.ang_end[i] = roundf((U.dice.ang[i] + omega) / (PI_F / 2)) * (PI_F / 2);
    }
    U.dice.phase = 1; U.dice.roll_t0 = NOW; U.dice.roll_dur = (uint32_t)(950 + k * 1300);
    U.dice.bounces = 2 + (int)lroundf(k * 3); U.dice.height = 50 + k * 50; U.dice.last_face = NOW; U.dice.hop = 0; U.dice.ntrail = 0;
    haptic(HAP_TICK);
}
/* Saltos que se acortan: devuelve la altura en este instante y en qué salto va */
static float hop_height(float pr, int *hop) {
    float tot = 0, d[8]; int n = U.dice.bounces; if (n > 8) n = 8;
    for (int k = 0; k < n; k++) { d[k] = powf(0.62f, (float)k); tot += d[k]; }
    float t = pr * tot;
    for (int k = 0; k < n; k++) {
        if (t <= d[k] || k == n - 1) { float u = fminf(1, t / d[k]); *hop = k; return 4 * U.dice.height * powf(0.42f, (float)k) * u * (1 - u); }
        t -= d[k];
    }
    *hop = n - 1; return 0;
}
static void land_dice(void) {
    game_t *g = &U.g;
    U.dice.phase = 2; U.dice.result_t = NOW; U.dice.page = 0;
    U.shake_t0 = NOW; U.shake_mag = 1.5f + U.dice.intensity * 5;
    uint16_t cols[3] = {C_GOLD, C_INK, TEAM[g->current].l};
    for (int i = 0; i < U.dice.n; i++) burst(dice_x(i), CY + 4 + DIE_SIZE / 2, 8 + (int)(U.dice.intensity * 26), cols, 3, 1.5f + U.dice.intensity * 3);
    haptic(HAP_THUD);
    if (U.dice.stage == 0) {
        resolve_skill(g, &U.pending, U.dice.values[0]);
        if (U.pending.quality == Q_CRIT) { uint16_t c2[2] = {C_GOLD, 0xFFFF}; burst(CX, CY, 40, c2, 2, 4.5f); U.dice.flash = 1; U.dice.flash_t = NOW; haptic(HAP_WIN); }
        if (U.pending.quality == Q_CRITFAIL) { U.dice.flash = 2; U.dice.flash_t = NOW; haptic(HAP_NO); }
    } else {
        resolve_damage(g, &U.pending, U.dice.values);
        const unit_t *t = unit_by_id_c(g, U.pending.target_id);
        if (t && !t->alive) { U.dice.flash = 3; U.dice.flash_t = NOW; }
    }
    pump();
}
static void continue_dice(void) {
    if (U.dice.stage == 0 && U.pending.stage == 1) { start_dice(1); return; }
    U.mode = M_BUSY; flush_deferred();
    const unit_t *t = unit_by_id_c(&U.g, U.pending.target_id);
    U.after_action_at = NOW + (t && !t->alive ? 1250 : U.pending.nchain ? 1100 : 850); /* tiempo para ver el golpe */
}

/* ---------- Entrada ---------- */
static void start_cut(const unit_t *u, const unit_t *t, int kind, int x, int y, int target);
/* Ejecuta el objetivo elegido (lo usan el jugador y Biddy). false si el motor no lo acepta. */
static bool confirm_target(void) {
    game_t *g = &U.g; target_t t = U.targets[U.tgt_idx]; unit_t *u = active_unit(g);
    if (!u) return false;
    if (U.action_id == A_MOVE || U.action_id == A_RETREAT) {
        /* dejamos ver el paso antes de volver al menú */
        do_move(g, t.x, t.y, U.action_id == A_RETREAT); haptic(HAP_THUD); pump(); U.mode = M_BUSY; U.after_action_at = NOW + 560; return true;
    }
    if (U.action_id == A_ABILITY) { start_cut(u, t.unit_id >= 0 ? unit_by_id_c(g, t.unit_id) : NULL, special_is_attack(u->cid) ? 2 : 1, t.x, t.y, t.unit_id); return true; }
    if (!begin_attack(g, U.action_id, t.unit_id, &U.pending)) return false;
    pump(); start_dice(0);
    return true;
}

/* ---------- Corte de especial: retrato grande en diagonal antes de que ocurra ---------- */
#define CUT_MS 1400
static void start_cut(const unit_t *u, const unit_t *t, int kind, int x, int y, int target) {
    U.cut.on = true; U.cut.t0 = NOW; U.cut.cid = u->cid; U.cut.p = u->player; U.cut.kind = kind; U.cut.x = x; U.cut.y = y; U.cut.target = target;
    U.cut.has_t = t != NULL; if (t) { U.cut.tcid = t->cid; U.cut.tp = t->player; }
    U.cut.act = U.action_id; U.mode = M_BUSY; haptic(HAP_MAGIC);
}
static void finish_cut(void) {
    game_t *g = &U.g; U.cut.on = false;
    if (U.cut.kind == 0) { do_special(g, 0, 0); pump(); U.after_action_at = NOW + 700; return; }
    if (U.cut.kind == 3) { enter_select(); return; }
    if (U.cut.kind == 1) { do_special(g, U.cut.x, U.cut.y); haptic(HAP_THUD); pump(); U.after_action_at = NOW + 760; return; }
    if (!begin_attack(g, A_ABILITY, U.cut.target, &U.pending)) { after_action(); return; }
    pump(); start_dice(0);
}
/* ---------- Biddy juega ----------
 * Biddy usa la misma interfaz que un jugador: elige criatura, se ve qué acción toma y a quién
 * apunta, y lanza los dados con la perilla "virtual". Piensa en segundo plano (brain_request). */
static action_id_t step_action(ai_step_type_t t) {
    switch (t) {
    case AS_MOVE: return A_MOVE; case AS_RETREAT: return A_RETREAT; case AS_ATTACK: return A_ATTACK;
    case AS_ABILITY: return A_ABILITY; case AS_DEFEND: return A_DEFEND; default: return A_END;
    }
}
static int AI_FALLBACKS;
static void ai_fallback(void) {
    /* nunca se queda atorado: si algo no cuadra, termina la activación */
    game_t *g = &U.g; U.ai.state = AI_IDLE; AI_FALLBACKS++;
    if (g->active_id >= 0) end_activation(g);
    else { int ids[MAX_UNITS]; int n = sel_list(ids); if (n && activate(g, ids[0])) end_activation(g); }
    after_action();
}
static void ai_execute(void) {
    game_t *g = &U.g; brain_decision_t *d = &U.ai.d;
    U.ai.state = AI_IDLE;
    switch (d->kind) {
    case BD_POWER: {
        const char *r = "";
        if (g->active_id >= 0) break;
        if (use_power(g, d->power.power, d->power.unit_id, d->power.x, d->power.y, &r)) power_done(true, r); else ai_fallback();
        break; }
    case BD_ACTIVATE: {
        if (g->active_id >= 0) break;   /* ya hay una activa: vuelve a pensar */
        int ids[MAX_UNITS]; int n = sel_list(ids);
        for (int i = 0; i < n; i++) if (ids[i] == d->unit_id) U.sel_idx = i;
        if (!activate(g, d->unit_id)) { ai_fallback(); break; }
        pump(); U.act_idx = 0; U.ai.steps = 0; enter_action(); U.busy_until = NOW + 300;
        break; }
    case BD_STEP: {
        if (g->active_id < 0 || d->unit_id != g->active_id) break;
        if (++U.ai.steps > 8) { end_activation(g); after_action(); break; }
        enter_action();
        action_id_t a = step_action(d->step.type);
        for (int i = 0; i < U.nact; i++) if (U.actions[i].id == a) U.act_idx = i;
        U.ai.state = AI_SHOW_ACTION; U.ai.at = NOW + 450;
        break; }
    default: ai_fallback(); break;
    }
}
static void ai_show_target(void) {
    game_t *g = &U.g; ai_step_t st = U.ai.d.step; unit_t *u = active_unit(g);
    U.ai.state = AI_IDLE;
    if (!u) { after_action(); return; }
    switch (st.type) {
    case AS_END: end_activation(g); after_action(); return;
    case AS_DEFEND: if (!do_defend(g)) end_activation(g); after_action(); return;
    case AS_ABILITY: if (special_is_instant(u->cid)) { U.action_id = A_ABILITY; start_cut(u, NULL, 0, 0, 0, -1); return; } break;
    default: break;
    }
    action_id_t a = step_action(st.type);
    U.action_id = a; U.ntargets = targets_for(g, a, U.targets); sort_around(u->x, u->y, U.targets, U.ntargets);
    int k = -1;
    for (int i = 0; i < U.ntargets && k < 0; i++)
        if (st.target >= 0 ? U.targets[i].unit_id == st.target : (U.targets[i].x == st.x && U.targets[i].y == st.y)) k = i;
    if (k < 0) { ai_fallback(); return; }
    U.tgt_idx = k; U.mode = M_TARGET; U.ai.state = AI_SHOW_TARGET; U.ai.at = NOW + 550;
}
static void ai_dice(void) {
    if (U.dice.phase == 0 && NOW - U.dice.t0 > 450) {
        /* carga el dado con unos cuantos "clics" y lo suelta: solo cambia la animación */
        if (!U.ai.want) { U.ai.want = 4 + rand() % 7; U.ai.clicks = 0; U.ai.at = NOW; }
        if (NOW >= U.ai.at) {
            if (U.ai.clicks < U.ai.want) { dice_click(rand() % 3 ? 1 : -1); U.ai.clicks++; U.ai.at = NOW + 80; }
            else { U.ai.want = 0; release_dice(); }
        }
    } else if (U.dice.phase == 2 && NOW - U.dice.result_t > 1500) continue_dice();
}
static void ai_drive(void) {
    game_t *g = &U.g;
    if (U.scene == SC_DEPLOY && U.vs && g->phase == PH_DEPLOY) {
        if (g->deploy_q[g->dq_head].player == BIDDY && NOW >= U.ai.at) {
            target_t c[MAX_TARGETS]; int n = deploy_cells(g, c);
            if (n) { int i = brain_deploy_index(g, U.lvl); U.deploy_idx = i; place_next(g, c[i].x, c[i].y); }
            else auto_place_rest(g);
            after_deploy(); U.ai.at = NOW + 380;
        }
        return;
    }
    if (!ai_turn() || U.cut.on || U.after_action_at || U.victory_at || NOW < U.busy_until) return;
    if (U.mode == M_DICE) { ai_dice(); return; }
    if (U.mode != M_SELECT && U.mode != M_ACTION && U.mode != M_TARGET) return;
    switch (U.ai.state) {
    case AI_IDLE:
        if (U.mode == M_TARGET) { U.mode = g->active_id >= 0 ? M_ACTION : M_SELECT; }
        if (brain_request(g, U.lvl)) { U.ai.state = AI_THINK; U.ai.t0 = NOW; }
        break;
    case AI_THINK: {
        if (brain_busy()) break;
        uint32_t min = g->active_id < 0 ? 650 : 250;   /* que se alcance a ver qué hace */
        if (NOW - U.ai.t0 < min) break;
        if (brain_poll(&U.ai.d)) ai_execute(); else U.ai.state = AI_IDLE;
        break; }
    case AI_SHOW_ACTION: if (NOW >= U.ai.at) ai_show_target(); break;
    case AI_SHOW_TARGET: if (NOW >= U.ai.at) { U.ai.state = AI_IDLE; if (!confirm_target()) ai_fallback(); } break;
    }
}

static void in_battle(input_t ev);
void ui_input(input_t ev) {
    if (ev == IN_L || ev == IN_R) haptic(HAP_TICK);
    bool dice = U.scene == SC_BATTLE && U.mode == M_DICE;
    if (NOW < U.busy_until && !dice) return;
    switch (U.scene) {
    case SC_SPLASH: if (ev == IN_TAP) { haptic(HAP_OK); go(SC_MENU); } break;
    case SC_MENU:
        if (ev == IN_L || ev == IN_R) U.menu_idx = stepi(U.menu_idx, 5, ev);
        if (ev == IN_TAP) {
            haptic(HAP_OK);
            if (U.menu_idx == 0) { U.lvl_idx = brain_level(); go(SC_LEVEL); }
            else if (U.menu_idx == 1) { U.vs = false; memset(U.armies, 0, sizeof U.armies); U.b_player = 0; U.b_idx = 0; U.priv_key = 0; U.priv_color = 0; go(SC_PRIVACY); }
            else if (U.menu_idx == 2) { U.rules_page = 0; go(SC_RULES); }
            else if (U.menu_idx == 3) { U.opt_idx = 0; go(SC_OPTIONS); }
            else hal_exit();
        }
        if (ev == IN_LONG) go(SC_SPLASH);
        break;
    case SC_LEVEL:
        if (ev == IN_L || ev == IN_R) U.lvl_idx = stepi(U.lvl_idx, LVL_COUNT, ev);
        if (ev == IN_TAP) {
            haptic(HAP_OK);
            U.vs = true; U.lvl = (level_t)U.lvl_idx; brain_set_level(U.lvl);
            memset(U.armies, 0, sizeof U.armies); U.b_player = 0; U.b_idx = 0; go(SC_BUILDER);
        }
        if (ev == IN_LONG) go(SC_MENU);
        break;
    case SC_OPTIONS:
        if (ev == IN_L || ev == IN_R) U.opt_idx = stepi(U.opt_idx, 3, ev);
        if (ev == IN_TAP) {
            if (U.opt_idx == 0) { set_lang(eng_lang() == LANG_ES ? LANG_EN : LANG_ES); hal_save_settings(eng_lang(), U.haptic_on); haptic(HAP_OK); }
            else if (U.opt_idx == 1) { U.haptic_on = !U.haptic_on; hal_save_settings(eng_lang(), U.haptic_on); haptic(HAP_OK); }
            else { haptic(HAP_OK); go(SC_MENU); }
        }
        if (ev == IN_LONG) go(SC_MENU);
        break;
    case SC_RULES:
        if (ev == IN_L && U.rules_page > 0) U.rules_page--;
        if (ev == IN_R && U.rules_page < 6) U.rules_page++;
        if (ev == IN_TAP) { if (U.rules_page < 6) U.rules_page++; else go(SC_MENU); }
        if (ev == IN_LONG) go(SC_MENU);
        break;
    case SC_PRIVACY:
        if (ev == IN_TAP && NOW - U.t0 > 400) {
            haptic(HAP_OK);
            if (U.priv_key == 0) go(SC_BUILDER);
            else if (U.priv_key == 1) { U.b_player = 1; U.b_idx = 0; go(SC_BUILDER); }
            else go(SC_REVEAL);
        }
        break;
    case SC_BUILDER: {
        army_t *army = &U.armies[U.b_player]; char b[64];
        if (ev == IN_L || ev == IN_R) U.b_idx = stepi(U.b_idx, C_COUNT + 1, ev);
        if (ev == IN_TAP) {
            if (U.b_idx == C_COUNT) {
                if (!army->n) { haptic(HAP_NO); flash_msg(T->need_one); break; }
                haptic(HAP_OK);
                if (U.vs) { U.doctrine = brain_pick_army(U.lvl, &U.armies[BIDDY]); go(SC_REVEAL); break; }  /* Biddy arma el suyo */
                U.priv_key = U.b_player == 0 ? 1 : 2; U.priv_color = U.b_player == 0 ? 1 : -1; go(SC_PRIVACY);
                break;
            }
            add_result_t r = army_add(army, U.b_idx);
            if (r == ADD_OK) { haptic(HAP_OK); U.add_flash = NOW; flash_msg(FMT(b, T->joins, cname(U.b_idx))); }
            else { haptic(HAP_NO); flash_msg(add_reason(r)); }
        }
        if (ev == IN_LONG) {
            if (army->n) { army->n--; haptic(HAP_NO); flash_msg(FMT(b, T->removed, cname(army->cid[army->n]))); }
            else if (U.b_player == 0) go(U.vs ? SC_LEVEL : SC_MENU);
        }
        break; }
    case SC_REVEAL: if (ev == IN_TAP && NOW - U.t0 > 1400) { haptic(HAP_OK); U.map_idx = 0; go(SC_MAP); } break;
    case SC_MAP:
        if (ev == IN_L || ev == IN_R) U.map_idx = stepi(U.map_idx, MAP_COUNT, ev);
        if (ev == IN_TAP) { haptic(HAP_OK); start_battle(U.map_idx); }
        break;
    case SC_DEPLOY: {
        target_t c[MAX_TARGETS]; int n = deploy_cells(&U.g, c);
        bool biddy_places = U.vs && U.g.phase == PH_DEPLOY && U.g.deploy_q[U.g.dq_head].player == BIDDY;
        if (ev == IN_LONG && U.vs) {
            /* contra Biddy: tus criaturas van a buenas casillas y Biddy coloca las suyas como siempre */
            while (U.g.phase == PH_DEPLOY) {
                int nn = deploy_cells(&U.g, c); if (!nn) { auto_place_rest(&U.g); break; }
                bool mine = U.g.deploy_q[U.g.dq_head].player != BIDDY;
                int i = brain_deploy_index(&U.g, mine ? LVL_HARD : U.lvl);
                place_next(&U.g, c[i].x, c[i].y);
            }
            haptic(HAP_OK); after_deploy(); break;
        }
        if (biddy_places) break;
        if (ev == IN_L || ev == IN_R) U.deploy_idx = stepi(U.deploy_idx, n, ev);
        if (ev == IN_TAP && n) { place_next(&U.g, c[U.deploy_idx].x, c[U.deploy_idx].y); haptic(HAP_THUD); after_deploy(); }
        if (ev == IN_LONG) { auto_place_rest(&U.g); haptic(HAP_OK); after_deploy(); }
        break; }
    case SC_BATTLE: in_battle(ev); break;
    case SC_VICTORY: if (ev == IN_TAP && NOW - U.t0 > 1500) { haptic(HAP_OK); U.has_game = false; go(SC_MENU); } break;
    }
}
static bool confirm_target(void);
static void in_battle(input_t ev) {
    game_t *g = &U.g; const char *reason = "";
    if (ai_turn() && U.mode != M_PAUSE) {
        /* turno de Biddy: solo se puede adelantar el resultado de los dados o pausar */
        if (U.mode == M_DICE && U.dice.phase == 2 && ev == IN_TAP && NOW - U.dice.result_t > 450) { continue_dice(); return; }
        if (ev == IN_LONG && (U.mode == M_SELECT || U.mode == M_ACTION) && (U.ai.state == AI_IDLE || U.ai.state == AI_THINK)) { U.mode = M_PAUSE; U.pause_idx = 0; }
        return;
    }
    switch (U.mode) {
    case M_SELECT: {
        int ids[MAX_UNITS]; int n = sel_list(ids);
        if (ev == IN_L || ev == IN_R) U.sel_idx = stepi(U.sel_idx, n + 1, ev);
        if (ev == IN_TAP) {
            if (U.sel_idx >= n) { haptic(HAP_OK); open_powers(); break; }
            activate(g, ids[U.sel_idx]); pump(); haptic(HAP_OK); U.act_idx = 0; enter_action();
        }
        if (ev == IN_LONG) { U.mode = M_PAUSE; U.pause_idx = 0; }
        break; }
    case M_POWER: {
        if (ev == IN_L || ev == IN_R) U.pow_idx = stepi(U.pow_idx, P_COUNT, ev);
        if (ev == IN_LONG) { U.mode = M_SELECT; break; }
        if (ev != IN_TAP) break;
        power_state_t *p = &U.powers[U.pow_idx];
        if (!p->enabled) { haptic(HAP_NO); flash_msg(p->reason); break; }
        if (p->id == P_HARDEN) {
            U.ntargets = power_targets(g, P_HARDEN, U.targets);
            for (int i = 1; i < U.ntargets; i++) for (int j = i; j > 0 && (U.targets[j - 1].y * MAX_W + U.targets[j - 1].x) > (U.targets[j].y * MAX_W + U.targets[j].x); j--) { target_t k = U.targets[j]; U.targets[j] = U.targets[j - 1]; U.targets[j - 1] = k; }
            U.tgt_idx = 0; U.mode = M_PTARGET; break;
        }
        if (p->id == P_RESUMMON) {
            U.nfallen = power_targets(g, P_RESUMMON, U.fallen); U.fallen_idx = 0;
            for (int i = 0; i < U.nfallen; i++) if (U.fallen[i].affordable) { U.fallen_idx = i; break; }
            U.mode = M_PFALLEN; break;
        }
        bool ok = use_power(g, p->id, -1, 0, 0, &reason); power_done(ok, reason);
        break; }
    case M_PTARGET:
        if (ev == IN_L || ev == IN_R) U.tgt_idx = stepi(U.tgt_idx, U.ntargets, ev);
        if (ev == IN_LONG) U.mode = M_POWER;
        if (ev == IN_TAP) { bool ok = use_power(g, P_HARDEN, U.targets[U.tgt_idx].unit_id, 0, 0, &reason); power_done(ok, reason); }
        break;
    case M_PFALLEN:
        if (ev == IN_L || ev == IN_R) U.fallen_idx = stepi(U.fallen_idx, U.nfallen, ev);
        if (ev == IN_LONG) U.mode = M_POWER;
        if (ev == IN_TAP) {
            target_t f = U.fallen[U.fallen_idx]; char b[48];
            if (!f.affordable) { haptic(HAP_NO); flash_msg(FMT(b, T->mana_cost, f.cost)); break; }
            U.fallen_pick = f; U.ntargets = resummon_cells(g, U.targets); U.tgt_idx = U.ntargets / 2; U.mode = M_PCELL; haptic(HAP_TICK);
        }
        break;
    case M_PCELL:
        if (ev == IN_L || ev == IN_R) U.tgt_idx = stepi(U.tgt_idx, U.ntargets, ev);
        if (ev == IN_LONG) U.mode = M_PFALLEN;
        if (ev == IN_TAP) { target_t c = U.targets[U.tgt_idx]; bool ok = use_power(g, P_RESUMMON, U.fallen_pick.unit_id, c.x, c.y, &reason); power_done(ok, reason); }
        break;
    case M_PAUSE:
        if (ev == IN_L || ev == IN_R) U.pause_idx = stepi(U.pause_idx, 2, ev);
        if (ev == IN_TAP) { if (U.pause_idx == 0) { if (g->active_id >= 0) enter_action(); else U.mode = M_SELECT; } else { brain_cancel(); U.ai.state = AI_IDLE; U.has_game = false; go(SC_MENU); } }
        if (ev == IN_LONG) { if (g->active_id >= 0) enter_action(); else U.mode = M_SELECT; }
        break;
    case M_ACTION:
        if (ev == IN_L || ev == IN_R) U.act_idx = stepi(U.act_idx, U.nact, ev);
        if (ev == IN_TAP) {
            action_t *a = &U.actions[U.act_idx];
            if (!a->enabled) { haptic(HAP_NO); flash_msg(a->reason); break; }
            if (a->id == A_END) { end_activation(g); haptic(HAP_OK); after_action(); break; }
            if (a->id == A_DEFEND) { do_defend(g); haptic(HAP_OK); after_action(); break; }
            if (a->id == A_INSPECT) { U.inspect_id = g->active_id; U.mode = M_INSPECT; break; }
            if (a->id == A_ABILITY && special_is_instant(active_unit(g)->cid)) { start_cut(active_unit(g), NULL, 0, 0, 0, -1); break; }
            U.action_id = a->id;
            U.ntargets = targets_for(g, a->id, U.targets);
            unit_t *u = active_unit(g); sort_around(u->x, u->y, U.targets, U.ntargets);
            U.tgt_idx = 0; U.mode = M_TARGET; haptic(HAP_TICK);
        }
        if (ev == IN_LONG) { haptic(HAP_NO); if (cancel_activation(g)) enter_select(); else flash_msg(eng_lang() == LANG_EN ? "Already acted: choose End" : "Ya actuó: elige Terminar"); }
        break;
    case M_TARGET: {
        if (ev == IN_L || ev == IN_R) U.tgt_idx = stepi(U.tgt_idx, U.ntargets, ev);
        if (ev == IN_LONG) { U.mode = M_ACTION; break; }
        if (ev != IN_TAP) break;
        if (!confirm_target()) haptic(HAP_NO);
        break; }
    case M_INSPECT: if (ev == IN_TAP || ev == IN_LONG) U.mode = g->active_id < 0 ? M_SELECT : M_ACTION; break;
    case M_DICE:
        if (U.dice.phase == 0) {
            if (ev == IN_L || ev == IN_R) dice_click(ev == IN_R ? 1 : -1);
            if (ev == IN_TAP) release_dice();
        } else if (U.dice.phase == 2) {
            /* Resultado: girar o mantener muestra el desglose; tocar sigue */
            if ((ev == IN_L || ev == IN_R || ev == IN_LONG) && NOW - U.dice.result_t > 300) U.dice.page ^= 1;
            if (ev == IN_TAP && NOW - U.dice.result_t > 450) continue_dice();
        }
        break;
    case M_BUSY: break;
    }
}
void ui_holding(bool holding, uint32_t since) { U.holding = holding; U.hold_start = since; }

/* ---------- Tablero ---------- */
/* Paisaje fuera del tablero: bosque, montaña o pasto según un hash fijo de la casilla */
static uint32_t hash2(int x, int y) { uint32_t v = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u; v = (v ^ (v >> 13)) * 1274126177u; return v ^ (v >> 16); }
static char tile_at(int x, int y) {
    if (x >= 0 && y >= 0 && x < U.g.w && y < U.g.h) return U.g.tiles[y][x];
    uint32_t r = hash2(x + 50, y + 50) % 100;
    return r < 30 ? 'T' : r < 40 ? 'R' : '.';
}
static void art_at(int id, int px, int py, int pal) { const art_t *t = &ART_TILE[id]; gfx_art(t->px, t->w, t->h, px, py, 2, false, TPAL[pal], 255, 0, 0); }
static void draw_tile(char ch, int x, int y) {
    int px, py; cell_xy(x, y, &px, &py);
    if (px <= -CELL || py <= -CELL || px >= SCR || py >= SCR) return;
    int id = TILE_GRASS0 + (int)(hash2(x, y) % 4), pal = 2;
    switch (ch) {
    case 'T': id = hash2(x, y) % 2 ? TILE_FOREST1 : TILE_FOREST; break; case ':': id = TILE_PATH; break; case 'R': id = TILE_MOUNTAIN; break; case '#': id = TILE_WALL; break;
    case 'u': id = TILE_RUINS; break; case '~': id = TILE_WATER0 + (int)((NOW / 420 + x + y) % 3); break; case 'L': id = TILE_LAVA0 + (int)((NOW / 380 + x) % 2); break;
    case 'I': id = TILE_ICE; break; case 'S': id = TILE_SHRINE; break;
    case 'C': id = TILE_CONTROL; pal = U.g.control_owner[y][x] < 0 ? 2 : U.g.control_owner[y][x]; break;
    }
    art_at(id, px, py, pal);
    /* orillas: agua con arena y espuma, lava con costra, hielo y camino con borde suave */
    if (ch == '~' || ch == 'L' || ch == 'I' || ch == ':') {
        static const int8_t D[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        uint16_t col = ch == '~' ? TPAL[2][ART_IDX_SAND] : ch == 'L' ? RGB(0x3c, 0x28, 0x28) : ch == 'I' ? RGB(0x82, 0xb4, 0xd2) : RGB(0x54, 0x92, 0x3c);
        int th = ch == ':' ? 2 : 3;
        for (int k = 0; k < 4; k++) {
            char n = tile_at(x + D[k][0], y + D[k][1]); if (n == ch) continue;
            if (D[k][1] == -1) gfx_fill(px, py, CELL, th, col);
            if (D[k][1] == 1) gfx_fill(px, py + CELL - th, CELL, th, col);
            if (D[k][0] == -1) gfx_fill(px, py, th, CELL, col);
            if (D[k][0] == 1) gfx_fill(px + CELL - th, py, th, CELL, col);
            if (ch == '~') { /* espuma */
                if (D[k][1] == -1) gfx_fill_a(px, py + th, CELL, 1, 0xFFFF, 150);
                if (D[k][1] == 1) gfx_fill_a(px, py + CELL - th - 1, CELL, 1, 0xFFFF, 150);
                if (D[k][0] == -1) gfx_fill_a(px + th, py, 1, CELL, 0xFFFF, 120);
                if (D[k][0] == 1) gfx_fill_a(px + CELL - th - 1, py, 1, CELL, 0xFFFF, 120);
            }
        }
    }
    if (ch == 'I') { /* destello que cruza el hielo */
        int ph = (int)((NOW / 40 + x * 7 + y * 13) % 90);
        if (ph < 40) for (int k = 0; k < 4; k++) { int d = ph - k; if (d >= 0 && d < 40) gfx_fill_a(px + d, py + 39 - d, 2, 2, 0xFFFF, 190); }
    }
    if (ch == 'C' && U.g.control_owner[y][x] >= 0) gfx_fill_a(px, py, CELL, CELL, TEAM[U.g.control_owner[y][x]].b, 30);
}
/* Torre de maná: columna de piedra con una esfera que flota, late y suelta chispas.
 * Si una criatura está en la torre, la esfera toma el color de su equipo. */
static void draw_tower(int x, int y) {
    int px, py; cell_xy(x, y, &px, &py);
    if (px <= -CELL || py <= -3 * CELL || px >= SCR || py >= SCR + CELL) return;
    const art_t *t = &ART_TILE[TILE_TOWER];
    int tx = px + CELL / 2 - t->w, ty = py + CELL - 4 - t->h * 2;
    gfx_art(t->px, t->w, t->h, tx, ty, 2, false, TPAL[2], 255, 0, 0);
    const unit_t *on = unit_at_c(&U.g, x, y);
    uint16_t glow = on ? TEAM[on->player].l : C_MANA, core = on ? TEAM[on->player].b : RGB(0x9b, 0x6a, 0xf0);
    float tt = NOW / 1000.0f + x * 1.3f + y * 0.7f;
    int cx = px + CELL / 2, cy = ty - 8 + (int)lroundf(sinf(tt * 2.2f) * 2.5f);
    float pulse = 0.5f + 0.5f * sinf(tt * 3.1f);
    gfx_disc(cx, cy, 13 + (int)(pulse * 3), glow, (uint8_t)(40 + 30 * pulse));
    gfx_disc(cx, cy, 9, glow, 110);
    gfx_disc(cx, cy, 7, core, 255);
    gfx_disc(cx - 1, cy - 1, 5, glow, 230);
    gfx_disc(cx - 2, cy - 3, 2, 0xFFFF, 240);
    for (int k = 0; k < 3; k++) { /* chispas en órbita (las de atrás, más tenues) */
        float a = tt * 2.4f + k * 2.094f; float sx = cosf(a) * 13, sy = sinf(a) * 5;
        gfx_fill_a(cx + (int)sx - 1, cy + (int)sy - 1, 3, 3, sy < 0 ? glow : 0xFFFF, sy < 0 ? 120 : 230);
    }
    if (((int)(tt * 3) + x) % 4 == 0) gfx_fill_a(cx + (int)(sinf(tt * 7) * 6), cy - 10 - (int)(fmodf(tt * 20, 10)), 2, 2, 0xFFFF, 180);
}
static void unit_pos(const unit_t *u, int *px, int *py) {
    cell_xy(u->x, u->y, px, py);
    for (int i = 0; i < MAX_ANIMS; i++) {
        anim_t *m = &U.anims[i];
        if (m->on && !m->deferred && m->type == AN_MOVE && m->id == u->id && NOW < m->t0 + m->dur) {
            float p = NOW < m->t0 ? 0 : (float)(NOW - m->t0) / m->dur; if (p > 1) p = 1;
            float e = p * (2 - p); int fx, fy, tx, ty; cell_xy(m->fx, m->fy, &fx, &fy); cell_xy(m->x, m->y, &tx, &ty);
            *px = (int)(fx + (tx - fx) * e); *py = (int)(fy + (ty - fy) * e - sinf(p * 3.14159f) * (m->p == 3 ? 16 : m->p == 4 ? 1 : 4));
        }
    }
    anim_t *l = anim_for(AN_LUNGE, u->id);
    if (l) { float p = (float)(NOW - l->t0) / l->dur, k = sinf(p * 3.14159f) * SC(7); *px += (int)((l->x > u->x) - (l->x < u->x)) * (int)k; *py += (int)((l->y > u->y) - (l->y < u->y)) * (int)k; }
}
static void draw_unit(const unit_t *u) {
    for (int i = 0; i < MAX_ANIMS; i++) { anim_t *a = &U.anims[i]; if (a->on && a->type == AN_POP && a->id == u->id && (a->deferred || NOW < a->t0)) return; }
    int px, py; unit_pos(u, &px, &py);
    bool active = U.g.active_id == u->id, spent = u->activated && U.g.phase == PH_BATTLE;
    anim_t *pop = anim_for(AN_POP, u->id);
    float base = spr_sc(), sc = pop ? base * fminf(1.0f, 0.4f + (float)(NOW - pop->t0) / pop->dur) : base;
    int bob = active ? (int)lroundf(sinf(NOW / 160.0f)) : 0;
    gfx_fill_a(px + 6, py + CELL - 5, CELL - 12, 3, 0, 90); gfx_fill_a(px + 9, py + CELL - 6, CELL - 18, 1, 0, 70);
    gfx_fill_a(px + 5, py + CELL - 3, CELL - 10, 2, TEAM[u->player].b, 160);
    anim_t *hurt = anim_for(AN_HURT, u->id);
    spr_mode_t mode = hurt && ((NOW - hurt->t0) / 70) % 2 == 0 ? SM_FLASH : SM_NORMAL;
    float c2 = CELL / 2.0f, by = SC(24);
    if (active) sprite(u->cid, u->player, px + c2 - 6 * sc, py + bob + (by - 12 * sc), sc, SM_SIL, 140);
    uint8_t al = spent ? 115 : 255;
    if (u->hidden) al = (uint8_t)(60 + 30 * sinf(NOW / 220.0f)); /* Stalker oculto: casi transparente */
    if (u->walling) { uint16_t st = RGB(0x9a, 0x93, 0x8a); gfx_fill(px + 1, py + 1, CELL - 2, 2, st); gfx_fill(px + 1, py + CELL - 3, CELL - 2, 2, st); gfx_fill(px + 1, py + 1, 2, CELL - 2, st); gfx_fill(px + CELL - 3, py + 1, 2, CELL - 2, st); }
    sprite(u->cid, u->player, px + c2 - 6 * sc, py + 1 + bob + (by - 12 * sc), sc, mode, al);
    hp_bar(px + SC(3), py - 1, SC(20), u->hp, u->max_hp, TEAM[u->player].l);
    if (u->defending) { int dx = px + CELL - 8; gfx_fill(dx, py + 4, 7, 8, C_OUT); gfx_fill(dx + 1, py + 5, 5, 5, RGB(0xcf, 0xc6, 0xb8)); gfx_fill(dx + 2, py + 10, 3, 1, RGB(0xcf, 0xc6, 0xb8)); }
    if (u->atk_buff > 0 || u->def_buff > 0) { gfx_fill(px + 2, py + 6, 3, 1, C_GOOD); gfx_fill(px + 3, py + 5, 1, 3, C_GOOD); }
    if (u->streak > 0) { char sb[12]; snprintf(sb, sizeof sb, "x%d", u->streak); gfx_fill_a(px + CELL - 13, py + 2, 12, 9, C_OUT, 200); gfx_text(sb, px + CELL - 7, py + 6, &FONT_XS, C_GOLD, AL_CENTER, false); }
}
static void draw_cursor(int x, int y, uint16_t c) {
    int px, py; cell_xy(x, y, &px, &py);
    int o = (int)lroundf(sinf(NOW / 150.0f) * 1.5f) + 1, s = CELL, Lc = 6;
    int corners[4][4] = {{px - o, py - o, 1, 1}, {px + s + o, py - o, -1, 1}, {px - o, py + s + o, 1, -1}, {px + s + o, py + s + o, -1, -1}};
    for (int i = 0; i < 4; i++) {
        int x0 = corners[i][0], y0 = corners[i][1], dx = corners[i][2], dy = corners[i][3];
        gfx_fill(dx > 0 ? x0 : x0 - Lc, dy > 0 ? y0 : y0 - 2, Lc, 2, c);
        gfx_fill(dx > 0 ? x0 : x0 - 2, dy > 0 ? y0 : y0 - Lc, 2, Lc, c);
    }
}
typedef struct { int x, y; uint16_t c; uint8_t a; } hl_t;
typedef struct { hl_t hl[MAX_TARGETS]; int nhl; bool cursor; int cx, cy; uint16_t cc; bool ghost; int gx, gy, gcid, gp; } board_opts_t;
static void draw_board(const board_opts_t *o) {
    game_t *g = &U.g;
    for (int y = -SCENERY; y < g->h + SCENERY; y++) for (int x = -SCENERY; x < g->w + SCENERY; x++) draw_tile(tile_at(x, y), x, y);
    {   /* un castillo de cada equipo detrás de su lado del tablero */
        const art_t *c = &ART_TILE[TILE_CASTLE]; int px, py;
        cell_xy(g->w / 2, g->h, &px, &py); gfx_art(c->px, c->w, c->h, px - c->w, py + 8, 2, false, TPAL[0], 255, 0, 0);
        cell_xy(g->w / 2, -2, &px, &py); gfx_art(c->px, c->w, c->h, px - c->w, py - 8, 2, false, TPAL[1], 255, 0, 0);
    }
    /* borde del área de juego */
    gfx_fill_a(BX - 2, BY - 2, CELL * g->w + 4, 2, 0xFFF0, 150); gfx_fill_a(BX - 2, BY + CELL * g->h, CELL * g->w + 4, 2, 0xFFF0, 150);
    gfx_fill_a(BX - 2, BY, 2, CELL * g->h, 0xFFF0, 150); gfx_fill_a(BX + CELL * g->w, BY, 2, CELL * g->h, 0xFFF0, 150);
    for (int y = 0; y < g->h; y++) for (int x = 0; x < g->w; x++) if (g->orbs[y][x]) { int px, py; cell_xy(x, y, &px, &py); gem(px + SC(20), py + SC(6) + (int)lroundf(sinf(NOW / 300.0f + x) * 1.5f), C_MANA); }
    /* casillas marcadas: relleno translúcido con borde claro, al estilo de los tácticos clásicos */
    if (o) for (int i = 0; i < o->nhl; i++) {
        int px, py; cell_xy(o->hl[i].x, o->hl[i].y, &px, &py); uint16_t c = o->hl[i].c; uint8_t a = o->hl[i].a > 110 ? o->hl[i].a : (uint8_t)(o->hl[i].a * 2);
        gfx_fill_a(px + 1, py + 1, CELL - 2, CELL - 2, c, a);
        gfx_fill_a(px + 1, py + 1, CELL - 2, 1, 0xFFFF, 90); gfx_fill_a(px + 1, py + CELL - 2, CELL - 2, 1, 0xFFFF, 60);
        gfx_fill_a(px + 1, py + 2, 1, CELL - 4, 0xFFFF, 90); gfx_fill_a(px + CELL - 2, py + 2, 1, CELL - 4, 0xFFFF, 60);
    }
    /* por filas, de arriba abajo: las torres y criaturas de abajo tapan a las de arriba */
    for (int y = 0; y < g->h; y++) {
        for (int x = 0; x < g->w; x++) if (g->tiles[y][x] == 'S') draw_tower(x, y);
        for (int i = 0; i < g->nunits; i++) if (g->units[i].alive && g->units[i].y == y) draw_unit(&g->units[i]);
    }
    for (int i = 0; i < MAX_ANIMS; i++) {
        anim_t *a = &U.anims[i];
        if (!a->on || a->deferred || a->type != AN_DEATH || NOW < a->t0 || NOW >= a->t0 + a->dur) continue;
        float p = (float)(NOW - a->t0) / a->dur; int px, py; cell_xy(a->x, a->y, &px, &py); float sc = spr_sc() * (1 - p * 0.6f);
        sprite(a->cid, a->p, px + CELL / 2 - 6 * sc, py + SC(25) - 12 * sc - p * 6, sc, p < 0.3f ? SM_FLASH : SM_NORMAL, (uint8_t)(255 * (1 - p)));
        if (!a->done) { a->done = true; uint16_t c[3] = {RGB(0xc9, 0x8a, 0x5b), TEAM[a->p].b, C_MANA}; burst(px + CELL / 2, py + CELL / 2, 14, c, 3, 2.2f); }
    }
    if (o && o->ghost) { int px, py; cell_xy(o->gx, o->gy, &px, &py); sprite(o->gcid, o->gp, px + 1, py + 1, spr_sc(), SM_NORMAL, (uint8_t)(128 + 64 * sinf(NOW / 150.0f))); }
    if (o && o->cursor) draw_cursor(o->cx, o->cy, o->cc);
}
static void draw_floats(void) {
    for (int i = 0; i < MAX_ANIMS; i++) {
        anim_t *a = &U.anims[i];
        if (!a->on || a->deferred || NOW < a->t0) continue;
        if (a->type == AN_HAPTIC && !a->done) { a->done = true; haptic((haptic_t)a->id); }
        if (a->type == AN_BURST && !a->done) {
            a->done = true; int px, py; cell_xy(a->x, a->y, &px, &py);
            if (a->p == -1) { uint16_t c[3] = {C_CLAY, RGB(0x8a, 0x5a, 0x3a), RGB(0x5a, 0x3a, 0x24)}; burst(px + CELL / 2, py + CELL - 4, 16, c, 3, 1.8f); }
            else if (a->p == -2) { uint16_t c[3] = {RGB(0x8e, 0x83, 0x99), RGB(0x5d, 0x59, 0x6b), 0xFFFF}; burst(px + CELL / 2, py + CELL / 2, 18, c, 3, 1.6f); }
            else { uint16_t c[3] = {C_MANA, 0xFFFF, TEAM[a->p].l}; burst(px + CELL / 2, py + CELL / 2, 26, c, 3, 2.6f); }
        }
        if (a->type == AN_BOLT && NOW < a->t0 + a->dur) {
            /* rayo azul en zigzag que parpadea entre dos casillas */
            int x0, y0, x1, y1; cell_xy(a->fx, a->fy, &x0, &y0); cell_xy(a->x, a->y, &x1, &y1);
            x0 += CELL / 2; y0 += CELL / 2; x1 += CELL / 2; y1 += CELL / 2;
            float px = (float)x0, py = (float)y0, len = sqrtf((float)((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0))) + 1, nx = -(y1 - y0) / len, ny = (x1 - x0) / len;
            uint8_t al = (uint8_t)(255 * (1 - (float)(NOW - a->t0) / a->dur));
            for (int k = 1; k <= 6; k++) {
                float f = k / 6.0f, j = k < 6 ? (frand() - 0.5f) * 12 : 0;
                float qx = x0 + (x1 - x0) * f + nx * j, qy = y0 + (y1 - y0) * f + ny * j;
                int steps = (int)(fabsf(qx - px) + fabsf(qy - py)) + 1;
                for (int t = 0; t <= steps; t++) {
                    float sx = px + (qx - px) * t / steps, sy = py + (qy - py) * t / steps;
                    gfx_fill_a((int)sx - 2, (int)sy - 2, 4, 4, RGB(0x5b, 0x9c, 0xf0), al / 2);
                    gfx_fill_a((int)sx - 1, (int)sy - 1, 2, 2, RGB(0xd8, 0xec, 0xff), al);
                }
                px = qx; py = qy;
            }
            if (!a->done) { a->done = true; uint16_t c[3] = {RGB(0xa9, 0xcd, 0xff), 0xFFFF, C_BLUE}; burst(x1, y1, 12, c, 3, 2.2f); }
            continue;
        }
        if (a->type != AN_FLOAT || NOW >= a->t0 + a->dur) continue;
        float p = (float)(NOW - a->t0) / a->dur; int px, py; cell_xy(a->x, a->y, &px, &py);
        int x = px + CELL / 2; if (x < 110) x = 110; if (x > 250) x = 250;
        gfx_global_alpha(p > 0.7f ? (uint8_t)(255 * (1 - p) / 0.3f) : 255);
        txt(a->text, x, py + 4 - (int)(p * 18), strlen(a->text) > 3 ? F_T1 : F_T2, a->color);
        gfx_global_alpha(255);
    }
}
static void draw_particles(void) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        particle_t *p = &U.parts[i]; if (p->life <= 0) continue;
        p->x += p->vx; p->y += p->vy; p->vy += 0.18f; p->life -= 0.025f;
        if (p->life > 0) gfx_fill_a((int)p->x, (int)p->y, p->s, p->s, p->c, (uint8_t)(255 * p->life));
    }
}
static uint16_t tint(int p) { return p ? C_ORANGE : C_BLUE; }

/* ---------- Escenas ---------- */
static void sc_splash(void) {
    float p = fminf(1.0f, (NOW - U.t0) / 700.0f);
    for (int i = 0; i < C_COUNT; i++) {
        float a = i / (float)C_COUNT * 6.2832f + NOW / 4000.0f;
        sprite(i, i % 2, CX + cosf(a) * 128 - 12, CY + sinf(a) * 128 - 12 + sinf(NOW / 200.0f + i) * 2, 2, SM_NORMAL, (uint8_t)(230 * p));
    }
    txt(T->project, CX, 128, F_T1, C_DIM);
    txt(T->clay, CX, 158, F_T3, C_CLAY);
    txt(T->working, CX, 186, F_S, C_DIM);
    txt(T->tagline, CX, 206, F_S, C_INK);
    if (blink(500)) txt(T->tap_start, CX, 244, F_T1, C_GOLD);
}
static void sc_menu(void) {
    txt(T->clay, CX, 72, F_T2, C_CLAY);
    list_menu(T->menu, 5, U.menu_idx, 106, 31);
    sprite(C_BEAST, 0, 96, 256, 3, SM_NORMAL, 255); sprite(C_GOLEM, 1, 228, 256, 3, SM_NORMAL, 255);
    txt(T->menu_hint, CX, 300, F_S, C_DIM);
}
static void sc_level(void) {
    static const uint16_t COL[LVL_COUNT] = {C_GOOD, C_GOLD, C_BAD};
    static const int SHOW[LVL_COUNT] = {C_LARVA, C_SOLDIER, C_COLOSSUS};
    char b[64]; int i = U.lvl_idx;
    txt(T->lvl_title, CX, 50, F_T2, C_ORANGEL);
    list_menu(T->lvl_names, LVL_COUNT, i, 96, 34);
    gfx_fill(70, 96 + i * 34 - 16, 3, 32, COL[i]);
    sprite(SHOW[i], 1, 298, 80 + i * 34 + sinf(NOW / 300.0f) * 2, 2, SM_NORMAL, 255);
    gfx_wrap(T->lvl_desc[i], CX, 200, 250, 16, F_S, C_INK);
    const brain_memory_t *m = brain_memory(); int pl = m->played[i], w = m->human_won[i];
    txt(pl ? FMT(b, T->lvl_record, w, pl) : T->lvl_new, CX, 266, F_XS, COL[i]);
    dots(LVL_COUNT, i, 288, COL[i]);
    txt(T->lvl_hint, CX, 312, F_XS, C_DIM);
}
static void sc_options(void) {
    char a[48], b[48];
    txt(T->options, CX, 72, F_T2, C_GOLD);
    const char *items[3] = {FMT(a, "%s: %s", T->lang, T->lang_name), FMT(b, "%s: %s", T->haptic, U.haptic_on ? T->on : T->off), T->back};
    list_menu(items, 3, U.opt_idx, 140, 42);
    txt("Español · English", CX, 268, F_S, C_DIM);
    txt(T->opt_hint, CX, 294, F_S, C_DIM);
    char t[48];
    if (U.tests_total) txt(FMT(t, T->tests_line, U.tests_pass, U.tests_total), CX, 318, F_XS, U.tests_pass == U.tests_total ? C_GOOD : C_BAD);
}
static void sc_rules(void) {
    txt(T->howto, CX, 52, F_S, C_DIM);
    txt(T->rules[U.rules_page][0], CX, 82, F_T2, C_GOLD);
    char rb[320];
    if (U.rules_page == 6) snprintf(rb, sizeof rb, T->rules[6][1], RULES.power_cost[P_HARDEN], RULES.power_cost[P_HASTE], RULES.power_cost[P_SURGE], RULES.power_cost[P_RESUMMON], RULES.income_per_round);
    else snprintf(rb, sizeof rb, "%s", T->rules[U.rules_page][1]);
    gfx_wrap(rb, CX, 114, 262, 17, gfx_wrap_count(rb, 262, F_S) > 10 ? F_XS : F_S, C_INK);
    dots(7, U.rules_page, 312, C_GOLD);
    txt(T->hold_exit, CX, 334, F_XS, C_DIM);
}
static void sc_privacy(void) {
    int pc = U.priv_color;
    gfx_fill(CX - 34, 62, 68, 68, C_OUT);
    gfx_fill(CX - 31, 65, 62, 62, pc >= 0 ? TEAM[pc].d : RGB(0x7a, 0x5a, 0x44));
    gfx_fill(CX - 26, 70, 52, 52, pc >= 0 ? TEAM[pc].b : RGB(0xb0, 0x7b, 0x55));
    gfx_fill(CX - 3, 80, 6, 32, C_OUT); gfx_fill(CX - 16, 93, 32, 6, C_OUT);
    const char *const *lines = U.priv_key == 0 ? T->priv_start : U.priv_key == 1 ? T->priv_pass : T->priv_reveal;
    for (int i = 0; i < 4; i++) txt(lines[i], CX, 160 + i * 22, i == 0 ? F_T1 : F_M, C_INK);
    if (NOW - U.t0 > 400 && blink(550)) txt(T->tap_ready, CX, 270, F_T1, C_GOLD);
}
static void sc_builder(void) {
    int pl = U.b_player; army_t *army = &U.armies[pl]; char b[96];
    int spent = army_cost(army), left = RULES.mana_budget - spent;
    txt(FMT(b, T->army, tname(pl)), CX, 34, F_T1, TEAM[pl].l);
    for (int i = 0; i < 20; i++) { int x = 98 + (int)(i * 8.4f); gfx_fill(x - 1, 48, 7, 9, C_OUT); gfx_fill(x, 49, 5, 7, i < spent ? RGB(0x4a, 0x3a, 0x63) : C_MANA); }
    txt(FMT(b, T->mana_left, left), CX, 68, F_S, C_MANA);
    if (U.b_idx == C_COUNT) {
        txt(T->done, CX, 116, F_T2, army->n ? C_GOLD : C_DIM);
        txt(army->n ? FMT(b, T->army_summary, army->n, spent) : T->army_empty, CX, 150, F_M, C_INK);
        txt(army->n ? T->seal : T->turn_to_pick, CX, 174, F_S, C_DIM);
        if (left > 0 && army->n) txt(FMT(b, T->leftover, left), CX, 198, F_S, C_ORANGEL);
    } else {
        int id = U.b_idx; const creature_t *c = &CREATURES[id]; bool can = army_can_add(army, id) == ADD_OK;
        /* mitad izquierda: retrato grande sobre una franja del color del equipo */
        for (int y = 84; y < 286; y++) { int x1 = 182 - (y - 84) / 5; gfx_fill_a(0, y, x1, 1, TEAM[pl].d, 150); }
        gfx_fill_a(0, 84, 182, 2, TEAM[pl].l, 200); gfx_fill_a(0, 284, 142, 2, TEAM[pl].l, 200);
        int fm = NOW - U.add_flash < 180 ? 2 : 0;
        if (!portrait_full(id, pl, 10, 86 + sinf(NOW / 400.0f) * 2, 168, 196, can ? 255 : 110, fm)) bust(id, pl, 26, 92 + sinf(NOW / 400.0f) * 2, 4.25f, false, can ? 255 : 110, fm);
        char up[24]; snprintf(up, sizeof up, "%s", L(c->name)); for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
        panel(26, 236, 136, 40, TEAM[pl].l);
        txt(up, 94, 249, F_T1, C_INK);
        txt(FMT(b, T->role_cost, L(c->role), c->cost), 94, 266, F_XS, can ? C_MANA : C_BAD);
        txt("<", 14, 180, F_T2, C_DIM); txt(">", 346, 180, F_T2, C_DIM);
        /* mitad derecha: atributos, especial, pasiva y un consejo */
        int rx = 258; char rng[8];
        if (c->special == SP_BREATH) snprintf(rng, sizeof rng, "1/3"); else if (c->special == SP_CHAIN) snprintf(rng, sizeof rng, "1/%d", RULES.chain_range);
        else if (c->ability == AB_MARKSMAN) snprintf(rng, sizeof rng, "2-%d", c->rng); else snprintf(rng, sizeof rng, "%d", c->rng);
        txt(FMT(b, "%s %d  ATK %d  DEF %d", T->stats[0], c->hp, c->atk, c->def), rx, 90, F_S, C_INK);
        txt(FMT(b, "MOV %d  %s %s", c->mov, T->stats[4], rng), rx, 106, F_S, C_INK);
        gfx_fill_a(rx - 66, 117, 132, 1, C_DIM, 160);
        txt(FMT(b, "%s · %d %s", L(c->sp_name), c->sp_ap, T->ap), rx, 130, F_S, C_GOLD);
        int nl = gfx_wrap(L(c->sp_text), rx, 145, 140, 11, F_XS, C_INK);
        int y = 145 + nl * 11 + 6;
        if (c->ability != AB_NONE) { txt(FMT(b, "%s: %s", T->passive_lbl, L(c->ab_name)), rx, y, F_XS, RGB(0xcf, 0xc6, 0xb8)); y += 15; }
        gfx_fill_a(rx - 66, y - 6, 132, 1, C_DIM, 160);
        gfx_wrap(T->tips[id], rx, y + 5, 136, 11, F_XS, C_GOOD);
    }
    int sx = CX - 6 * 26 / 2;
    for (int i = 0; i < 6; i++) { int x = sx + i * 26; gfx_fill(x, 298, 24, 24, C_FAINT); if (i < army->n) sprite(army->cid[i], pl, x, 298, 2, SM_NORMAL, 255); }
    bool msg_on = NOW - U.msg_t < 1600;
    txt(msg_on ? U.msg : T->builder_hint, CX, 334, F_XS, msg_on ? C_GOLD : C_DIM);
}
static void sc_reveal(void) {
    float p = fminf(1.0f, (NOW - U.t0) / 900.0f); char b[48];
    txt("VS", CX, CY, F_T3, C_GOLD);
    for (int k = 0; k < 2; k++) {
        int pl = 1 - k; army_t *army = &U.armies[pl]; int y = pl == 1 ? 96 : 236, n = army->n;
        txt(FMT(b, T->reveal_mana, tname(pl), army_cost(army)), CX, pl == 1 ? 60 : 300, F_T1, TEAM[pl].l);
        if (U.vs && pl == BIDDY) { char d2[64]; txt(FMT(d2, T->ai_doctrine, L(DOCTRINES[U.doctrine].name), T->lvl_names[U.lvl]), CX, 78, F_XS, C_DIM); }
        for (int i = 0; i < n; i++) {
            int x = CX - n * 40 / 2 + i * 40 + 4; float d = fmaxf(0, fminf(1, p * 1.6f - i * 0.12f));
            sprite(army->cid[i], pl, x, y + (1 - d) * (pl == 1 ? -30 : 30), 2.66f, d < 0.5f ? SM_SIL : SM_NORMAL, (uint8_t)(255 * d));
        }
    }
    if (NOW - U.t0 > 1400 && blink(550)) txt(T->tap_map, CX, 330, F_S, C_GOLD);
    if (U.reveal_burst != U.t0) { U.reveal_burst = U.t0; uint16_t c[3] = {C_BLUE, C_ORANGE, C_GOLD}; burst(CX, CY, 40, c, 3, 3.5f); }
}
static void sc_map(void) {
    const map_t *m = &MAPS[U.map_idx];
    txt(T->pick_field, CX, 42, F_S, C_DIM);
    char up[24]; snprintf(up, sizeof up, "%s", m->name); for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    txt(up, CX, 64, F_T1, C_INK);
    int cs = m->w > 8 ? 14 : 17, ox = CX - cs * m->w / 2, oy = 86;
    for (int y = 0; y < m->h; y++) for (int x = 0; x < m->w; x++) {
        uint16_t c;
        switch (m->rows[y][x]) {
        case '#': c = RGB(0x4a, 0x3f, 0x52); break; case 'R': c = RGB(0x5b, 0x55, 0x66); break; case 'T': c = RGB(0x2f, 0x7a, 0x45); break;
        case 'u': c = RGB(0x6d, 0x60, 0x77); break; case '~': c = RGB(0x1f, 0x4f, 0x7a); break; case 'L': c = RGB(0xd0, 0x49, 0x1f); break;
        case 'I': c = RGB(0xb4, 0xdc, 0xec); break;
        case ':': c = RGB(0x3d, 0x33, 0x45); break; case 'C': c = C_GOLD; break; case 'S': c = C_MANA; break; default: c = RGB(0x2a, 0x22, 0x30);
        }
        gfx_fill(ox + x * cs, oy + y * cs, cs - 1, cs - 1, c);
        if (m->rows[y][x] == 'C') gfx_fill(ox + x * cs + cs / 2 - 1, oy + y * cs + 2, 2, cs - 5, C_OUT);
    }
    for (int p = 0; p < 2; p++) for (int r = 0; r < 2; r++) gfx_fill_a(ox, oy + m->deploy[p][r] * cs, cs * m->w - 1, cs - 1, tint(p), 64);
    { char sz[32]; txt(FMT(sz, T->map_size, m->w, m->h), CX, oy + cs * m->h + 9, F_XS, C_DIM); }
    gfx_wrap(L(m->blurb), CX, 246, 236, 15, F_S, C_INK);
    dots(MAP_COUNT, U.map_idx, 284, C_GOLD);
    bool msg_on = NOW - U.msg_t < 1600;
    txt(msg_on ? U.msg : T->play_here, CX, 304, F_S, msg_on ? C_ORANGEL : C_GOLD);
}
static void panel(int x, int y, int w, int h, uint16_t edge);
static void draw_cut(void);
static void sc_deploy(void) {
    game_t *g = &U.g; target_t cells[MAX_TARGETS]; int n = deploy_cells(g, cells); char b[64];
    if (!n) return;
    int pl = g->deploy_q[g->dq_head].player, cid = g->deploy_q[g->dq_head].cid;
    if (U.deploy_idx >= n) U.deploy_idx = n - 1;
    board_opts_t o = {0};
    for (int i = 0; i < n; i++) o.hl[o.nhl++] = (hl_t){cells[i].x, cells[i].y, tint(pl), 56};
    o.cursor = true; o.cx = cells[U.deploy_idx].x; o.cy = cells[U.deploy_idx].y; o.cc = TEAM[pl].l;
    o.ghost = true; o.gx = o.cx; o.gy = o.cy; o.gcid = cid; o.gp = pl;
    { float dt = fminf(0.1f, (NOW - U.cam_t) / 1000.0f); U.cam_t = NOW; cam_focus(o.cx, o.cy); cam_update(dt); }
    draw_board(&o);
    panel(96, 30, 168, 40, TEAM[pl].b); panel(46, 290, 268, 62, C_GOLD);
    txt(T->deploy, CX, 40, F_S, C_DIM);
    txt(FMT(b, T->places, tname(pl), cname(cid)), CX, 60, F_T1, TEAM[pl].l);
    txt(FMT(b, T->remaining, g->dq_len - g->dq_head), CX, 302, F_S, C_INK);
    txt(T->deploy_hint1, CX, 320, F_XS, C_DIM);
    txt(T->deploy_hint2, CX, 335, F_XS, C_DIM);
}
/* Panel azul marino con borde fino, como los menús de los tácticos clásicos */
static void panel(int x, int y, int w, int h, uint16_t edge) {
    gfx_fill_a(x, y, w, h, RGB(0x12, 0x16, 0x2c), 222);
    gfx_fill_a(x + 1, y + 1, w - 2, 1, 0xFFFF, 50);
    gfx_fill_a(x, y, w, 1, edge, 220); gfx_fill_a(x, y + h - 1, w, 1, edge, 220);
    gfx_fill_a(x, y, 1, h, edge, 220); gfx_fill_a(x + w - 1, y, 1, h, edge, 220);
}
static void hud_top(void) {
    game_t *g = &U.g; char b[32];
    panel(84, 31, 74, 22, C_BLUE); panel(202, 31, 74, 22, C_ORANGE); panel(154, 31, 52, 22, C_GOLD); panel(118, 53, 124, 19, TEAM[g->current].b);
    txt(FMT(b, "R%d/%d", g->round, RULES.max_rounds), CX, 42, F_S, C_GOLD);
    flag(98, 42, C_BLUE); txtl(FMT(b, "%d", g->vp[0]), 106, 42, F_T1, C_INK); gem(126, 42, C_MANA); txtl(FMT(b, "%d", g->mana[0]), 132, 42, F_T1, C_MANA);
    flag(222, 42, C_ORANGE); txtl(FMT(b, "%d", g->vp[1]), 230, 42, F_T1, C_INK); gem(250, 42, C_MANA); txtl(FMT(b, "%d", g->mana[1]), 256, 42, F_T1, C_MANA);
    txt(FMT(b, T->turn, tname(g->current)), CX, 61, F_T1, TEAM[g->current].l);
}
static void unit_line(const unit_t *u, int y) {
    const creature_t *c = &CREATURES[u->cid]; char b[8];
    bool show_ap = U.g.active_id == u->id; int ap_max = u->ap_start > 2 ? u->ap_start : 2;
    int nw = gfx_text_w(L(c->name), F_T1);
    int x = CX - (nw + 8 + 46 + (show_ap ? ap_max * 8 : 0)) / 2;
    txtl(L(c->name), x, y, F_T1, TEAM[u->player].l); x += nw + 8;
    hp_bar(x, y - 1, 30, u->hp, u->max_hp, TEAM[u->player].l); txtl(FMT(b, "%d", u->hp), x + 35, y, F_S, C_INK); x += 46;
    if (show_ap) pips(x, y - 3, u->ap, ap_max, C_GOLD, C_FAINT, 6);
}
static bool hostile_action(action_id_t id) { unit_t *u = active_unit(&U.g); return id == A_ATTACK || (id == A_ABILITY && u && special_is_attack(u->cid)); }
static void draw_inspect(void);
static void draw_pause(void);
static void draw_power(void);
static void draw_fallen(void);
static void draw_dice(void);
static void sc_battle(void) {
    game_t *g = &U.g; board_opts_t o = {0}; char b[96];
    int ids[MAX_UNITS]; int nsel = sel_list(ids);
    if (U.mode == M_SELECT || U.mode == M_PAUSE) {
        for (int i = 0; i < nsel; i++) { const unit_t *u = unit_by_id_c(g, ids[i]); o.hl[o.nhl++] = (hl_t){u->x, u->y, tint(g->current), 46}; }
        if (U.sel_idx < nsel) { const unit_t *u = unit_by_id_c(g, ids[U.sel_idx]); o.cursor = true; o.cx = u->x; o.cy = u->y; o.cc = TEAM[g->current].l; }
    } else if (U.mode == M_ACTION || U.mode == M_INSPECT) {
        action_t *a = &U.actions[U.act_idx]; unit_t *u = active_unit(g);
        if (a->enabled && (a->id == A_MOVE || a->id == A_RETREAT || a->id == A_ATTACK || a->id == A_ABILITY)) {
            target_t t[MAX_TARGETS]; int n = targets_for(g, a->id, t);
            bool host = hostile_action(a->id), friendly = a->id == A_ABILITY && !host;
            for (int i = 0; i < n; i++) o.hl[o.nhl++] = (hl_t){t[i].x, t[i].y, host ? C_BAD : friendly ? C_GOOD : tint(g->current), host ? 97 : friendly ? 90 : 77};
        }
        if (u) { o.cursor = true; o.cx = u->x; o.cy = u->y; o.cc = C_INK; }
    } else if (U.mode == M_TARGET) {
        bool host = hostile_action(U.action_id), friendly = U.action_id == A_ABILITY && !host;
        for (int i = 0; i < U.ntargets; i++) o.hl[o.nhl++] = (hl_t){U.targets[i].x, U.targets[i].y, host ? C_BAD : friendly ? C_GOOD : tint(g->current), host ? 97 : friendly ? 90 : 77};
        o.cursor = true; o.cx = U.targets[U.tgt_idx].x; o.cy = U.targets[U.tgt_idx].y; o.cc = host ? C_BAD : C_GOLD;
    } else if (U.mode == M_PTARGET) {
        for (int i = 0; i < U.ntargets; i++) o.hl[o.nhl++] = (hl_t){U.targets[i].x, U.targets[i].y, C_GOOD, 90};
        o.cursor = true; o.cx = U.targets[U.tgt_idx].x; o.cy = U.targets[U.tgt_idx].y; o.cc = C_GOOD;
    } else if (U.mode == M_PCELL) {
        for (int i = 0; i < U.ntargets; i++) o.hl[o.nhl++] = (hl_t){U.targets[i].x, U.targets[i].y, C_MANA, 77};
        o.cursor = true; o.cx = U.targets[U.tgt_idx].x; o.cy = U.targets[U.tgt_idx].y; o.cc = C_MANA;
        o.ghost = true; o.gx = o.cx; o.gy = o.cy; o.gcid = U.fallen_pick.x /* cid */; o.gp = g->current;
    }
    {   /* cámara: sigue al cursor, a la criatura activa o a quien se está moviendo */
        float dt = fminf(0.1f, (NOW - U.cam_t) / 1000.0f); U.cam_t = NOW;
        int fx = -1, fy = -1; unit_t *au = active_unit(g);
        if (o.cursor) { fx = o.cx; fy = o.cy; } else if (au) { fx = au->x; fy = au->y; }
        for (int i = 0; i < MAX_ANIMS; i++) { anim_t *m = &U.anims[i]; if (m->on && !m->deferred && m->type == AN_MOVE && NOW < m->t0 + m->dur) { fx = m->x; fy = m->y; } }
        cam_focus(fx, fy); cam_update(dt);
    }
    draw_board(&o);
    draw_floats();
    hud_top();
    if (U.mode != M_DICE && !U.cut.on) panel(46, 288, 268, 66, C_GOLD);
    bool msg_on = NOW - U.msg_t < 1500;
    if ((U.mode == M_SELECT || U.mode == M_PAUSE) && ai_turn()) {
        char d3[8]; int k = (NOW / 300) % 4; for (int j = 0; j < 3; j++) d3[j] = j < k ? '.' : ' '; d3[3] = 0;
        txt(FMT(b, "%s%s", T->ai_thinking, d3), CX, 306, F_T1, C_ORANGEL);
        txt(FMT(b, T->vs_level, T->lvl_names[U.lvl]), CX, 326, F_XS, C_DIM);
        txt(T->hold_pause, CX, 344, F_XS, C_DIM);
    } else if (U.mode == M_SELECT || U.mode == M_PAUSE) {
        if (U.sel_idx >= nsel) {
            bool used = g->round < g->power_ready[g->current]; char cb[48];
            int w = gfx_text_w(T->summoner, F_T1);
            txt(T->summoner, CX - 10, 302, F_T1, C_MANA);
            gem(CX + w / 2 - 2, 302, C_MANA); txtl(FMT(b, "%d", g->mana[g->current]), CX + w / 2 + 4, 302, F_T1, C_MANA);
            txt(used ? FMT(cb, T->power_cooling, g->power_ready[g->current]) : T->summoner_sub, CX, 320, F_XS, used ? C_ORANGEL : C_INK);
        } else {
            unit_line(unit_by_id_c(g, ids[U.sel_idx]), 302);
            int bap = g->boost_ap[g->current], bmov = g->boost_mov[g->current];
            if (bap || bmov) {
                char s1[16] = "", s2[16] = "";
                if (bap) snprintf(s1, sizeof s1, " +%d %s", bap, T->ap);
                if (bmov) snprintf(s2, sizeof s2, " +%d MOV", bmov);
                txt(FMT(b, T->boost_ready, s1, s2), CX, 320, F_XS, C_GOLD);
            } else txt(FMT(b, T->to_activate, nsel), CX, 320, F_XS, C_DIM);
        }
        dots(nsel + 1, U.sel_idx, 330, U.sel_idx >= nsel ? C_MANA : C_GOLD);
        txt(T->hold_pause, CX, 344, F_XS, C_DIM);
    } else if (U.mode == M_ACTION || U.mode == M_INSPECT) {
        unit_t *u = active_unit(g); action_t *a = &U.actions[U.act_idx];
        if (u) unit_line(u, 300);
        char up[32]; snprintf(up, sizeof up, "%s", a->name ? a->name : "");
        for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
        if (a->ap) FMT(b, "%s · %d %s", up, a->ap, T->ap); else FMT(b, "%s", up);
        txt("<", 84, 319, F_T1, C_DIM); txt(">", 276, 319, F_T1, C_DIM);
        txt(b, CX, 319, F_T1, a->enabled ? C_INK : C_DIM);
        const char *hint = a->enabled ? (u && u->ap == u->ap_start ? T->tap_choose_other : T->tap_choose) : (a->reason ? a->reason : "");
        if (ai_turn()) hint = T->ai_playing;
        txt(msg_on ? U.msg : hint, CX, 336, F_XS, msg_on || !a->enabled ? C_ORANGEL : C_DIM);
    } else if (U.mode == M_TARGET) {
        target_t *t = &U.targets[U.tgt_idx];
        if (t->unit_id >= 0) {
            const unit_t *tu = unit_by_id_c(g, t->unit_id); unit_line(tu, 300);
            unit_t *att = active_unit(g);
            bool cover = terrain_of(g->tiles[tu->y][tu->x])->cover && !(U.action_id == A_ABILITY && att && CREATURES[att->cid].special == SP_BREATH) && tu->player != g->current;
            if (hostile_action(U.action_id) && att) {
                /* Vista previa: probabilidades exactas de este ataque */
                odds_t od; attack_odds(g, att, tu, kind_for(g, att, U.action_id), &od);
                int hit = (int)lroundf(od.p_hit * 100), ed = (int)lroundf(od.e_dmg * 10), kill = (int)lroundf(od.p_kill * 100);
                char o1[24], o2[24], o3[24];
                FMT(o1, T->odds_hit, hit); FMT(o2, T->odds_dmg, ed / 10, ed % 10); FMT(o3, T->odds_kill, kill);
                if (kill > 0) FMT(b, "%s · %s", o1, o3); else FMT(b, "%s", o1);
                txt(b, CX, 317, F_S, kill >= 50 ? C_GOLD : C_INK);
                if (od.nchain) { char o4[24]; FMT(o4, T->odds_chain, od.nchain); FMT(b, "%s · %s", o2, o4); txt(b, CX, 334, F_XS, RGB(0xa9, 0xcd, 0xff)); goto target_done; }
                FMT(b, "%s · DEF %d%s%s%s%s", o2, eff_def(g, tu), tu->defending ? " · " : "", tu->defending ? T->on_guard : "", cover ? " · " : "", cover ? T->cover : "");
                txt(b, CX, 334, F_XS, C_DIM);
                goto target_done;
            }
            FMT(b, "DEF %d%s%s%s%s", eff_def(g, tu), tu->defending ? " · " : "", tu->defending ? T->on_guard : "", cover ? " · " : "", cover ? T->cover : "");
            txt(b, CX, 318, F_S, C_INK);
        } else {
            const terrain_t *tr = terrain_of(g->tiles[t->y][t->x]);
            char nm[32]; snprintf(nm, sizeof nm, "%s", L(tr->name)); if (nm[0] >= 'A' && nm[0] <= 'Z') nm[0] = (char)(nm[0] + 32);
            txt(FMT(b, U.action_id == A_RETREAT ? T->retreat_to : T->move_to, nm), CX, 302, F_T1, C_INK);
            char b2[32];
            txt(tr->hazard ? T->burns : g->orbs[t->y][t->x] ? T->pick_mana : FMT(b2, T->n_of_m, U.tgt_idx + 1, U.ntargets), CX, 318, F_S, tr->hazard ? C_BAD : C_MANA);
        }
        txt(ai_turn() ? T->ai_playing : T->tap_confirm, CX, 336, F_XS, C_DIM);
    target_done:;
    } else if (U.mode == M_PTARGET) {
        unit_line(unit_by_id_c(g, U.targets[U.tgt_idx].unit_id), 300);
        txt(T->pick_ally, CX, 318, F_S, C_GOOD);
        txt(T->tap_confirm, CX, 336, F_XS, C_DIM);
    } else if (U.mode == M_PCELL) {
        txt(FMT(b, T->pick_cell, cname(U.fallen_pick.x)), CX, 302, F_T1, C_MANA);
        char b2[32]; txt(FMT(b2, T->mana_cost, U.fallen_pick.cost), CX, 318, F_S, C_MANA);
        txt(T->tap_confirm, CX, 336, F_XS, C_DIM);
    }
    if (U.mode == M_INSPECT) draw_inspect();
    if (U.mode == M_PAUSE) draw_pause();
    if (U.mode == M_POWER) draw_power();
    if (U.mode == M_PFALLEN) draw_fallen();
    if (U.mode == M_DICE) draw_dice();
    if (U.cut.on) draw_cut();
    if (U.banner.on) {
        uint32_t dur = U.banner.lng ? 1700 : 1100; float p = (float)(NOW - U.banner.t0) / dur;
        if (p >= 1) U.banner.on = false;
        else {
            float a = p < 0.12f ? p / 0.12f : p > 0.78f ? (1 - p) / 0.22f : 1;
            int lines = U.banner.lng ? gfx_wrap_count(U.banner.sub, 270, F_XS) : 0;
            int h = U.banner.lng ? 46 + lines * 13 : 60;
            gfx_global_alpha((uint8_t)(255 * a));
            gfx_fill_a(0, CY - h / 2, SCR, h, RGB(0x0d, 0x0a, 0x10), 235);
            gfx_fill(0, CY - h / 2, SCR, 3, TEAM[U.banner.team].b); gfx_fill(0, CY + h / 2 - 3, SCR, 3, TEAM[U.banner.team].b);
            if (U.banner.lng) {
                int top = CY - h / 2;
                txt(U.banner.text, CX, top + 17, F_T2, TEAM[U.banner.team].l);
                gfx_wrap(U.banner.sub, CX, top + 38, 270, 13, F_XS, C_INK);
            } else {
                txt(U.banner.text, CX, CY - (U.banner.sub[0] ? 6 : 0), F_T2, TEAM[U.banner.team].l);
                if (U.banner.sub[0]) txt(U.banner.sub, CX, CY + 17, F_S, C_INK);
            }
            gfx_global_alpha(255);
        }
    }
}
static void draw_cut(void) {
    float t = (float)(NOW - U.cut.t0), p = t / CUT_MS; if (p > 1) p = 1;
    float in = fminf(1, t / 220.0f), out = p > 0.86f ? (1 - p) / 0.14f : 1; float ein = 1 - (1 - in) * (1 - in) * (1 - in);
    const team_col_t *tc = &TEAM[U.cut.p];
    const hiart_t *sp = HI_SPLASH[U.cut.cid][U.cut.p];
    if (sp) {
        /* arte de pantalla completa: entra desde la derecha con un golpe de zoom */
        gfx_global_alpha((uint8_t)(255 * out));
        gfx_fill_a(0, 0, SCR, SCR, 0, (uint8_t)(220 * ein));
        float zoom = 1 + 0.08f * expf(-t / 180.0f), sw = sp->w * zoom;
        float x = (SCR - sw) / 2 + (1 - ein) * 220, y = (SCR - sp->h * zoom) / 2;
        gfx_art(sp->px, sp->w, sp->h, x, y, zoom, false, sp->pal, (uint8_t)(255 * ein), t < 80 ? 2 : 0, 0xFFFF);
        const creature_t *c = &CREATURES[U.cut.cid]; char up[40]; snprintf(up, sizeof up, "%s", L(c->sp_name));
        for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
        float tx = 108 - (1 - ein) * 160;
        for (int k = 0; k < 64; k++) gfx_fill_a(0, 128 + k, (int)tx + 104 - abs(k - 32), 1, 0, 170); /* placa oscura detrás del nombre */
        gfx_text(up, (int)tx, 150, &FONT_T2, C_INK, AL_CENTER, true);
        gfx_fill_a((int)tx - 60, 166, 120, 2, tc->l, 255);
        gfx_text(L(c->name), (int)tx, 180, &FONT_S, tc->l, AL_CENTER, true);
        if (U.cut.has_t && t > 380) { /* objetivo abajo a la izquierda, donde el arte deja espacio */
            float k = fminf(1, (t - 380) / 200.0f), e = 1 - (1 - k) * (1 - k);
            int px = 44, py = (int)(312 - 76 * e);
            gfx_fill_a(px - 6, py - 4, 84, 76, RGB(0x2a, 0x08, 0x0c), 225);
            gfx_fill_a(px - 6, py - 4, 84, 2, C_BAD, 255); gfx_fill_a(px - 6, py + 70, 84, 2, C_BAD, 255);
            int hm = (t > 900 && ((int)t / 60) % 2) ? 2 : 0;
            if (!portrait_half(U.cut.tcid, U.cut.tp, px + 36, py + 1, 68, 255, hm)) bust(U.cut.tcid, U.cut.tp, px + 4, py, 2.0f, U.cut.tp == 1, 255, hm);
        }
        gfx_global_alpha(255);
        return;
    }
    gfx_global_alpha((uint8_t)(255 * out));
    gfx_fill_a(0, 0, SCR, SCR, C_VEIL, 120);
    /* franja diagonal (sube hacia la derecha) que barre la pantalla desde la izquierda */
    int yc = 152, hh = 60; int wipe = (int)(SCR * ein);
    for (int x = 0; x < wipe; x++) {
        int yt = yc - hh + (int)((CX - x) * 0.22f);
        gfx_fill_a(x, yt, 1, 2 * hh, tc->d, 235);
        gfx_fill_a(x, yt, 1, 4, tc->l, 255); gfx_fill_a(x, yt + 2 * hh - 4, 1, 4, tc->l, 255);
    }
    for (int k = 0; k < 9; k++) { /* líneas de velocidad */
        int len = 40 + (k * 53) % 70, x = (int)(SCR - fmodf(t * (0.7f + k * 0.09f) + k * 97, SCR + len));
        int y = yc - hh + 10 + (k * 37) % (2 * hh - 20) + (int)((CX - x) * 0.22f);
        for (int i = 0; i < len; i += 2) if (x + i < wipe) gfx_fill_a(x + i, y - (int)(i * 0.22f), 2, 1, 0xFFFF, 110);
    }
    /* retrato grande del que actúa */
    float bx = -130 + 150 * ein, by = yc - hh + 2 + sinf(t / 140.0f) * 1.5f;
    {   /* medio cuerpo con un pequeño golpe de zoom al entrar */
        float zoom = 1 + 0.12f * expf(-t / 160.0f);
        if (!portrait_half(U.cut.cid, U.cut.p, bx + 60, yc - hh - 34 - (zoom - 1) * 60, 168 * zoom, 255, t < 90 ? 2 : 0))
            bust(U.cut.cid, U.cut.p, bx, by, 3.75f, U.cut.p == 1, 255, t < 90 ? 2 : 0);
    }
    /* nombre del especial */
    const creature_t *c = &CREATURES[U.cut.cid]; char up[40]; snprintf(up, sizeof up, "%s", L(c->sp_name));
    for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    float tx = 248 + (1 - ein) * 160;
    int ty = yc - (int)((tx - CX) * 0.22f);
    gfx_text(up, (int)tx, ty - 12, &FONT_T2, C_INK, AL_CENTER, true);
    gfx_text(L(c->name), (int)tx, ty + 14, &FONT_S, tc->l, AL_CENTER, true);
    /* retrato del objetivo, abajo a la derecha */
    if (U.cut.has_t && t > 380) {
        float k = fminf(1, (t - 380) / 200.0f), e = 1 - (1 - k) * (1 - k);
        int px = 214, py = (int)(306 - 76 * e);
        gfx_fill_a(px - 6, py - 4, 84, 76, RGB(0x2a, 0x08, 0x0c), 225);
        gfx_fill_a(px - 6, py - 4, 84, 2, C_BAD, 255); gfx_fill_a(px - 6, py + 70, 84, 2, C_BAD, 255);
        int hm = (t > 900 && ((int)t / 60) % 2) ? 2 : 0;
        if (!portrait_half(U.cut.tcid, U.cut.tp, px + 36, py + 1, 68, 255, hm)) bust(U.cut.tcid, U.cut.tp, px + 4, py, 2.0f, U.cut.tp == 1, 255, hm);
    }
    gfx_global_alpha(255);
}
static void draw_inspect(void) {
    veil();
    const unit_t *u = unit_by_id_c(&U.g, U.inspect_id); const creature_t *c = &CREATURES[u->cid]; char b[96];
    if (!portrait_half(u->cid, u->player, CX, 34 + (int)lroundf(sinf(NOW / 300.0f) * 2), 58, 255, 0)) bust(u->cid, u->player, CX - 28, 38 + (int)lroundf(sinf(NOW / 300.0f) * 2), 1.75f, u->player == 1, 255, 0);
    char up[24]; snprintf(up, sizeof up, "%s", L(c->name)); for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    txt(up, CX, 102, F_T2, TEAM[u->player].l);
    txt(FMT(b, "%s · %s", L(c->role), L(c->desc)), CX, 120, F_XS, C_DIM);
    int vals[5] = {u->hp, eff_atk(u), eff_def(&U.g, u), eff_mov(u), u->ap};
    const char *labels[5] = {T->stats[0], "ATK", "DEF", "MOV", T->ap};
    for (int i = 0; i < 5; i++) {
        int x = 92 + i * 44;
        if (i == 0) FMT(b, "%d/%d", u->hp, u->max_hp); else FMT(b, "%d", vals[i]);
        txt(b, x, 140, F_T1, C_INK); txt(labels[i], x, 155, F_XS, C_DIM);
    }
    txt(FMT(b, "%s · %d %s", L(c->sp_name), c->sp_ap, T->ap), CX, 174, F_T1, C_GOLD);
    int nl = gfx_wrap(L(c->sp_text), CX, 190, 256, 12, F_XS, C_INK);
    if (c->ability != AB_NONE) {
        int y = 190 + nl * 12 + 10;
        txt(FMT(b, "%s · %s", L(c->ab_name), T->passive), CX, y, F_T1, RGB(0xcf, 0xc6, 0xb8));
        gfx_wrap(L(c->ab_text), CX, y + 16, 256, 12, F_XS, C_INK);
    }
    b[0] = 0;
    if (u->defending) { strncat(b, T->on_guard, sizeof b - strlen(b) - 1); }
    char t2[24];
    if (u->atk_buff) { snprintf(t2, sizeof t2, "%s+%d ATK", b[0] ? " · " : "", u->atk_buff); strncat(b, t2, sizeof b - strlen(b) - 1); }
    if (u->def_buff) { snprintf(t2, sizeof t2, "%s+%d DEF", b[0] ? " · " : "", u->def_buff); strncat(b, t2, sizeof b - strlen(b) - 1); }
    if (u->mov_bonus) { snprintf(t2, sizeof t2, "%s+%d MOV", b[0] ? " · " : "", u->mov_bonus); strncat(b, t2, sizeof b - strlen(b) - 1); }
    if (u->hidden) { if (b[0]) strncat(b, " · ", sizeof b - strlen(b) - 1); strncat(b, T->hidden_lbl, sizeof b - strlen(b) - 1); }
    if (u->walling) { if (b[0]) strncat(b, " · ", sizeof b - strlen(b) - 1); strncat(b, T->f_wall, sizeof b - strlen(b) - 1); }
    if (b[0]) txt(b, CX, 306, F_S, C_GOOD);
    txt(T->tap_back, CX, 326, F_XS, C_DIM);
}
static void draw_pause(void) {
    veil();
    txt(T->pause, CX, 110, F_T2, C_INK);
    list_menu(T->pause_items, 2, U.pause_idx, 160, 40);
    txt(T->pause_note, CX, 250, F_S, C_DIM);
}
static void draw_power(void) {
    veil();
    game_t *g = &U.g; power_state_t *p = &U.powers[U.pow_idx]; char b[48];
    txt(T->power_title, CX, 44, F_T1, C_MANA);
    gem(CX - 10, 64, C_MANA); txtl(FMT(b, "%d", g->mana[g->current]), CX - 2, 64, F_T1, C_MANA);
    icon(p->id, CX - 27, 80 + (int)lroundf(sinf(NOW / 300.0f) * 2), 6, p->enabled ? 255 : 100);
    txt("<", 82, 108, F_T2, C_DIM); txt(">", 278, 108, F_T2, C_DIM);
    char up[32]; snprintf(up, sizeof up, "%s", L(POWERS[p->id].name));
    for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    txt(up, CX, 150, F_T2, p->enabled ? C_INK : C_DIM);
    if (p->id == P_RESUMMON) txt(p->cost >= 0 ? FMT(b, T->mana_from, p->cost) : "-", CX, 171, F_S, C_MANA);
    else txt(FMT(b, T->mana_cost, p->cost), CX, 171, F_S, C_MANA);
    gfx_wrap(L(POWERS[p->id].text), CX, 194, 240, 15, F_S, C_INK);
    bool msg_on = NOW - U.msg_t < 1500;
    txt(msg_on ? U.msg : p->enabled ? T->power_hint : p->reason, CX, 282, F_S, msg_on || !p->enabled ? C_ORANGEL : C_GOLD);
    dots(P_COUNT, U.pow_idx, 302, C_MANA);
}
static void draw_fallen(void) {
    veil();
    game_t *g = &U.g; target_t *f = &U.fallen[U.fallen_idx]; int cid = f->x; char b[48];
    txt(T->choose_fallen, CX, 46, F_T1, C_MANA);
    gem(CX - 10, 66, C_MANA); txtl(FMT(b, "%d", g->mana[g->current]), CX - 2, 66, F_T1, C_MANA);
    sprite(cid, g->current, CX - 36, 82 + (int)lroundf(sinf(NOW / 300.0f) * 2), 6, f->affordable ? SM_NORMAL : SM_GHOST, f->affordable ? 255 : 150);
    if (U.nfallen > 1) { txt("<", 82, 118, F_T2, C_DIM); txt(">", 278, 118, F_T2, C_DIM); }
    char up[24]; snprintf(up, sizeof up, "%s", cname(cid)); for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
    txt(up, CX, 168, F_T2, C_INK);
    txt(FMT(b, T->returns_half, (CREATURES[cid].hp + 1) / 2), CX, 190, F_S, C_INK);
    txt(FMT(b, T->mana_cost, f->cost), CX, 208, F_T1, f->affordable ? C_MANA : C_BAD);
    bool msg_on = NOW - U.msg_t < 1500;
    txt(msg_on ? U.msg : T->power_hint, CX, 282, F_S, msg_on ? C_ORANGEL : C_GOLD);
    dots(U.nfallen, U.fallen_idx, 302, C_MANA);
}
/* Dado dibujado píxel a píxel con rotación real: cada píxel de pantalla se lleva al espacio del dado. */
static void draw_die(float x, float y, int face, int size, float ang, float scale, float squash, uint16_t body, uint8_t alpha) {
    static const int8_t PP[7][6][2] = {{{0}}, {{0, 0}}, {{-1, -1}, {1, 1}}, {{-1, -1}, {0, 0}, {1, 1}}, {{-1, -1}, {1, -1}, {-1, 1}, {1, 1}},
                                       {{-1, -1}, {1, -1}, {0, 0}, {-1, 1}, {1, 1}}, {{-1, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {1, 1}}};
    float h = size * scale / 2, rr = h * 0.3f, out = 3 * scale, o = size * scale * 0.27f, pr = fmaxf(2.5f, size * scale / 13.0f);
    float ca = cosf(ang), sa = sinf(ang), sx = 1.0f / (1 + (1 - squash) * 0.6f), sy = 1.0f / squash;
    uint16_t hi = RGB(0xff, 0xf7, 0xea), lo = RGB(0xb9, 0xab, 0x98), pip = face == 1 ? RGB(0xc0, 0x39, 0x2b) : C_OUT;
    if (!body) body = RGB(0xe9, 0xdf, 0xd0);
    int R = (int)(h * 1.45f + out + 2);
    float base_y = y + h * (1 - squash); /* el aplastamiento se apoya en el suelo */
    for (int py = -R; py <= R; py++) for (int px = -R; px <= R; px++) {
        float dx = px * sx, dy = py * sy - (base_y - y) * sy;
        float u = dx * ca + dy * sa, v = -dx * sa + dy * ca;
        float qx = fabsf(u) - (h - rr), qy = fabsf(v) - (h - rr);
        float dist = (qx > 0 || qy > 0) ? sqrtf(fmaxf(qx, 0) * fmaxf(qx, 0) + fmaxf(qy, 0) * fmaxf(qy, 0)) - rr : fmaxf(qx, qy) - rr;
        if (dist > out) continue;
        uint16_t c;
        if (dist > 0) c = C_OUT;
        else {
            c = body;
            if (dist > -4 * scale) c = (u + v < 0) ? hi : lo; /* bisel: arriba-izquierda claro, abajo-derecha oscuro */
            for (int k = 0; k < face; k++) { float ex = u - PP[face][k][0] * o, ey = v - PP[face][k][1] * o; if (ex * ex + ey * ey <= pr * pr) { c = pip; break; } }
        }
        gfx_px((int)x + px, (int)y + py, c, alpha);
    }
}
static int face_for_angle(float a, int i) { int q = (int)floorf(a / (PI_F / 2)); return 1 + (((q + i * 3) % 6) + 6) % 6; }
static uint16_t charge_col(float k) { return k < 0.5f ? TEAM[U.g.current].b : k < 0.85f ? C_GOLD : C_BAD; }
static void draw_breakdown(const attack_t *p) {
    const char *what[12], *why[12]; int val[12], n = 0; bool skill = U.dice.stage == 0;
    if (skill) {
        what[n] = T->bd_die; why[n] = T->bd_die_why; val[n++] = p->skill_roll;
        for (int i = 0; i < p->nmods; i++) { what[n] = p->mods[i].what; why[n] = p->mods[i].why; val[n++] = p->mods[i].val; }
    } else for (int i = 0; i < p->nparts && n < 11; i++) { what[n] = p->parts[i].what; why[n] = p->parts[i].why; val[n++] = p->parts[i].val; }
    int total = skill ? p->skill_total : p->final_dmg;
    txt(skill ? T->bd_skill : T->bd_dmg, CX, 116, F_S, C_GOLD);
    int top = 138, bottom = skill ? 204 : 292 - p->nchain * 16;
    bool with_why = n * 27 + 24 <= bottom - top;
    int rh = with_why ? 27 : (n * 19 + 24 <= bottom - top ? 19 : 16);
    char b[16];
    for (int i = 0; i < n; i++) {
        int y = top + i * rh, dy = y - CY; float hw = sqrtf(180.0f * 180 - dy * dy) - 26; if (hw > 128) hw = 128;
        int xl = CX - (int)hw, xr = CX + (int)hw;
        txtl(what[i], xl, y, F_S, C_INK);
        if (i == 0) FMT(b, "%d", val[i]); else if (val[i] > 0) FMT(b, "+%d", val[i]); else if (val[i] < 0) FMT(b, "−%d", -val[i]); else FMT(b, "0");
        gfx_text(b, xr, y, F_T1, i == 0 ? C_INK : val[i] > 0 ? C_GOOD : val[i] < 0 ? C_BAD : C_DIM, AL_RIGHT, true);
        if (with_why && why[i][0]) txtl(why[i], xl + 6, y + 12, F_XS, C_DIM);
    }
    int y = top + n * rh + (with_why ? 0 : 4), dy = y - CY; float hw = sqrtf(180.0f * 180 - dy * dy) - 26; if (hw > 128) hw = 128;
    int xl = CX - (int)hw, xr = CX + (int)hw;
    gfx_fill(xl, y - 8, xr - xl, 1, C_DIM);
    txtl(T->bd_total, xl, y + 4, F_T1, C_INK);
    if (skill) FMT(b, "= %d", total); else FMT(b, "= %d %s", total, T->hp);
    gfx_text(b, xr, y + 4, F_T1, skill ? qcol(p->quality) : total > 0 ? C_BAD : C_DIM, AL_RIGHT, true);
    if (!skill && p->nchain) {
        /* cada salto: lo que traía el rayo, menos 1 por salto, menos lo que reste el objetivo */
        for (int k = 0; k < p->nchain; k++) {
            const unit_t *h = unit_by_id_c(&U.g, p->chain_ids[k]); if (!h) continue;
            char w[72]; snprintf(w, sizeof w, T->chain_row, cname(h->cid), p->chain_base, k + 1);
            if (CREATURES[h->cid].ability == AB_FORTRESS) strncat(w, eng_lang() == LANG_EN ? " − 1 Fort." : " − 1 Fort.", sizeof w - strlen(w) - 1);
            if (h->defending) strncat(w, eng_lang() == LANG_EN ? " − 1 guard" : " − 1 guardia", sizeof w - strlen(w) - 1);
            int yy = y + 24 + k * 16;
            txtl(w, xl, yy, F_XS, RGB(0xa9, 0xcd, 0xff));
            FMT(b, "= %d", p->chain_dmg[k]); gfx_text(b, xr, yy, F_S, p->chain_dmg[k] ? C_BAD : C_DIM, AL_RIGHT, true);
        }
    }
    if (skill) {
        /* Escala de resultados: qué pasa con cada total, y dónde cayó esta tirada */
        int sy = y + 26; if (sy < 222) sy = 222; if (sy > 232) sy = 232;
        for (int q = 0; q < 6; q++) {
            int ry = sy + q * 14; bool on = q == (int)p->quality; uint16_t qc = qcol((quality_t)q);
            if (on) gfx_fill_a(CX - 124, ry - 7, 240, 14, qc, 70);
            gfx_text(T->bd_num[q], CX - 120, ry, F_XS, on ? C_INK : C_DIM, AL_LEFT, false);
            gfx_text(T->bd_scale[q], CX - 74, ry, F_XS, on ? C_INK : qc, AL_LEFT, false);
            gfx_text(T->bd_eff[q], CX + 16, ry, F_XS, on ? C_INK : C_DIM, AL_LEFT, false);
        }
    }
}
static void draw_dice(void) {
    game_t *g = &U.g; attack_t *p = &U.pending; char b[160];
    float dt = fminf(0.1f, (NOW - U.dice.last_t) / 1000.0f); U.dice.last_t = NOW;
    veil();
    /* destello de pantalla: crítico dorado, pifia gris, muerte roja */
    if (U.dice.flash) {
        uint32_t ft = NOW - U.dice.flash_t; uint32_t dur = U.dice.flash == 2 ? 700 : 380;
        if (ft < dur) gfx_fill_a(0, 0, SCR, SCR, U.dice.flash == 1 ? C_GOLD : U.dice.flash == 2 ? RGB(0x50, 0x4a, 0x55) : C_BAD, (uint8_t)((U.dice.flash == 2 ? 130 : 150) * (1 - (float)ft / dur)));
    }
    const unit_t *att = unit_by_id_c(g, p->attacker_id), *tgt = unit_by_id_c(g, p->target_id);
    if (U.dice.phase == 1) gfx_global_alpha(90); /* el dado vuela por encima de la cabecera */
    txt(U.dice.stage == 0 ? T->skill_die : T->dmg_die, CX, 44, F_T1, C_GOLD);
    sprite(att->cid, att->player, CX - 70, 56, 2, SM_NORMAL, 255); sprite(tgt->cid, tgt->player, CX + 46, 56, 2, SM_NORMAL, 255);
    txt(">", CX, 68, F_T1, C_DIM);
    txt(FMT(b, T->vs, cname(att->cid), cname(tgt->cid)), CX, 92, F_S, C_INK);
    if (U.dice.phase == 2 && U.dice.page == 1) {
        draw_breakdown(p);
        txt(T->bd_hint_back, CX, 322, F_XS, C_GOLD);
        return;
    }
    if (U.dice.stage == 0) {
        if (p->nmods) {
            b[0] = 0; char t2[56];
            for (int i = 0; i < p->nmods; i++) { snprintf(t2, sizeof t2, "%s%s %+d", i ? " · " : "", p->mods[i].what, p->mods[i].val); strncat(b, t2, sizeof b - strlen(b) - 1); }
            txt(b, CX, 108, F_XS, C_DIM);
        } else txt(T->no_mods, CX, 108, F_XS, C_DIM);
    } else txt(p->damage_dice > 1 ? T->crush_note : L(QUALITY_LABEL[p->quality]), CX, 108, F_XS, qcol(p->quality));
    gfx_global_alpha(255);
    /* arco de carga alrededor del borde */
    float a0 = PI_F * 0.75f, sweep = PI_F * 1.5f;
    gfx_arc(CX, CY, 168, a0, a0 + sweep, 7, C_FAINT, 255);
    if (U.dice.intensity > 0) {
        int segs = 30;
        for (int i = 0; i < segs * U.dice.intensity; i++) {
            float f = (float)i / segs;
            gfx_arc(CX, CY, 168, a0 + sweep * f, a0 + sweep * fminf(U.dice.intensity, f + 1.0f / segs) - 0.015f, 7, charge_col(f), 255);
        }
    }
    float sh = NOW - U.shake_t0 < 300 ? U.shake_mag * (1 - (NOW - U.shake_t0) / 300.0f) : 0;
    if (U.dice.maxed && U.dice.phase == 0) sh += 1.2f;
    float jx = (frand() - 0.5f) * sh * 2, jy = (frand() - 0.5f) * sh * 2;
    int size = DIE_SIZE; float dy0 = CY + 4;
    if (U.dice.phase == 0) {
        uint32_t el = NOW - U.dice.t0; float k = U.dice.intensity;
        U.dice.intensity = fmaxf(0, k - dt * 0.1f); if (U.dice.intensity < 0.9f) U.dice.maxed = false;
        U.dice.spin *= expf(-dt * 1.5f);
        for (int i = 0; i < U.dice.n; i++) {
            float x = dice_x(i) + jx, y = dy0 + jy + sinf(NOW / 260.0f + i) * 2;
            float prev = U.dice.ang[i];
            U.dice.ang[i] += (U.dice.spin * (i ? -1 : 1) + sinf(NOW / 700.0f + i) * 0.15f) * dt;
            int q0 = (int)floorf(prev / (PI_F / 2)), q1 = (int)floorf(U.dice.ang[i] / (PI_F / 2));
            if (q0 != q1 && NOW - U.dice.last_face > 45) { U.dice.last_face = NOW; haptic(HAP_TICK); }
            /* resplandor que crece con la carga */
            if (k > 0.02f) {
                float pulse = 0.5f + 0.5f * sinf(NOW / (U.dice.maxed ? 70.0f : 180.0f));
                uint16_t gc = charge_col(k);
                gfx_disc((int)x, (int)y, (int)(30 + k * 18 + pulse * 4 * k), gc, (uint8_t)(30 + 50 * k));
                gfx_disc((int)x, (int)y, (int)(28 + k * 10), gc, (uint8_t)(40 + 70 * k));
            }
            /* chispas que orbitan, más cuanto más carga */
            int ne = (int)(k * 10);
            for (int e = 0; e < ne; e++) {
                float ea = U.dice.ang[i] * 0.6f + e * 2 * PI_F / ne + NOW / 400.0f, er = 44 + 6 * k + sinf(NOW / 120.0f + e) * 3;
                gfx_fill_a((int)(x + cosf(ea) * er) - 1, (int)(y + sinf(ea) * er) - 1, 3, 3, e % 2 ? C_GOLD : 0xFFFF, (uint8_t)(140 + 100 * k));
            }
            if (U.dice.maxed) gfx_ring((int)x, (int)y, 50 + (int)((NOW - U.dice.max_t) / 8 % 14), 2, C_BAD, 160);
            float pop = NOW - U.dice.click_t < 120 ? 1 + 0.06f * (1 - (NOW - U.dice.click_t) / 120.0f) : 1;
            draw_die(x, y, face_for_angle(U.dice.ang[i], i), size, U.dice.ang[i], pop, 1, 0, 255);
        }
        if (U.dice.maxed) txt(T->max_charge, CX, 250, F_T1, blink(120) ? C_BAD : C_GOLD);
        else txt(T->charge, CX, 250, F_T1, blink(400) ? C_INK : C_GOLD);
        txt(T->tap_throw, CX, 270, F_S, C_DIM);
        int left = el >= CHARGE_LIMIT ? 0 : (int)((CHARGE_LIMIT - el + 999) / 1000);
        txt(FMT(b, "%d s", left), CX, 298, F_S, left <= 3 ? C_BAD : C_DIM);
        txt(T->fair, CX, 316, F_XS, C_DIM);
        if (el >= CHARGE_LIMIT) release_dice();
    } else if (U.dice.phase == 1) {
        float pr = fminf(1, (float)(NOW - U.dice.roll_t0) / U.dice.roll_dur), e = 1 - (1 - pr) * (1 - pr) * (1 - pr);
        int hop; float lift = hop_height(pr, &hop);
        if (hop != U.dice.hop) {
            /* toca el suelo: chispas, golpe y un pequeño temblor */
            U.dice.hop = hop; U.dice.contact_t = NOW; U.shake_t0 = NOW; U.shake_mag = 1 + U.dice.intensity * 2.5f;
            uint16_t c[2] = {C_GOLD, 0xFFFF};
            for (int i = 0; i < U.dice.n; i++) burst(dice_x(i), dy0 + size / 2, 6, c, 2, 1.6f);
            haptic(HAP_THUD);
        }
        float squash = NOW - U.dice.contact_t < 140 ? 1 - 0.22f * (1 - (NOW - U.dice.contact_t) / 140.0f) : 1;
        for (int i = 0; i < U.dice.n; i++) {
            float ang = U.dice.ang_start[i] + (U.dice.ang_end[i] - U.dice.ang_start[i]) * e;
            float drift = (1 - e) * 26 * (i ? 1 : -1) * (U.dice.n > 1 ? 1 : 0.6f);
            float x = dice_x(i) + drift + jx, y = dy0 - lift + jy;
            int face = fabsf(U.dice.ang_end[i] - ang) < PI_F / 2 ? U.dice.values[i] : face_for_angle(ang, i);
            if (face != U.dice.last_q[i] && NOW - U.dice.last_face > 40) { U.dice.last_face = NOW; haptic(HAP_TICK); }
            U.dice.last_q[i] = face;
            /* estela: copias anteriores cada vez más tenues */
            int nt = U.dice.ntrail < 3 ? U.dice.ntrail : 3; /* 3 copias: suficiente y barato en la placa */
            for (int t = nt - 1; t >= 0; t--) {
                const trail_t *tr = &U.dice.trail[i][t];
                draw_die(tr->x, tr->y, tr->face, size, tr->a, 0.92f - t * 0.05f, 1, charge_col(U.dice.intensity), (uint8_t)(70 - t * 13) * (1 - pr));
            }
            draw_die(x, y, face, size, ang, 1, squash, 0, 255);
            for (int t = 4; t > 0; t--) U.dice.trail[i][t] = U.dice.trail[i][t - 1];
            U.dice.trail[i][0].x = x; U.dice.trail[i][0].y = y; U.dice.trail[i][0].a = ang; U.dice.trail[i][0].face = face;
            gfx_fill_a((int)x - 20, (int)dy0 + size / 2 + 4, 40, 3, 0, (uint8_t)(120 - fminf(100, lift)));
        }
        if (U.dice.ntrail < 5) U.dice.ntrail++;
        if (pr >= 1) land_dice();
    } else {
        uint32_t t = NOW - U.dice.result_t;
        /* onda expansiva al caer */
        if (t < 520) { float f = t / 520.0f; gfx_ring(CX, (int)dy0, (int)(30 + f * 140), 3, p->quality == Q_CRIT && U.dice.stage == 0 ? C_GOLD : TEAM[g->current].l, (uint8_t)(200 * (1 - f))); }
        if (t > 120 && t < 640 && p->quality == Q_CRIT && U.dice.stage == 0) { float f = (t - 120) / 520.0f; gfx_ring(CX, (int)dy0, (int)(30 + f * 120), 2, 0xFFFF, (uint8_t)(180 * (1 - f))); }
        float pop = 1 + 0.3f * expf(-(float)t / 110.0f) * cosf((float)t / 38.0f);
        int best = 0; for (int i = 0; i < U.dice.n; i++) { int v = RULES.damage_table[U.dice.values[i] - 1]; if (v > best) best = v; }
        for (int i = 0; i < U.dice.n; i++) {
            bool dim = U.dice.stage == 1 && U.dice.n > 1 && RULES.damage_table[U.dice.values[i] - 1] < best;
            uint16_t body = U.dice.stage == 0 && p->quality == Q_CRIT ? RGB(0xff, 0xe9, 0xa6) : U.dice.stage == 0 && p->quality == Q_CRITFAIL ? RGB(0x9a, 0x93, 0x8a) : 0;
            if (U.dice.stage == 0 && p->quality == Q_CRIT) gfx_disc((int)dice_x(i), (int)dy0, 40 + (int)(sinf(NOW / 150.0f) * 3), C_GOLD, 70);
            draw_die(dice_x(i) + jx, dy0 + jy, U.dice.values[i], size, 0, pop, 1, body, dim ? 100 : 255);
        }
        if (U.dice.stage == 0) {
            int mod = p->skill_total - p->skill_roll;
            char up[40]; snprintf(up, sizeof up, "%s", L(QUALITY_LABEL[p->quality]));
            for (char *q = up; *q; q++) if (*q >= 'a' && *q <= 'z') *q = (char)(*q - 32);
            txt(up, CX, 250, F_T2, qcol(p->quality));
            if (mod) FMT(b, "%d %s %d = %d", p->skill_roll, mod > 0 ? "+" : "−", abs(mod), p->skill_total); else FMT(b, T->rolled, p->skill_roll);
            txt(b, CX, 274, F_M, C_INK);
            if (p->quality == Q_CRITFAIL || p->quality == Q_FAIL) txt(p->quality == Q_CRITFAIL ? T->next_critfail : T->next_fail, CX, 294, F_S, C_DIM);
            else { char nb[96]; txt(FMT(nb, "%s · %s", T->next_dmg, T->bd_eff[p->quality]), CX, 294, F_XS, C_INK); }
        } else {
            if (p->final_dmg > 0) FMT(b, "-%d %s", p->final_dmg, T->hp); else FMT(b, "%s", T->no_damage);
            txt(b, CX, 252, F_T2, p->final_dmg > 0 ? C_BAD : C_DIM);
            if (!tgt->alive) { char t3[64]; txt(FMT(t3, T->back_to_clay, cname(tgt->cid)), CX, 282, F_T1, C_GOLD); }
            else { char t3[48]; txt(FMT(t3, "%s %d/%d", cname(tgt->cid), tgt->hp, tgt->max_hp), CX, 282, F_S, C_INK); }
            if (p->nchain) {
                /* saltos del rayo: a quién y cuánto */
                char t4[96] = "", t5[32];
                snprintf(t4, sizeof t4, "%s", T->chain_jumps);
                for (int k = 0; k < p->nchain; k++) { const unit_t *h = unit_by_id_c(g, p->chain_ids[k]); if (!h) continue; snprintf(t5, sizeof t5, "%s%s -%d", k ? " · " : " ", cname(h->cid), p->chain_dmg[k]); strncat(t4, t5, sizeof t4 - strlen(t4) - 1); }
                txt(t4, CX, 300, F_XS, RGB(0xa9, 0xcd, 0xff));
            }
        }
        if (t > 450) txt(T->bd_hint, CX, 318, F_XS, blink(600) ? C_GOLD : C_INK);
    }
}
static void sc_victory(void) {
    game_t *g = &U.g; int w = g->winner; char b[64];
    float p = fminf(1, (NOW - U.t0) / 600.0f);
    txt(w < 0 ? T->draw : U.vs && w != BIDDY ? T->you_win : FMT(b, T->wins, tname(w)), CX, 58, F_T2, w >= 0 ? TEAM[w].l : C_INK);
    if (g->win_reason == WIN_ELIMINATION) FMT(b, T->r_elim, tword(1 - w));
    else if (g->win_reason == WIN_POINTS) FMT(b, T->r_points, g->vp[0], g->vp[1]);
    else FMT(b, "%s", g->win_reason == WIN_COST ? T->r_cost : T->r_draw);
    txt(b, CX, 82, F_S, C_DIM);
    int n = 0, sv[MAX_UNITS];
    for (int i = 0; i < g->nunits && n < 6; i++) if (g->units[i].alive && (w < 0 || g->units[i].player == w)) sv[n++] = i;
    for (int i = 0; i < n; i++) sprite(g->units[sv[i]].cid, g->units[sv[i]].player, CX - n * 34 / 2 + i * 34 + 5, 98 - fabsf(sinf(NOW / 200.0f + i)) * 8 * p, 2, SM_NORMAL, 255);
    txt(tname(0), 132, 146, F_T1, C_BLUEL); txt(tname(1), 232, 146, F_T1, C_ORANGEL);
    int *vals[4] = {U.dmg, U.kills, g->vp, U.crits};
    for (int i = 0; i < 4; i++) {
        int y = 168 + i * 22;
        txtl(T->stat_rows[i], 66, y, F_S, C_DIM);
        txt(FMT(b, "%d", vals[i][0]), 132, y, F_T1, C_INK); txt(FMT(b, "%d", vals[i][1]), 232, y, F_T1, C_INK);
    }
    txt(FMT(b, T->rounds_rolls, g->round, U.rolls), CX, 266, F_S, C_INK);
    if (U.vs) { char b2[48]; txt(FMT(b2, T->vs_level, T->lvl_names[U.lvl]), CX, 282, F_XS, C_DIM); }
    if (U.vic_burst != U.t0) { U.vic_burst = U.t0; uint16_t c[3] = {w >= 0 ? TEAM[w].b : C_INK, w >= 0 ? TEAM[w].l : C_INK, C_GOLD}; burst(CX, 70, 50, c, 3, 4); }
    if (NOW - U.t0 > 1500 && blink(550)) txt(T->tap_menu, CX, 300, F_S, C_GOLD);
}

/* ---------- Bucle ---------- */
void ui_init(lang_t lang, bool haptic_on, int tests_pass, int tests_total) {
    memset(&U, 0, sizeof U); build_palettes(); brain_init();
    set_lang(lang); U.haptic_on = haptic_on; U.tests_pass = tests_pass; U.tests_total = tests_total;
    NOW = hal_ms(); go(SC_SPLASH);
}
void ui_frame(void) {
    NOW = hal_ms();
    if (U.cut.on && NOW >= U.cut.t0 + CUT_MS) finish_cut();
    if (U.after_action_at && NOW >= U.after_action_at) { U.after_action_at = 0; after_action(); }
    if (U.victory_at && NOW >= U.victory_at) { U.victory_at = 0; to_victory(); }
    ai_drive();
    for (int i = 0; i < MAX_ANIMS; i++) { anim_t *a = &U.anims[i]; if (a->on && !a->deferred && NOW > a->t0 + a->dur + 50) a->on = false; }
    gfx_background();
    switch (U.scene) {
    case SC_SPLASH: sc_splash(); break;
    case SC_MENU: sc_menu(); break;
    case SC_OPTIONS: sc_options(); break;
    case SC_RULES: sc_rules(); break;
    case SC_PRIVACY: sc_privacy(); break;
    case SC_BUILDER: sc_builder(); break;
    case SC_REVEAL: sc_reveal(); break;
    case SC_MAP: sc_map(); break;
    case SC_DEPLOY: sc_deploy(); break;
    case SC_BATTLE: sc_battle(); break;
    case SC_VICTORY: sc_victory(); break;
    case SC_LEVEL: sc_level(); break;
    }
    draw_particles();
    if (U.holding) {
        float p = fminf(1, (NOW - U.hold_start) / 550.0f);
        if (p > 0.12f) gfx_arc(CX, CY, 175, -1.5708f, -1.5708f + 6.2832f * p, 4, C_GOLD, 200);
    }
}

#ifdef UI_DEBUG
void ui_debug(int *scene, int *mode, int *dice_phase) { *scene = U.scene; *mode = U.mode; *dice_phase = U.dice.phase; }
int ui_debug_menu(void) { return U.menu_idx; }
int ui_debug_level(void) { return U.lvl_idx; }
int ui_debug_ai_turn(void) { return ai_turn(); }
int ui_debug_ai_fallbacks(void) { return AI_FALLBACKS; }
int ui_debug_winner(void) { return U.g.winner; }
int ui_debug_bidx(void) { return U.b_idx; }
int ui_debug_mana(void) { return RULES.mana_budget - army_cost(&U.armies[U.b_player]); }
void ui_debug_dice(int *stage, int *page) { *stage = U.dice.stage; *page = U.dice.page; }
void ui_debug_load(const game_t *g) { U.g = *g; U.has_game = true; layout_board(U.g.w, U.g.h); U.last_seq = U.g.seq; go(SC_BATTLE); U.last_current = U.g.current; U.last_round = U.g.round; U.sel_idx = 0; enter_select(); }
void ui_debug_builder(int idx) { go(SC_BUILDER); U.b_idx = idx; U.b_player = 0; }
void ui_debug_inspect(int id) { U.inspect_id = id; U.mode = M_INSPECT; }
void ui_debug_cut(int uid, int tid) { const unit_t *u = unit_by_id_c(&U.g, uid), *t = tid >= 0 ? unit_by_id_c(&U.g, tid) : NULL; start_cut(u, t, 3, 0, 0, tid); }
int ui_debug_chain(void) { return U.mode == M_DICE && U.dice.phase == 2 && U.dice.stage == 1 ? U.pending.nchain : 0; }
int ui_debug_tgt_unit(void) { return U.mode == M_TARGET && U.ntargets && U.targets[U.tgt_idx].unit_id >= 0 && hostile_action(U.action_id); }
#endif
