/*
 * Contador — la app de ejemplo mas simple, y la prueba de que los datos sobreviven.
 *
 *   perilla: sube o baja   ·   tocar: +1   ·   mantener el dedo: volver a 0
 *
 * El numero se guarda en NVS (namespace "counter"). Reinicia la placa o flashea
 * un firmware nuevo con `idf.py flash`: el numero sigue ahi.
 *
 * Copia esta carpeta como punto de partida para una app nueva (ver docs/nueva-app.md).
 */
#include "app.h"
#include "sys.h"
#include "storage.h"
#include "biddy_fonts.h"

#define COUNTER_VERSION "0.1.0"
#define APP_ID "counter"
#define ACCENT 0xF5A623
#define SAVE_DELAY_MS 1000      /* guardar 1 s despues del ultimo cambio (cuida la flash) */

static lv_obj_t *s_value, *s_note;
static int32_t s_count;
static int32_t s_saved;
static uint32_t s_dirty_ms;     /* tiempo desde el ultimo cambio sin guardar */
static bool s_dirty;

static void show(void)
{
    lv_label_set_text_fmt(s_value, "%ld", (long)s_count);
}

static void changed(void)
{
    s_dirty = true;
    s_dirty_ms = 0;
    show();
}

static void save_now(void)
{
    if (s_dirty && s_count != s_saved) {
        storage_set_i32(APP_ID, "count", s_count);
        s_saved = s_count;
    }
    s_dirty = false;
}

static void counter_enter(lv_obj_t *scr)
{
    s_count = 0;
    storage_get_i32(APP_ID, "count", &s_count);   /* si no existe, se queda en 0 */
    s_saved = s_count;
    s_dirty = false;

    lv_obj_t *title = lv_label_create(scr);
    lv_obj_set_style_text_font(title, &biddy_font_22, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(ACCENT), 0);
    lv_label_set_text(title, "Contador");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 56);

    s_value = lv_label_create(scr);
    lv_obj_set_style_text_font(s_value, &biddy_font_digits_84, 0);
    lv_obj_set_style_text_color(s_value, lv_color_white(), 0);
    lv_obj_center(s_value);

    s_note = lv_label_create(scr);
    lv_obj_set_style_text_font(s_note, &biddy_font_16, 0);
    lv_obj_set_style_text_color(s_note, lv_color_hex(0x5C6578), 0);
    lv_obj_set_style_text_align(s_note, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_note, "Se guarda solo.\nReinicia o actualiza:\nsigue aquí.");
    lv_obj_align(s_note, LV_ALIGN_BOTTOM_MID, 0, -46);

    show();
}

static void counter_knob(int dir)
{
    s_count += dir;
    changed();
}

static void counter_touch(const touch_event_t *ev)
{
    if (ev->type == TOUCH_TAP) {
        s_count++;
        changed();
    } else if (ev->type == TOUCH_LONG_PRESS) {
        s_count = 0;
        sys_haptic(10);
        changed();
    }
}

static void counter_tick(uint32_t dt_ms)
{
    if (!s_dirty) return;
    s_dirty_ms += dt_ms;
    if (s_dirty_ms >= SAVE_DELAY_MS) save_now();
}

static void counter_exit(void)
{
    save_now();
}

const app_t app_counter = {
    .id = APP_ID,
    .name = "Contador",
    .version = COUNTER_VERSION,
    .icon = LV_SYMBOL_PLUS,
    .color = ACCENT,
    .tick_ms = 100,
    .on_enter = counter_enter,
    .on_knob = counter_knob,
    .on_touch = counter_touch,
    .on_tick = counter_tick,
    .on_exit = counter_exit,
};
