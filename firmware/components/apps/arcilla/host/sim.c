/* Simulador en PC: corre la interfaz con un reloj virtual y entradas al azar; guarda capturas. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "gfx.h"
#include "ui.h"
void ui_debug(int *scene, int *mode, int *dice_phase);
int ui_debug_menu(void); int ui_debug_bidx(void); int ui_debug_mana(void); int ui_debug_level(void); int ui_debug_ai_turn(void); int ui_debug_ai_fallbacks(void); int ui_debug_winner(void);
void ui_debug_dice(int *stage, int *page); int ui_debug_tgt_unit(void); int ui_debug_chain(void);
static uint32_t T_MS = 1000;
uint32_t hal_ms(void) { return T_MS; }
void hal_haptic(haptic_t k) { (void)k; }
void hal_save_settings(lang_t l, bool h) { (void)l; (void)h; }
void hal_exit(void) {}
bool hal_load(const char *k, void *d, int n) { (void)k; (void)d; (void)n; return false; }
void hal_store(const char *k, const void *d, int n) { (void)k; (void)d; (void)n; }
void hal_think_async(void (*fn)(void)) { fn(); }
uint8_t platform_random_byte(void) { return (uint8_t)(rand() & 0xFF); }
static uint16_t FB[SCR * SCR], BG[SCR * SCR];
static const char *DIR;
static void save(const char *name) {
  char path[256]; snprintf(path, sizeof path, "%s/%s.ppm", DIR, name);
  FILE *f = fopen(path, "wb"); fprintf(f, "P6 %d %d 255\n", SCR, SCR);
  for (int i = 0; i < SCR * SCR; i++) { uint16_t c = FB[i]; int dx = i % SCR - 180, dy = i / SCR - 180; unsigned char px[3];
    if (dx * dx + dy * dy > 180 * 180) { px[0] = px[1] = px[2] = 0; } else { px[0] = (c >> 11) << 3; px[1] = ((c >> 5) & 63) << 2; px[2] = (c & 31) << 3; }
    fwrite(px, 1, 3, f); }
  fclose(f);
}
static void tick(int ms) { for (int t = 0; t < ms; t += 33) { T_MS += 33; ui_frame(); } }
static void in(input_t e, int wait) { ui_input(e); tick(wait); }
static int scene(void) { int s, m, d; ui_debug(&s, &m, &d); return s; }
static void menu_to(int idx) { while (ui_debug_menu() != idx) in(IN_R, 60); }
static void build_army(const char *tag) {
  /* elige criaturas al azar hasta que no alcance el maná, luego LISTO */
  for (int k = 0; k < 12; k++) { int want = rand() % C_COUNT; while (ui_debug_bidx() != want) in(IN_R, 40); in(IN_TAP, 80); if (ui_debug_mana() < 2) break; }
  if (tag) save(tag);
  while (ui_debug_bidx() != C_COUNT) in(IN_R, 40);
  in(IN_TAP, 500);
}
int main(int argc, char **argv) {
  DIR = argc > 1 ? argv[1] : ".";
  int lang = argc > 2 ? atoi(argv[2]) : 0; unsigned seed = argc > 3 ? (unsigned)atoi(argv[3]) : 1; int shots = argc > 4 ? atoi(argv[4]) : 1; int map = argc > 5 ? atoi(argv[5]) : 0;
  int vs = argc > 6 ? atoi(argv[6]) : -1;   /* -1 = dos jugadores · 0, 1, 2 = contra Biddy en Fácil, Medio, Difícil */
  srand(seed);
  gfx_init(FB, BG); int tot; eng_set_lang(lang); int pass = run_tests(NULL, &tot);
  ui_init(lang, true, pass, tot);
  tick(800); if (shots) save("01_splash");
  in(IN_TAP, 300); if (shots) save("02_menu");
  menu_to(3); in(IN_TAP, 300); if (shots) save("03_options"); in(IN_LONG, 300);
  menu_to(2); in(IN_TAP, 300); in(IN_R, 100); in(IN_R, 100); if (shots) save("04_rules"); in(IN_LONG, 300);
  if (vs >= 0) {
    menu_to(0); in(IN_TAP, 400);
    while (ui_debug_level() != vs) in(IN_R, 80);
    tick(300); if (shots) save("05_level");
    in(IN_TAP, 400);
    build_army(shots ? "06_builder" : NULL);
    tick(900); if (shots) save("08_reveal"); tick(800); in(IN_TAP, 300);
  } else {
    menu_to(1); in(IN_TAP, 500); if (shots) save("05_privacy"); in(IN_TAP, 300);
    build_army(shots ? "06_builder" : NULL);
    if (shots) save("07_privacy_pass"); in(IN_TAP, 300);
    build_army(NULL); in(IN_TAP, 900); if (shots) save("08_reveal"); tick(800); in(IN_TAP, 300);
  }
  for (int k = 0; k < map; k++) in(IN_R, 60);
  if (shots) save("09_map"); in(IN_TAP, 400); if (shots) save("10_deploy");
  if (vs >= 0) {
    /* despliegue de uno en uno: tú colocas, Biddy muestra dónde y coloca */
    in(IN_TAP, 120); tick(200); if (shots) save("10a_biddy_elige"); tick(500); if (shots) save("10b_biddy_coloca"); tick(500); if (shots) save("10c_tu_turno");
    in(IN_TAP, 1300);
  }
  in(IN_LONG, 1500); if (shots) save("11_battle");
  int ai_shots = 0;
  int seen[16] = {0}; char name[64];
  for (int step = 0; step < 20000; step++) {
    int sc, mode, dp; ui_debug(&sc, &mode, &dp);
    if (sc == 10) { tick(1600); if (shots) save("99_victory"); printf("seed %u: victoria en paso %d · ganador %d · atajos de la IA %d\n", seed, step, ui_debug_winner(), ui_debug_ai_fallbacks()); return 0; }
    if (sc != 9) { printf("seed %u: escena inesperada %d en paso %d\n", seed, sc, step); return 1; }
    if (ui_debug_ai_turn()) {
      /* turno de Biddy: solo se mira. Capturas de lo que hace. */
      if (shots && ai_shots < 6 && step % 7 == 0) { snprintf(name, sizeof name, "ai_%d", ai_shots++); save(name); }
      tick(100); continue;
    }
    int key = mode == 9 ? 11 + dp : mode;
    if (shots && !seen[key] && step > 3) { seen[key] = 1; tick(250); snprintf(name, sizeof name, "m%02d", key); save(name); }
    int r = rand() % 100;
    if (mode == 5) { in(IN_TAP, 200); continue; }                       /* pausa → seguir */
    if (mode == 9) {
      int st, pg; ui_debug_dice(&st, &pg);
      if (dp == 0) {
        static int shown = 0;
        if (shots && !shown) { shown = 1; for (int k = 0; k < 16; k++) in(IN_R, 40); save("dice_1_charge"); for (int k = 0; k < 2; k++) in(IN_R, 40); tick(200); save("dice_2_max");
          in(IN_TAP, 260); save("dice_3_roll"); tick(400); save("dice_4_roll"); continue; }
        for (int k = 0; k < 6; k++) in(IN_R, 30); in(IN_TAP, 100); continue;
      }
      if (dp == 2 && ui_debug_chain()) {
        static int chain_shot = 0;
        if (!chain_shot) { chain_shot = 1; tick(700); save("chain_result"); in(IN_R, 200); save("chain_breakdown"); printf("CHAIN\n"); }
      }
      if (dp == 2) {
        static int bd[2] = {0, 0}, land[2] = {0, 0};
        if (shots && !land[st]) { land[st] = 1; tick(120); snprintf(name, sizeof name, "dice_5_land_%d", st); save(name); tick(600); snprintf(name, sizeof name, "dice_6_result_%d", st); save(name); }
        if (shots && !bd[st] && pg == 0) { bd[st] = 1; tick(500); in(IN_R, 200); snprintf(name, sizeof name, "dice_7_breakdown_%d", st); save(name); continue; }
        in(IN_TAP, 500); continue;
      }
      tick(60); continue;
    }
    if (mode == 7 && shots && ui_debug_tgt_unit() && !seen[15]) { seen[15] = 1; tick(200); save("target_odds"); }
    if (mode == 10) { tick(300); continue; }
    if (r < 60) in(IN_TAP, 160); else if (r < 94) in(IN_R, 80); else if (r < 97) in(IN_L, 80); else in(IN_LONG, 200);
  }
  printf("seed %u: sin terminar\n", seed);
  return 1;
}
