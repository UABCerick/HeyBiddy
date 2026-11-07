/*
 * Reloj — hora grande, segundos en el aro, fecha e icono de Wi-Fi.
 *
 * La hora viene de internet (SNTP) en cuanto la placa se conecta al hotspot,
 * y sigue corriendo aunque se pierda el Wi-Fi.
 * Respaldo: si todavia no hay hora de internet, se pone a mano:
 *   mantener el dedo -> la perilla cambia la hora -> tocar -> la perilla cambia
 *   los minutos -> tocar -> guardado.
 * Si la placa se apaga sin Wi-Fi, la hora manual se pierde.
 */
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "app.h"
#include "sys.h"
#include "biddy_fonts.h"

#define CLOCK_VERSION "0.2.0"
#define ACCENT 0x3FA9F5
#define DIM    0x5C6578
#define OFF    0x2A3142

static const char *DIAS[] = {"domingo", "lunes", "martes", "miércoles", "jueves", "viernes", "sábado"};
static const char *MESES[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};

typedef enum { VIEW, EDIT_HOUR, EDIT_MIN } clock_mode_t;

static lv_obj_t *s_arc, *s_time, *s_date, *s_hint, *s_wifi;
static clock_mode_t s_mode;
static int s_eh, s_em;          /* hora y minutos mientras se edita */
static int s_last_sec = -1;
static int s_last_net = -1;     /* estado del icono la ultima vez que se dibujo */
static uint32_t s_msg_ms;       /* tiempo restante del aviso "Hora de internet" */

static bool time_is_set(const struct tm *tm)
{
    return tm->tm_year + 1900 >= 2025;
}

static void refresh_wifi(void)
{
    /* bit 0: hay Wi-Fi; bit 1: la hora vino de internet */
    int net = (sys_net_connected() ? 1 : 0) | (sys_time_synced() ? 2 : 0);
    if (net == s_last_net) return;
    s_last_net = net;
    s_last_sec = -1;                 /* redibujar la pista ("hora manual") ya */
    lv_obj_set_style_text_color(s_wifi, lv_color_hex((net & 1) ? ACCENT : OFF), 0);
}

static void refresh(void)
{
    refresh_wifi();

    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);

    if (s_mode != VIEW) {
        lv_label_set_text_fmt(s_time, "%02d:%02d", s_eh, s_em);
        lv_label_set_text(s_date, s_mode == EDIT_HOUR ? "Gira: hora" : "Gira: minutos");
        lv_label_set_text(s_hint, "Toca para seguir");
        /* resaltar el campo que se edita con el aro */
        lv_arc_set_value(s_arc, s_mode == EDIT_HOUR ? s_eh * 60 / 24 : s_em);
        return;
    }

    if (!time_is_set(&tm)) {
        lv_label_set_text(s_time, "--:--");
        lv_label_set_text(s_date, sys_net_connected() ? "Pidiendo la hora..." : "Hora sin ajustar");
        lv_label_set_text(s_hint, "Mantén el dedo para ponerla");
        lv_arc_set_value(s_arc, 0);
        return;
    }
    if (tm.tm_sec == s_last_sec) return;
    s_last_sec = tm.tm_sec;
    lv_label_set_text_fmt(s_time, "%02d:%02d", tm.tm_hour, tm.tm_min);
    lv_label_set_text_fmt(s_date, "%s %d %s", DIAS[tm.tm_wday], tm.tm_mday, MESES[tm.tm_mon]);
    if (s_msg_ms) lv_label_set_text(s_hint, "Hora de internet");
    else lv_label_set_text(s_hint, sys_time_synced() ? "" : "hora manual");
    lv_arc_set_value(s_arc, tm.tm_sec);
}

static void save_edited_time(void)
{
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    if (!time_is_set(&tm)) {
        /* sin fecha conocida: usamos una fecha base; el Wi-Fi la corregira despues */
        memset(&tm, 0, sizeof(tm));
        tm.tm_year = 2026 - 1900;
        tm.tm_mon = 0;
        tm.tm_mday = 1;
    }
    tm.tm_hour = s_eh;
    tm.tm_min = s_em;
    tm.tm_sec = 0;
    tm.tm_isdst = -1;
    struct timeval tv = {.tv_sec = mktime(&tm), .tv_usec = 0};
    settimeofday(&tv, NULL);
}

