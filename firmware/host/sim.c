/*
 * Simulador de PC: dibuja las pantallas del menu y de las apps con el mismo LVGL
 * y guarda capturas en PNG (via PPM). Sirve para revisar el diseño sin la placa.
 *
 * Uso (Linux o WSL, desde firmware/):  bash host/sim.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lvgl.h"
#include "sys.h"
#include "storage.h"
#include "launcher.h"
#include "registry.h"

/* ---------------- almacenamiento en memoria ---------------- */
typedef struct { char ns[16], key[16]; int32_t v; } kv_t;
static kv_t kv[64];
static int nkv;

esp_err_t storage_init(void) { return ESP_OK; }
esp_err_t storage_get_i32(const char *ns, const char *key, int32_t *out)
{
    for (int i = 0; i < nkv; i++)
        if (!strcmp(kv[i].ns, ns) && !strcmp(kv[i].key, key)) { *out = kv[i].v; return ESP_OK; }
    return ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t storage_set_i32(const char *ns, const char *key, int32_t v)
{
    for (int i = 0; i < nkv; i++)
        if (!strcmp(kv[i].ns, ns) && !strcmp(kv[i].key, key)) { kv[i].v = v; return ESP_OK; }
    snprintf(kv[nkv].ns, 16, "%s", ns);
    snprintf(kv[nkv].key, 16, "%s", key);
    kv[nkv++].v = v;
    return ESP_OK;
}
esp_err_t storage_get_str(const char *ns, const char *k, char *o, size_t l) { return ESP_ERR_NVS_NOT_FOUND; }
esp_err_t storage_set_str(const char *ns, const char *k, const char *v) { return ESP_OK; }
esp_err_t storage_get_blob(const char *ns, const char *k, void *o, size_t *l) { return ESP_ERR_NVS_NOT_FOUND; }
esp_err_t storage_set_blob(const char *ns, const char *k, const void *d, size_t l) { return ESP_OK; }
esp_err_t storage_erase_key(const char *ns, const char *k) { return ESP_OK; }
esp_err_t storage_erase_all(const char *ns) { return ESP_OK; }

/* ---------------- sistema minimo ---------------- */
static const app_t *active, *pending;
static lv_obj_t *launcher_scr, *app_scr;
static uint8_t bright = 80;
static bool haptic = true;

void sys_go_home(void) { pending = launcher_app(); }
bool sys_open_app(const char *id)
{
    for (size_t i = 0; i < g_app_count; i++)
        if (!strcmp(g_apps[i]->id, id)) { pending = g_apps[i]; return true; }
    return false;
}
void sys_haptic(uint8_t e) {}
void sys_haptic_click(void) {}
bool sys_run_on_ui(sys_ui_fn_t fn, void *arg) { fn(arg); return true; }
void sys_raw_draw(int x, int y, int w, int h, const uint16_t *p) {}
uint8_t sys_get_brightness(void) { return bright; }
void sys_set_brightness(uint8_t p) { bright = p; }
bool sys_get_haptic_enabled(void) { return haptic; }
void sys_set_haptic_enabled(bool on) { haptic = on; }
uint32_t sys_get_boot_count(void) { return 7; }
const char *sys_firmware_version(void) { return "0.1.0"; }
size_t sys_app_count(void) { return g_app_count; }
const app_t *sys_app_at(size_t i) { return i < g_app_count ? g_apps[i] : NULL; }

/* ---------------- pantalla en memoria ---------------- */
static lv_color_t fb[360 * 360];
static void flush(lv_disp_drv_t *d, const lv_area_t *a, lv_color_t *px)
{
    for (int y = a->y1; y <= a->y2; y++)
        for (int x = a->x1; x <= a->x2; x++)
            fb[y * 360 + x] = *px++;
    lv_disp_flush_ready(d);
}

static void switch_now(void)
{
    if (!pending || pending == active) { pending = NULL; return; }
    const app_t *next = pending;
    pending = NULL;
    if (active && active->on_exit) active->on_exit();
    lv_obj_t *old = app_scr;
    app_scr = NULL;
    lv_obj_t *scr;
    if (next == launcher_app()) scr = launcher_scr;
    else {
        scr = lv_obj_create(NULL);
        lv_obj_remove_style_all(scr);
        lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
        app_scr = scr;
    }
    lv_scr_load(scr);
    if (old) lv_obj_del(old);
    active = next;
    if (next->on_enter) next->on_enter(scr);
}

static void run(int ms)
{
    for (int t = 0; t < ms; t += 10) {
        switch_now();
        lv_tick_inc(10);
        static int acc;
        acc += 10;
        uint32_t per = active->tick_ms ? active->tick_ms : 33;
        if (acc >= (int)per) { if (active->on_tick) active->on_tick(acc); acc = 0; }
        lv_timer_handler();
    }
}

static void knob(int d) { if (active->on_knob) active->on_knob(d); run(400); }
static void touch(touch_type_t t, int x, int y)
{
    touch_event_t e = {.type = t, .x = x, .y = y};
    if (active->on_touch) active->on_touch(&e);
    run(400);
}

static void shot(const char *name)
{
    lv_obj_invalidate(lv_scr_act());
    run(50);
    char path[256];
    snprintf(path, sizeof(path), "%s.ppm", name);
    FILE *f = fopen(path, "wb");
    fprintf(f, "P6\n360 360\n255\n");
    for (int y = 0; y < 360; y++)
        for (int x = 0; x < 360; x++) {
            int dx = x - 180, dy = y - 180;
            unsigned char rgb[3] = {200, 200, 200};
            if (dx * dx + dy * dy <= 180 * 180) {   /* la pantalla es redonda */
                lv_color_t c = fb[y * 360 + x];
                rgb[0] = c.ch.red << 3; rgb[1] = c.ch.green << 2; rgb[2] = c.ch.blue << 3;
            }
            fwrite(rgb, 1, 3, f);
        }
    fclose(f);
    printf("captura: %s\n", path);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    char p[256];
    lv_init();
    static lv_disp_draw_buf_t buf;
    static lv_color_t b1[360 * 360];
    lv_disp_draw_buf_init(&buf, b1, NULL, 360 * 360);
    static lv_disp_drv_t drv;
    lv_disp_drv_init(&drv);
    drv.hor_res = 360; drv.ver_res = 360; drv.flush_cb = flush; drv.draw_buf = &buf;
    lv_disp_drv_register(&drv);

    launcher_scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(launcher_scr);
    lv_obj_set_style_bg_opa(launcher_scr, LV_OPA_COVER, 0);
    launcher_build(launcher_scr);
    pending = launcher_app();
    run(100);

    snprintf(p, sizeof p, "%s/01_menu", dir); shot(p);
    knob(+1);
    snprintf(p, sizeof p, "%s/02_menu_girado", dir); shot(p);
    touch(TOUCH_TAP, 180, 180);           /* abre Contador */
    knob(+1); knob(+1); knob(+1);
    snprintf(p, sizeof p, "%s/03_contador", dir); shot(p);
    sys_go_home(); run(100);
    knob(-1);                              /* vuelve a Reloj */
    touch(TOUCH_TAP, 180, 180);
    snprintf(p, sizeof p, "%s/04_reloj_sin_hora", dir); shot(p);
    touch(TOUCH_LONG_PRESS, 180, 180);
    knob(+1);
    snprintf(p, sizeof p, "%s/05_reloj_editando", dir); shot(p);
    touch(TOUCH_TAP, 180, 180); touch(TOUCH_TAP, 180, 180);
    run(1200);
    snprintf(p, sizeof p, "%s/06_reloj", dir); shot(p);
    sys_open_app("settings"); run(100);
    snprintf(p, sizeof p, "%s/07_ajustes", dir); shot(p);
    touch(TOUCH_TAP, 180, 180);
    snprintf(p, sizeof p, "%s/08_ajustes_brillo", dir); shot(p);
    touch(TOUCH_TAP, 180, 180);
    knob(+1); knob(+1); knob(+1); knob(+1); knob(+1); knob(+1);
    snprintf(p, sizeof p, "%s/09_ajustes_apps", dir); shot(p);
    return 0;
}
