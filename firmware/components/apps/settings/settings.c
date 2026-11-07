/*
 * Ajustes — brillo, vibracion y la informacion del sistema y de cada app.
 *
 *   perilla: moverse entre filas
 *   tocar en "Brillo": entrar/salir de edicion (la perilla cambia el valor)
 *   tocar en "Vibración": encender/apagar
 *   tocar en "Pantalla oscura": entrar/salir de edicion (la perilla elige el tiempo del standby)
 */
#include <stdio.h>
#include "app.h"
#include "sys.h"
#include "biddy_fonts.h"
#include "esp_heap_caps.h"

#define SETTINGS_VERSION "0.2.0"
#define ACCENT 0x9B8CFF
#define ROW_GAP 48
#define MAX_ROWS 40

enum { ROW_BRIGHTNESS, ROW_HAPTIC, ROW_STANDBY, ROW_WIFI, ROW_FIRMWARE, ROW_BOOTS, ROW_MEMORY, ROW_APPS_HEADER, ROW_FIRST_APP };

static lv_obj_t *s_rows[MAX_ROWS];
static int s_nrows;
static int s_sel;
static bool s_editing;
static lv_obj_t *s_title;

/* opciones del standby, en segundos (0 = nunca) */
static const uint16_t STANDBY_OPTS[] = {15, 30, 60, 120, 300, 600, 0};
#define N_STANDBY_OPTS (int)(sizeof(STANDBY_OPTS) / sizeof(STANDBY_OPTS[0]))

static void standby_text(uint16_t sec, char *buf, size_t len)
{
    if (sec == 0) snprintf(buf, len, "nunca");
    else if (sec < 60) snprintf(buf, len, "%u s", sec);
    else snprintf(buf, len, "%u min", sec / 60);
}

static int standby_index(void)
{
    uint16_t cur = sys_get_standby_s();
    for (int i = 0; i < N_STANDBY_OPTS; i++)
        if (STANDBY_OPTS[i] == cur) return i;
    return 2;   /* 1 min */
}

static void row_text(int i, char *buf, size_t len)
{
    switch (i) {
    case ROW_BRIGHTNESS:
        snprintf(buf, len, (s_editing && s_sel == i) ? "Brillo  < %d %% >" : "Brillo  %d %%", sys_get_brightness());
        break;
    case ROW_HAPTIC:
        snprintf(buf, len, "Vibración  %s", sys_get_haptic_enabled() ? "Sí" : "No");
        break;
    case ROW_STANDBY: {
        char t[16];
        standby_text(sys_get_standby_s(), t, sizeof(t));
        snprintf(buf, len, (s_editing && s_sel == i) ? "Pantalla oscura  < %s >" : "Pantalla oscura  %s", t);
        break;
    }
    case ROW_WIFI:
        snprintf(buf, len, "Wi-Fi  %s%s", sys_net_connected() ? "conectado" : "sin conexión",
                 sys_time_synced() ? " · hora ok" : "");
        break;
    case ROW_FIRMWARE:
        snprintf(buf, len, "Firmware  %s", sys_firmware_version());
        break;
    case ROW_BOOTS:
        snprintf(buf, len, "Arranques  %lu", (unsigned long)sys_get_boot_count());
        break;
    case ROW_MEMORY:
        snprintf(buf, len, "RAM %u KB · PSRAM %u KB",
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
        break;
    case ROW_APPS_HEADER:
        snprintf(buf, len, "· Apps instaladas ·");
        break;
    default: {
        const app_t *a = sys_app_at(i - ROW_FIRST_APP);
        snprintf(buf, len, "%s  v%s", a->name, a->version);
        break;
    }
    }
}

static void layout(void)
{
    char buf[64];
    for (int i = 0; i < s_nrows; i++) {
        int off = i - s_sel;
        lv_obj_t *l = s_rows[i];
        if (off < -2 || off > 2) {
            lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(l, LV_OBJ_FLAG_HIDDEN);
        row_text(i, buf, sizeof(buf));
        lv_label_set_text(l, buf);
        bool sel = off == 0;
        lv_obj_set_style_text_font(l, sel ? &biddy_font_22 : &biddy_font_16, 0);
        lv_color_t c = sel ? (s_editing ? lv_color_hex(ACCENT) : lv_color_white()) : lv_color_hex(0x8A93A6);
        lv_obj_set_style_text_color(l, c, 0);
        lv_obj_set_style_text_opa(l, (off >= -1 && off <= 1) ? LV_OPA_COVER : LV_OPA_50, 0);
        lv_obj_align(l, LV_ALIGN_CENTER, 0, off * ROW_GAP);
    }
}

static void settings_enter(lv_obj_t *scr)
{
    s_sel = 0;
    s_editing = false;
    s_nrows = ROW_FIRST_APP + (int)sys_app_count();
    if (s_nrows > MAX_ROWS) s_nrows = MAX_ROWS;

    s_title = lv_label_create(scr);
    lv_obj_set_style_text_font(s_title, &biddy_font_16, 0);
    lv_obj_set_style_text_color(s_title, lv_color_hex(ACCENT), 0);
    lv_label_set_text(s_title, LV_SYMBOL_SETTINGS "  Ajustes");
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 22);

    /* barra que marca la fila elegida */
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 300, 40);
    lv_obj_center(bar);
    lv_obj_set_style_radius(bar, 20, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x151B28), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    for (int i = 0; i < s_nrows; i++) {
        s_rows[i] = lv_label_create(scr);
    }
    layout();
}

static void settings_knob(int dir)
{
    if (s_editing && s_sel == ROW_BRIGHTNESS) {
        int b = sys_get_brightness() + dir * 5;
        if (b < 5) b = 5;
        if (b > 100) b = 100;
        sys_set_brightness((uint8_t)b);
    } else if (s_editing && s_sel == ROW_STANDBY) {
        int i = standby_index() + dir;
        if (i < 0) i = 0;
        if (i >= N_STANDBY_OPTS) i = N_STANDBY_OPTS - 1;
        sys_set_standby_s(STANDBY_OPTS[i]);
    } else {
        s_sel += dir;
        if (s_sel < 0) s_sel = 0;
        if (s_sel >= s_nrows) s_sel = s_nrows - 1;
    }
    layout();
}

static void settings_touch(const touch_event_t *ev)
{
    if (ev->type == TOUCH_TAP) {
        if (s_sel == ROW_BRIGHTNESS || s_sel == ROW_STANDBY) {
            s_editing = !s_editing;
        } else if (s_sel == ROW_HAPTIC) {
            sys_set_haptic_enabled(!sys_get_haptic_enabled());
            sys_haptic(1);
        }
        layout();
    } else if (ev->type == TOUCH_SWIPE_UP || ev->type == TOUCH_SWIPE_DOWN) {
        if (!s_editing) settings_knob(ev->type == TOUCH_SWIPE_UP ? +1 : -1);
    }
}

static void settings_tick(uint32_t dt_ms)
{
    if (!s_editing) layout();   /* refresca Wi-Fi y memoria libre */
}

const app_t app_settings = {
    .id = "settings",
    .name = "Ajustes",
    .version = SETTINGS_VERSION,
    .icon = LV_SYMBOL_SETTINGS,
    .color = ACCENT,
    .tick_ms = 1000,
    .on_enter = settings_enter,
    .on_knob = settings_knob,
    .on_touch = settings_touch,
    .on_tick = settings_tick,
};
