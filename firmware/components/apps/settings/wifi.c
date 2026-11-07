/*
 * Ajustes -> Wi-Fi
 *
 * Lista de redes:
 *   perilla: elegir · tocar: abrir la red (o la opcion) · las redes se buscan al entrar
 * Contrasena (todo con la perilla):
 *   perilla: recorrer letras · tocar: agregar la letra
 *   deslizar arriba/abajo: cambiar de grupo (abc, ABC, 123, #+=)
 *   deslizar a la izquierda: borrar · deslizar a la derecha: volver sin conectar
 *   mantener el dedo: guardar y conectar
 */
#include <stdio.h>
#include <string.h>
#include "wifi.h"
#include "sys.h"
#include "biddy_fonts.h"
#include "esp_log.h"

static const char *TAG = "wifi_ui";

#define ACCENT  0x9B8CFF
#define GREY    0x8A93A6
#define DIM     0x5C6578
#define BAR_BG  0x151B28
#define RED     0xFF6B6B
#define GREEN   0x4CD964
#define ROW_GAP 44
#define MAX_APS 20
#define MAX_ITEMS (MAX_APS + 4)
#define PASS_MAX 64

typedef enum { W_OFF, W_LIST, W_PASS } wmode_t;
typedef enum { IT_AP, IT_SCAN, IT_FORGET, IT_BACK } item_t;

static wmode_t s_mode = W_OFF;
static lv_obj_t *s_parent, *s_view;

/* lista */
static sys_wifi_ap_t s_aps[MAX_APS];
static int s_nap = -1;                 /* -1 = buscando */
static item_t s_items[MAX_ITEMS];
static int s_item_ap[MAX_ITEMS];
static int s_nitems, s_sel;
static lv_obj_t *s_rows[MAX_ITEMS], *s_status;
static sys_wifi_status_t s_last_status = (sys_wifi_status_t)-1;

/* contrasena */
static const char *const GROUPS[] = {
    "abcdefghijklmnopqrstuvwxyz",
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
    "0123456789",
    "!@#$%&*-_+=.,:;?/\\|~^'\"()[]{}<> `",
};
static const char *const GROUP_NAMES[] = {"abc", "ABC", "123", "#+="};
#define N_GROUPS 4
static char s_ssid[33];
static char s_pass[PASS_MAX + 1];
static int s_group, s_ci;
static uint32_t s_msg_ticks;
static lv_obj_t *s_pass_lbl, *s_chars[5], *s_group_lbl[N_GROUPS], *s_msg;

/* ------------------------------------------------------------ utilidades */

static lv_obj_t *label(lv_obj_t *p, const lv_font_t *f, uint32_t color, lv_align_t al, int x, int y)
{
    lv_obj_t *l = lv_label_create(p);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_align(l, al, x, y);
    return l;
}

static void new_view(void)
{
    if (s_view) lv_obj_del(s_view);
    s_view = lv_obj_create(s_parent);
    lv_obj_remove_style_all(s_view);
    lv_obj_set_size(s_view, SYS_SCREEN_W, SYS_SCREEN_H);
    lv_obj_set_style_bg_color(s_view, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_view, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_view, LV_OBJ_FLAG_SCROLLABLE);
}

static const char *signal_dots(int8_t rssi)
{
    if (rssi >= -60) return "•••";
    if (rssi >= -72) return "••";
    return "•";
}

/* ------------------------------------------------------------ lista de redes */

static void status_text(char *buf, size_t len)
{
    char saved[33];
    bool have = sys_wifi_saved_ssid(saved, sizeof(saved));
    switch (sys_wifi_status()) {
    case SYS_WIFI_CONNECTED:      snprintf(buf, len, "Conectado a %s", saved); break;
    case SYS_WIFI_CONNECTING:     snprintf(buf, len, "Conectando a %s...", saved); break;
    case SYS_WIFI_WRONG_PASSWORD: snprintf(buf, len, "Contraseña incorrecta"); break;
    case SYS_WIFI_NOT_FOUND:      snprintf(buf, len, "No encuentro %s", have ? saved : "la red"); break;
    default:                      snprintf(buf, len, "Sin red guardada"); break;
    }
}

