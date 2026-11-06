/*
 * launcher.c — menu circular de Hey Biddy.
 *
 * Los iconos de las apps forman un aro. La app elegida siempre queda arriba,
 * mas grande; al girar la perilla el aro gira. En el centro se ve el nombre.
 *   - perilla: elegir
 *   - tocar el centro: abrir la elegida
 *   - tocar un icono del aro: abrir esa app
 *   - deslizar izquierda/derecha: elegir (por si no usas la perilla)
 */
#include <math.h>
#include <stdio.h>
#include <time.h>
#include "launcher.h"
#include "sys.h"
#include "biddy_fonts.h"

#define CX 180
#define CY 180
#define RING_R 138
#define ICON_MIN 46
#define ICON_MAX 72
#define MAX_ICONS 24

static lv_obj_t *s_icons[MAX_ICONS];
static lv_obj_t *s_name, *s_ver, *s_hint, *s_time;
static int s_sel;              /* sin limite (puede ser negativo); la app es s_sel mod N */
static float s_offset;         /* angulo actual del aro, en grados */
static int s_n;

static int wrap(int i)
{
    return ((i % s_n) + s_n) % s_n;
}

static float step_deg(void)
{
    return s_n <= 12 ? 360.0f / s_n : 30.0f;
}

static void layout(void)
{
    float step = step_deg();
    for (int i = 0; i < s_n; i++) {
        /* angulo relativo a "arriba", en (-180, 180] */
        float rel = fmodf(i * step + s_offset, 360.0f);
        if (rel > 180.0f) rel -= 360.0f;
        if (rel <= -180.0f) rel += 360.0f;
        float d = fabsf(rel);
        float t = 1.0f - d / step;
        if (t < 0) t = 0;

        lv_obj_t *o = s_icons[i];
        if (s_n > 12 && d > 165.0f) {
            lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);

        int size = ICON_MIN + (int)((ICON_MAX - ICON_MIN) * t);
        float a = (rel - 90.0f) * (float)M_PI / 180.0f;
        int x = CX + (int)lroundf(RING_R * cosf(a)) - size / 2;
        int y = CY + (int)lroundf(RING_R * sinf(a)) - size / 2;
        lv_obj_set_size(o, size, size);
        lv_obj_set_pos(o, x, y);
        lv_obj_set_style_bg_opa(o, (lv_opa_t)(LV_OPA_40 + (LV_OPA_COVER - LV_OPA_40) * t), 0);
        lv_obj_set_style_border_width(o, t > 0.8f ? 3 : 0, 0);
    }
}

static void show_selected(void)
{
    if (s_n == 0) return;
    const app_t *a = sys_app_at(wrap(s_sel));
    lv_label_set_text(s_name, a->name);
    lv_label_set_text_fmt(s_ver, "v%s", a->version);
    lv_obj_set_style_text_color(s_name, lv_color_hex(a->color), 0);
}

static void update_time(void)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (tm.tm_year + 1900 >= 2025) {
        lv_label_set_text_fmt(s_time, "%02d:%02d", tm.tm_hour, tm.tm_min);
    } else {
        lv_label_set_text(s_time, "Hey Biddy");
    }
}

static void select_delta(int d)
{
    if (s_n == 0) return;
    s_sel += d;
    show_selected();
}

static void open_selected(void)
{
    if (s_n == 0) return;
    sys_haptic(1);
    sys_open_app(sys_app_at(wrap(s_sel))->id);
}

/* ------------------------------------------------------- contrato */

static void launcher_enter(lv_obj_t *screen)
{
    update_time();
    show_selected();
    layout();
}

static void launcher_knob(int dir)
{
    select_delta(dir);
}