static void clock_enter(lv_obj_t *scr)
{
    s_mode = VIEW;
    s_last_sec = -1;
    s_last_net = -1;
    s_msg_ms = 0;

    s_arc = lv_arc_create(scr);
    lv_obj_set_size(s_arc, 340, 340);
    lv_obj_center(s_arc);
    lv_arc_set_rotation(s_arc, 270);
    lv_arc_set_bg_angles(s_arc, 0, 360);
    lv_arc_set_range(s_arc, 0, 59);
    lv_obj_remove_style(s_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(s_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(s_arc, 6, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_arc, 6, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(0x151B28), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(ACCENT), LV_PART_INDICATOR);

    s_wifi = lv_label_create(scr);
    lv_obj_set_style_text_font(s_wifi, &biddy_font_22, 0);
    lv_label_set_text(s_wifi, LV_SYMBOL_WIFI);
    lv_obj_align(s_wifi, LV_ALIGN_TOP_MID, 0, 52);

    s_time = lv_label_create(scr);
    lv_obj_set_style_text_font(s_time, &biddy_font_digits_84, 0);
    lv_obj_set_style_text_color(s_time, lv_color_white(), 0);
    lv_obj_align(s_time, LV_ALIGN_CENTER, 0, -14);

    s_date = lv_label_create(scr);
    lv_obj_set_style_text_font(s_date, &biddy_font_22, 0);
    lv_obj_set_style_text_color(s_date, lv_color_hex(ACCENT), 0);
    lv_obj_align(s_date, LV_ALIGN_CENTER, 0, 52);

    s_hint = lv_label_create(scr);
    lv_obj_set_style_text_font(s_hint, &biddy_font_16, 0);
    lv_obj_set_style_text_color(s_hint, lv_color_hex(DIM), 0);
    lv_obj_align(s_hint, LV_ALIGN_CENTER, 0, 92);

    refresh();
}

static void clock_knob(int dir)
{
    if (s_mode == EDIT_HOUR) s_eh = (s_eh + dir + 24) % 24;
    else if (s_mode == EDIT_MIN) s_em = (s_em + dir + 60) % 60;
    else return;
    refresh();
}

static void clock_touch(const touch_event_t *ev)
{
    if (ev->type == TOUCH_LONG_PRESS && s_mode == VIEW) {
        if (sys_time_synced()) {
            /* la hora de internet manda: no se edita a mano */
            s_msg_ms = 2000;
            s_last_sec = -1;
            sys_haptic(7);
            refresh();
            return;
        }
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        s_eh = time_is_set(&tm) ? tm.tm_hour : 12;
        s_em = time_is_set(&tm) ? tm.tm_min : 0;
        s_mode = EDIT_HOUR;
        sys_haptic(10);
        refresh();
    } else if (ev->type == TOUCH_TAP && s_mode == EDIT_HOUR) {
        s_mode = EDIT_MIN;
        refresh();
    } else if (ev->type == TOUCH_TAP && s_mode == EDIT_MIN) {
        save_edited_time();
        s_mode = VIEW;
        s_last_sec = -1;
        sys_haptic(12);
        refresh();
    }
}

static void clock_tick(uint32_t dt_ms)
{
    if (s_msg_ms) {
        s_msg_ms = dt_ms >= s_msg_ms ? 0 : s_msg_ms - dt_ms;
        if (!s_msg_ms) s_last_sec = -1;   /* quitar el aviso en el siguiente refresh */
    }
    if (s_mode == VIEW) refresh();
    else refresh_wifi();
}

const app_t app_clock = {
    .id = "clock",
    .name = "Reloj",
    .version = CLOCK_VERSION,
    .icon = LV_SYMBOL_BELL,
    .color = ACCENT,
    .tick_ms = 200,
    .on_enter = clock_enter,
    .on_knob = clock_knob,
    .on_touch = clock_touch,
    .on_tick = clock_tick,
};