static uint32_t status_color(void)
{
    switch (sys_wifi_status()) {
    case SYS_WIFI_CONNECTED:      return GREEN;
    case SYS_WIFI_WRONG_PASSWORD:
    case SYS_WIFI_NOT_FOUND:      return RED;
    default:                      return GREY;
    }
}

static void build_items(void)
{
    char saved[33];
    bool have = sys_wifi_saved_ssid(saved, sizeof(saved));
    s_nitems = 0;
    for (int i = 0; i < s_nap; i++) {
        s_items[s_nitems] = IT_AP;
        s_item_ap[s_nitems++] = i;
    }
    s_items[s_nitems++] = IT_SCAN;
    if (have) s_items[s_nitems++] = IT_FORGET;
    s_items[s_nitems++] = IT_BACK;
    if (s_sel >= s_nitems) s_sel = s_nitems - 1;
}

static void item_text(int i, char *buf, size_t len)
{
    char saved[33] = "";
    sys_wifi_saved_ssid(saved, sizeof(saved));
    switch (s_items[i]) {
    case IT_AP: {
        const sys_wifi_ap_t *a = &s_aps[s_item_ap[i]];
        bool mine = !strcmp(a->ssid, saved);
        snprintf(buf, len, "%s%s  %s%s", mine ? LV_SYMBOL_OK " " : "", a->ssid, signal_dots(a->rssi),
                 a->secure ? "" : "  abierta");
        break;
    }
    case IT_SCAN:   snprintf(buf, len, s_nap < 0 ? "Buscando redes..." : LV_SYMBOL_REFRESH "  Buscar otra vez"); break;
    case IT_FORGET: snprintf(buf, len, "Olvidar %s", saved); break;
    case IT_BACK:   snprintf(buf, len, LV_SYMBOL_LEFT "  Volver"); break;
    }
}

static void list_layout(void)
{
    char buf[80];
    status_text(buf, sizeof(buf));
    lv_label_set_text(s_status, buf);
    lv_obj_set_style_text_color(s_status, lv_color_hex(status_color()), 0);

    for (int i = 0; i < MAX_ITEMS; i++) {
        lv_obj_t *l = s_rows[i];
        int off = i - s_sel;
        if (i >= s_nitems || off < -2 || off > 2) {
            lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(l, LV_OBJ_FLAG_HIDDEN);
        item_text(i, buf, sizeof(buf));
        lv_label_set_text(l, buf);
        bool sel = off == 0;
        lv_obj_set_width(l, sel ? 270 : (off == -2 || off == 2 ? 200 : 250));
        lv_obj_set_style_text_font(l, sel ? &biddy_font_22 : &biddy_font_16, 0);
        lv_obj_set_style_text_color(l, sel ? lv_color_white() : lv_color_hex(GREY), 0);
        lv_obj_set_style_text_opa(l, (off >= -1 && off <= 1) ? LV_OPA_COVER : LV_OPA_50, 0);
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 20 + off * ROW_GAP);
    }
}

static void show_list(void)
{
    s_mode = W_LIST;
    new_view();
    lv_obj_t *t = label(s_view, &biddy_font_16, ACCENT, LV_ALIGN_TOP_MID, 0, 22);
    lv_label_set_text(t, LV_SYMBOL_WIFI "  Wi-Fi");
    s_status = label(s_view, &biddy_font_16, GREY, LV_ALIGN_TOP_MID, 0, 50);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_status, 240);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *bar = lv_obj_create(s_view);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, 300, 40);
    lv_obj_align(bar, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_radius(bar, 20, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(BAR_BG), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);

    for (int i = 0; i < MAX_ITEMS; i++) {
        s_rows[i] = lv_label_create(s_view);
        lv_label_set_long_mode(s_rows[i], LV_LABEL_LONG_DOT);
        lv_obj_set_style_text_align(s_rows[i], LV_TEXT_ALIGN_CENTER, 0);
    }
    build_items();
    list_layout();
}

static void start_scan(void)
{
    s_nap = -1;
    if (!sys_wifi_scan_start()) ESP_LOGW(TAG, "no se pudo empezar la busqueda");
    build_items();
    s_sel = 0;
}