static void launcher_touch(const touch_event_t *ev)
{
    switch (ev->type) {
    case TOUCH_TAP: {
        /* ¿toco un icono del aro? */
        for (int i = 0; i < s_n; i++) {
            lv_obj_t *o = s_icons[i];
            if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) continue;
            int r = lv_obj_get_width(o) / 2 + 6;
            int ix = lv_obj_get_x(o) + lv_obj_get_width(o) / 2;
            int iy = lv_obj_get_y(o) + lv_obj_get_height(o) / 2;
            int dx = ev->x - ix, dy = ev->y - iy;
            if (dx * dx + dy * dy <= r * r) {
                sys_haptic(1);
                sys_open_app(sys_app_at(i)->id);
                return;
            }
        }
        int dx = ev->x - CX, dy = ev->y - CY;
        if (dx * dx + dy * dy <= 100 * 100) open_selected();
        break;
    }
    case TOUCH_SWIPE_LEFT:  select_delta(+1); sys_haptic_click(); break;
    case TOUCH_SWIPE_RIGHT: select_delta(-1); sys_haptic_click(); break;
    default: break;
    }
}

static void launcher_tick(uint32_t dt_ms)
{
    if (s_n == 0) return;
    float target = -s_sel * step_deg();
    float diff = target - s_offset;
    if (fabsf(diff) > 0.3f) {
        s_offset += diff * 0.35f;              /* suaviza el giro */
        layout();
    } else if (diff != 0.0f) {
        s_offset = target;
        layout();
    }
    static uint32_t acc;
    acc += dt_ms;
    if (acc >= 1000) {
        acc = 0;
        update_time();
    }
}

static const app_t s_launcher = {
    .id = "launcher",
    .name = "Menú",
    .version = "0.1.0",
    .icon = LV_SYMBOL_HOME,
    .color = 0xFFFFFF,
    .on_enter = launcher_enter,
    .on_knob = launcher_knob,
    .on_touch = launcher_touch,
    .on_tick = launcher_tick,
};

const app_t *launcher_app(void)
{
    return &s_launcher;
}

void launcher_build(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x070B14), 0);

    /* aro de fondo */
    lv_obj_t *ring = lv_obj_create(scr);
    lv_obj_remove_style_all(ring);
    lv_obj_set_size(ring, RING_R * 2, RING_R * 2);
    lv_obj_center(ring);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ring, 2, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(0x1C2436), 0);
    lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);

    s_time = lv_label_create(scr);
    lv_obj_set_style_text_font(s_time, &biddy_font_22, 0);
    lv_obj_set_style_text_color(s_time, lv_color_hex(0x8A93A6), 0);
    lv_obj_align(s_time, LV_ALIGN_CENTER, 0, -58);

    s_name = lv_label_create(scr);
    lv_obj_set_style_text_font(s_name, &biddy_font_32, 0);
    lv_obj_align(s_name, LV_ALIGN_CENTER, 0, -8);

    s_ver = lv_label_create(scr);
    lv_obj_set_style_text_font(s_ver, &biddy_font_16, 0);
    lv_obj_set_style_text_color(s_ver, lv_color_hex(0x5C6578), 0);
    lv_obj_align(s_ver, LV_ALIGN_CENTER, 0, 26);

    s_hint = lv_label_create(scr);
    lv_obj_set_style_text_font(s_hint, &biddy_font_16, 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(0x5C6578), 0);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_hint, "Gira para elegir\ntoca para abrir");
    lv_obj_align(s_hint, LV_ALIGN_CENTER, 0, 66);

    s_n = (int)sys_app_count();
    if (s_n > MAX_ICONS) s_n = MAX_ICONS;
    if (s_n == 0) {
        lv_label_set_text(s_name, "Sin apps");
        lv_label_set_text(s_ver, "");
        return;
    }

    for (int i = 0; i < s_n; i++) {
        const app_t *a = sys_app_at(i);
        lv_obj_t *o = lv_obj_create(scr);
        lv_obj_remove_style_all(o);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(o, lv_color_hex(a->color), 0);
        lv_obj_set_style_border_color(o, lv_color_white(), 0);
        lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);

        lv_obj_t *l = lv_label_create(o);
        lv_obj_set_style_text_font(l, &biddy_font_22, 0);
        lv_obj_set_style_text_color(l, lv_color_white(), 0);
        lv_label_set_text(l, a->icon ? a->icon : LV_SYMBOL_FILE);
        lv_obj_center(l);
        s_icons[i] = o;
    }
    s_sel = 0;
    s_offset = 0;
}
