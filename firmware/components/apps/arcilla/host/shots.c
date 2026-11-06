/* Capturas de pantallas concretas (fichas y estados especiales) para revisar la interfaz. */
#include <stdio.h>
#include <stdlib.h>
#include "engine.h"
#include "gfx.h"
#include "ui.h"
void ui_debug_load(const game_t *g); void ui_debug_cut(int uid, int tid); void ui_debug_builder(int idx); void ui_debug_inspect(int id);
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
static void save(const char *dir, const char *name) {
    char path[256]; snprintf(path, sizeof path, "%s/%s.ppm", dir, name);
    FILE *f = fopen(path, "wb"); fprintf(f, "P6 %d %d 255\n", SCR, SCR);
    for (int i = 0; i < SCR * SCR; i++) { uint16_t c = FB[i]; int dx = i % SCR - 180, dy = i / SCR - 180; unsigned char px[3];
        if (dx * dx + dy * dy > 180 * 180) px[0] = px[1] = px[2] = 0; else { px[0] = (c >> 11) << 3; px[1] = ((c >> 5) & 63) << 2; px[2] = (c & 31) << 3; }
        fwrite(px, 1, 3, f); }
    fclose(f);
}
static void tick(int ms) { for (int t = 0; t < ms; t += 33) { T_MS += 33; ui_frame(); } }
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "."; int lang = argc > 2 ? atoi(argv[2]) : 0; char n[32];
    gfx_init(FB, BG); eng_set_lang(lang); ui_init(lang, true, 0, 0);
    int show[] = {C_SOLDIER, C_HUNTER, C_STALKER, C_LARVA, C_SHAMAN, C_GOLEM, C_BEAST, C_COLOSSUS, C_WYRMLING};
    for (int i = 0; i < 9; i++) { ui_debug_builder(show[i]); tick(300); snprintf(n, sizeof n, "b_%d", show[i]); save(dir, n); }
    static game_t g; army_t a0 = {0}, a1 = {0};
    int c0[] = {C_GOLEM, C_SOLDIER, C_HUNTER, C_LARVA}, c1[] = {C_BEAST, C_LARVA, C_STALKER, C_SHAMAN};
    for (int i = 0; i < 4; i++) { army_add(&a0, c0[i]); army_add(&a1, c1[i]); }
    game_new(&g, 0, &a0, &a1); auto_place_rest(&g);
    /* Golem con escudo junto al Guerrero, Stalker oculto */
    unit_t *golem = &g.units[0]; golem->walling = true;
    for (int i = 0; i < g.nunits; i++) if (g.units[i].cid == C_STALKER) g.units[i].hidden = true;
    ui_debug_load(&g); tick(600); save(dir, "board");
    for (int i = 0; i < g.nunits; i++) if (g.units[i].cid == C_STALKER) { ui_debug_inspect(g.units[i].id); tick(200); save(dir, "inspect_stalker"); }
    for (int i = 0; i < g.nunits; i++) if (g.units[i].cid == C_HUNTER) { ui_debug_inspect(g.units[i].id); tick(200); save(dir, "inspect_hunter"); }
    ui_debug_load(&g); tick(300);
    { int a = -1, t = -1; for (int i = 0; i < g.nunits; i++) { if (g.units[i].cid == C_SOLDIER) a = g.units[i].id; if (g.units[i].cid == C_BEAST) t = g.units[i].id; }
      ui_debug_cut(a, t); tick(120); save(dir, "cut_1"); tick(400); save(dir, "cut_2"); tick(500); save(dir, "cut_3"); tick(800); }
    { int a = -1; for (int i = 0; i < g.nunits; i++) if (g.units[i].cid == C_STALKER) a = g.units[i].id;
      ui_debug_cut(a, -1); tick(700); save(dir, "cut_4"); tick(900); }
    { int a = -1, t = -1; for (int i = 0; i < g.nunits; i++) { if (g.units[i].cid == C_SHAMAN) a = g.units[i].id; if (g.units[i].cid == C_GOLEM) t = g.units[i].id; }
      ui_debug_cut(a, t); tick(150); save(dir, "cut_5"); tick(700); save(dir, "cut_6"); tick(900); }
    return 0;
}