/* ------------------------------------------------------------ contrasena */

static char cur_char(void) { return GROUPS[s_group][s_ci]; }

static void char_text(char c, char *buf)
{
    if (c == ' ') strcpy(buf, "esp");
    else { buf[0] = c; buf[1] = 0; }
}

static void pass_layout(void)
{
    /* contrasena escrita (se ve: es tu placa) con cursor */
    size_t n = strlen(s_pass);
    char shown[24];
    if (n == 0) {
        lv_label_set_text(s_pass_lbl, "_");
    } else {
        const char *tail = n > 14 ? s_pass + n - 13 : s_pass;
        snprintf(shown, sizeof(shown), "%s%s_", n > 14 ? "…" : "", tail);
        lv_label_set_text(s_pass_lbl, shown);
    }

    /* carrusel: la letra elegida grande y dos vecinas a cada lado */
    int len = (int)strlen(GROUPS[s_group]);
    for (int k = -2; k <= 2; k++) {
        char b[4];
        char_text(GROUPS[s_group][(s_ci + k + len) % len], b);
        lv_obj_t *l = s_chars[k + 2];
        lv_label_set_text(l, b);
        lv_obj_set_style_text_font(l, k == 0 ? &biddy_font_32 : (k == -1 || k == 1 ? &biddy_font_22 : &biddy_font_16), 0);
        lv_obj_set_style_text_color(l, k == 0 ? lv_color_white() : lv_color_hex(k == -1 || k == 1 ? GREY : DIM), 0);
        lv_obj_align(l, LV_ALIGN_CENTER, k * 52, 6);
    }

    for (int g = 0; g < N_GROUPS; g++) {
        lv_obj_set_style_text_color(s_group_lbl[g], lv_color_hex(g == s_group ? ACCENT : DIM), 0);
    }
}

static void show_pass(const char *ssid)
{
    s_mode = W_PASS;
    strncpy(s_ssid, ssid, sizeof(s_ssid));
    s_ssid[32] = 0;
    s_pass[0] = 0;
    s_group = s_ci = 0;
    s_msg_ticks = 0;
    new_view();

    lv_obj_t *t = label(s_view, &biddy_font_16, GREY, LV_ALIGN_TOP_MID, 0, 26);
    lv_label_set_text(t, "Contraseña de");
    lv_obj_t *n = label(s_view, &biddy_font_16, ACCENT, LV_ALIGN_TOP_MID, 0, 46);
    lv_label_set_long_mode(n, LV_LABEL_LONG_DOT);
    lv_obj_set_width(n, 230);
    lv_obj_set_style_text_align(n, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(n, s_ssid);

    lv_obj_t *box = lv_obj_create(s_view);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, 280, 44);
    lv_obj_align(box, LV_ALIGN_CENTER, 0, -66);
    lv_obj_set_style_radius(box, 22, 0);
    lv_obj_set_style_bg_color(box, lv_color_hex(BAR_BG), 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    s_pass_lbl = label(s_view, &biddy_font_22, 0xFFFFFF, LV_ALIGN_CENTER, 0, -66);

    for (int k = 0; k < 5; k++) s_chars[k] = lv_label_create(s_view);

    for (int g = 0; g < N_GROUPS; g++) {
        s_group_lbl[g] = label(s_view, &biddy_font_16, DIM, LV_ALIGN_CENTER, (g - 1) * 52 - 26, 56);
        lv_label_set_text(s_group_lbl[g], GROUP_NAMES[g]);
    }

    s_msg = label(s_view, &biddy_font_16, DIM, LV_ALIGN_CENTER, 0, 92);
    lv_obj_set_style_text_align(s_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_msg, "toca: agregar · " LV_SYMBOL_LEFT " borrar · " LV_SYMBOL_RIGHT " volver\n"
                             "mantén el dedo: conectar");
    pass_layout();
}

static void pass_message(const char *text)
{
    lv_label_set_text(s_msg, text);
    lv_obj_set_style_text_color(s_msg, lv_color_hex(RED), 0);
    s_msg_ticks = 15;   /* ~3 s */
}

static void connect_and_back(const char *ssid, const char *pass)
{
    sys_wifi_connect(ssid, pass);
    sys_haptic(12);
    s_sel = 0;
    s_last_status = (sys_wifi_status_t)-1;
    show_list();
}

/* ------------------------------------------------------------ entrada */

void wifi_screen_open(lv_obj_t *parent)
{
    s_parent = parent;
    s_view = NULL;
    s_sel = 0;
    ESP_LOGI(TAG, "pantalla Wi-Fi abierta");
    start_scan();
    show_list();
}

bool wifi_screen_active(void) { return s_mode != W_OFF; }

void wifi_screen_reset(void)
{
    s_mode = W_OFF;
    s_view = NULL;
}

static void close_screen(void)
{
    if (s_view) lv_obj_del(s_view);
    s_view = NULL;
    s_mode = W_OFF;
}

void wifi_screen_knob(int dir)
{
    if (s_mode == W_LIST) {
        s_sel += dir;
        if (s_sel < 0) s_sel = 0;
        if (s_sel >= s_nitems) s_sel = s_nitems - 1;
        list_layout();
    } else if (s_mode == W_PASS) {
        int len = (int)strlen(GROUPS[s_group]);
        s_ci = (s_ci + dir + len) % len;
        pass_layout();
    }
}

void wifi_screen_touch(const touch_event_t *ev)
{
    if (s_mode == W_LIST) {
        if (ev->type == TOUCH_SWIPE_UP || ev->type == TOUCH_SWIPE_DOWN) {
            wifi_screen_knob(ev->type == TOUCH_SWIPE_UP ? +1 : -1);
            return;
        }
        if (ev->type != TOUCH_TAP) return;
        switch (s_items[s_sel]) {
        case IT_AP: {
            const sys_wifi_ap_t *a = &s_aps[s_item_ap[s_sel]];
            if (a->secure) show_pass(a->ssid);
            else connect_and_back(a->ssid, "");
            break;
        }
        case IT_SCAN:
            if (s_nap >= 0) { start_scan(); list_layout(); }
            break;
        case IT_FORGET:
            sys_wifi_forget();
            sys_haptic(10);
            build_items();
            list_layout();
            break;
        case IT_BACK:
            ESP_LOGI(TAG, "volver a Ajustes");
            close_screen();
            break;
        }
    } else if (s_mode == W_PASS) {
        size_t n = strlen(s_pass);
        switch (ev->type) {
        case TOUCH_TAP:
            if (n < PASS_MAX) {
                s_pass[n] = cur_char();
                s_pass[n + 1] = 0;
            }
            pass_layout();
            break;
        case TOUCH_SWIPE_LEFT:
            if (n) s_pass[n - 1] = 0;
            pass_layout();
            break;
        case TOUCH_SWIPE_RIGHT:
            show_list();
            break;
        case TOUCH_SWIPE_UP:
        case TOUCH_SWIPE_DOWN:
            s_group = (s_group + (ev->type == TOUCH_SWIPE_UP ? 1 : N_GROUPS - 1)) % N_GROUPS;
            s_ci = 0;
            pass_layout();
            break;
        case TOUCH_LONG_PRESS:
            if (n < 8) pass_message("Mínimo 8 caracteres");
            else connect_and_back(s_ssid, s_pass);
            break;
        default:
            break;
        }
    }
}

void wifi_screen_tick(void)
{
    if (s_mode == W_LIST) {
        bool changed = false;
        if (s_nap < 0) {
            int n = sys_wifi_scan_results(s_aps, MAX_APS);
            if (n >= 0) {
                ESP_LOGI(TAG, "%d redes en la lista", n);
                s_nap = n;
                build_items();
                changed = true;
            }
        }
        sys_wifi_status_t st = sys_wifi_status();
        if (st != s_last_status) {
            s_last_status = st;
            changed = true;
        }
        if (changed) list_layout();
    } else if (s_mode == W_PASS && s_msg_ticks && --s_msg_ticks == 0) {
        lv_label_set_text(s_msg, "toca: agregar · " LV_SYMBOL_LEFT " borrar · " LV_SYMBOL_RIGHT " volver\n"
                                 "mantén el dedo: conectar");
        lv_obj_set_style_text_color(s_msg, lv_color_hex(DIM), 0);
    }
}
