/*
 * ui_core.c — el "sistema operativo" de Hey Biddy.
 *
 * Una sola tarea (sys) hace todo lo de interfaz, en este orden, una y otra vez:
 *   1. ejecuta lo que otras tareas pidieron con sys_run_on_ui()
 *   2. reparte los clicks de la perilla a la app activa (con vibracion)
 *   3. lee el touch, reconoce gestos y los reparte (y detecta "deslizar desde arriba = menu")
 *   4. cambia de app si alguien lo pidio
 *   5. llama on_tick de la app activa
 *   6. deja que LVGL dibuje (salvo en apps de pantalla directa)
 *
 * Standby: tras el tiempo de Ajustes (60 s por defecto) sin perilla ni touch se apaga la luz. La app sigue abierta
 * y recibe on_tick, pero no perilla ni touch; solo un doble tap enciende la pantalla.
 *
 * Como todo ocurre en la misma tarea, las apps nunca necesitan candados.
 */
#include <string.h>
#include <stdlib.h>
#include "ui_core.h"
#include "sys.h"
#include "board.h"
#include "storage.h"
#include "launcher.h"
#include "net.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_app_desc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static const char *TAG = "sys";

#define SYS_NS               "system"
#define LVGL_BUF_LINES       36          /* 1/10 de la pantalla, como el demo */
#define RAW_STRIP_LINES      24
#define TOUCH_POLL_MS        15
#define LONG_PRESS_MS        600
#define TAP_MAX_MOVE         12          /* px: mas que esto ya no es toque */
#define SWIPE_MIN_MOVE       60          /* px */
#define HOME_EDGE_Y          60          /* deslizar hacia abajo empezando aqui arriba = menu */
#define HOME_MIN_DY          70
#define KNOB_HAPTIC_EFFECT   6           /* "Sharp Click 30%" */
#define STANDBY_DEFAULT_S    60          /* sin tocar nada este tiempo = pantalla oscura (se cambia en Ajustes) */
#define DOUBLE_TAP_MS        500         /* maximo entre los dos toques para despertar */
#define WAKE_HAPTIC_EFFECT   7           /* golpe suave al despertar */

/* ------------------------------------------------------------ estado */

static const app_t *const *s_apps;
static size_t s_app_count;

static const app_t *s_active;            /* app abierta; el lanzador cuenta como una app mas */
static const app_t *s_pending;           /* app a abrir al final de la vuelta */
static lv_obj_t *s_app_screen;
static lv_obj_t *s_launcher_screen;
static bool s_raw_mode;

static QueueHandle_t s_knob_q;
static QueueHandle_t s_ui_q;
typedef struct { sys_ui_fn_t fn; void *arg; } ui_job_t;

static uint8_t s_brightness = 80;
static bool s_haptic_on = true;
static uint32_t s_boot_count;

/* LVGL */
static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_disp_t *s_disp;

/* pantalla directa */
static uint16_t *s_raw_strip[2];
static SemaphoreHandle_t s_raw_sem;

/* touch */
static bool s_touch_down;
static int16_t s_tx, s_ty;               /* posicion actual (tambien la lee LVGL) */

/* standby */
static bool s_standby;
static uint16_t s_standby_s = STANDBY_DEFAULT_S;   /* 0 = nunca */
static volatile bool s_wake_req;
static int64_t s_last_input;             /* ms de la ultima perilla o toque */

/* ------------------------------------------------------ pantalla / LVGL */

static void IRAM_ATTR lcd_done(void *ctx)
{
    if (s_raw_mode) {
        BaseType_t woken = pdFALSE;
        xSemaphoreGiveFromISR(s_raw_sem, &woken);
        if (woken) portYIELD_FROM_ISR();
    } else {
        lv_disp_flush_ready(&s_disp_drv);
    }
}

static void lvgl_flush(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *px)
{
    board_lcd_draw(a->x1, a->y1, a->x2 + 1, a->y2 + 1, px);
}

/* El controlador necesita areas que empiecen en par y terminen en impar */
static void lvgl_rounder(lv_disp_drv_t *drv, lv_area_t *a)
{
    a->x1 &= ~1;
    a->y1 &= ~1;
    a->x2 |= 1;
    a->y2 |= 1;
}

static void lvgl_touch_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    data->point.x = s_tx;
    data->point.y = s_ty;
    data->state = s_touch_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void display_init(void)
{
    lv_init();

    size_t buf_px = BOARD_LCD_H_RES * LVGL_BUF_LINES;
    lv_color_t *b1 = heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    lv_color_t *b2 = heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(b1 && b2);
    lv_disp_draw_buf_init(&s_draw_buf, b1, b2, buf_px);

    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = BOARD_LCD_H_RES;
    s_disp_drv.ver_res = BOARD_LCD_V_RES;
    s_disp_drv.flush_cb = lvgl_flush;
    s_disp_drv.rounder_cb = lvgl_rounder;
    s_disp_drv.draw_buf = &s_draw_buf;
    s_disp = lv_disp_drv_register(&s_disp_drv);

    static lv_indev_drv_t indev;
    lv_indev_drv_init(&indev);
    indev.type = LV_INDEV_TYPE_POINTER;
    indev.disp = s_disp;
    indev.read_cb = lvgl_touch_read;
    lv_indev_drv_register(&indev);

    /* Tiras para la pantalla directa (RAM interna con DMA) */
    s_raw_sem = xSemaphoreCreateCounting(2, 2);
    for (int i = 0; i < 2; i++) {
        s_raw_strip[i] = heap_caps_malloc(BOARD_LCD_H_RES * RAW_STRIP_LINES * 2, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
        assert(s_raw_strip[i]);
    }

    board_lcd_set_done_cb(lcd_done, NULL);
}

/* Espera a que LVGL termine de mandar su ultimo bloque */
static void lvgl_wait_idle(void)
{
    while (s_disp_drv.draw_buf->flushing) {
        vTaskDelay(1);
    }
}

void sys_raw_draw(int x, int y, int w, int h, const uint16_t *pixels)
{
    if (!s_raw_mode || w <= 0 || h <= 0) return;
    int lines_per_strip = RAW_STRIP_LINES * BOARD_LCD_H_RES / w;  /* mas filas si el rectangulo es angosto */
    lines_per_strip &= ~1;
    if (lines_per_strip < 2) lines_per_strip = 2;
    int k = 0;
    for (int row = 0; row < h; row += lines_per_strip) {
        int n = h - row < lines_per_strip ? h - row : lines_per_strip;
        xSemaphoreTake(s_raw_sem, portMAX_DELAY);
        memcpy(s_raw_strip[k], pixels + (size_t)row * w, (size_t)n * w * 2);
        board_lcd_draw(x, y + row, x + w, y + row + n, s_raw_strip[k]);
        k ^= 1;
    }
    /* esperar a que terminen las dos tiras */
    xSemaphoreTake(s_raw_sem, portMAX_DELAY);
    xSemaphoreTake(s_raw_sem, portMAX_DELAY);
    xSemaphoreGive(s_raw_sem);
    xSemaphoreGive(s_raw_sem);
}

/* ------------------------------------------------------ cambiar de app */

static lv_obj_t *new_black_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_style_all(scr);
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    return scr;
}

static void switch_to(const app_t *next)
{
    if (next == s_active) return;

    /* 1. cerrar la app actual */
    if (s_active && s_active->on_exit) {
        s_active->on_exit();
    }
    if (s_raw_mode) {
        s_raw_mode = false;
        lv_obj_invalidate(lv_scr_act());          /* LVGL vuelve a pintar todo */
    }
    lv_obj_t *old_screen = s_app_screen;
    s_app_screen = NULL;

    /* 2. preparar la pantalla de la nueva */
    const bool is_launcher = (next == launcher_app());
    lv_obj_t *scr = is_launcher ? s_launcher_screen : new_black_screen();
    if (!is_launcher) s_app_screen = scr;
    lv_scr_load(scr);
    if (old_screen) lv_obj_del(old_screen);       /* borra todo lo que creo la app anterior */

    if (next->flags & APP_FLAG_RAW_DISPLAY) {
        lv_refr_now(s_disp);                       /* deja la pantalla en negro */
        lvgl_wait_idle();
        s_raw_mode = true;
    }

    /* 3. abrir */
    s_active = next;
    ESP_LOGI(TAG, "app: %s", next->id);
    if (next->on_enter) {
        next->on_enter((next->flags & APP_FLAG_RAW_DISPLAY) ? NULL : scr);
    }
}

void sys_go_home(void)
{
    s_pending = launcher_app();
}

bool sys_open_app(const char *id)
{
    for (size_t i = 0; i < s_app_count; i++) {
        if (strcmp(s_apps[i]->id, id) == 0) {
            s_pending = s_apps[i];
            return true;
        }
    }
    return false;
}

size_t sys_app_count(void) { return s_app_count; }
const app_t *sys_app_at(size_t i) { return i < s_app_count ? s_apps[i] : NULL; }

/* ------------------------------------------------------ ajustes */

uint8_t sys_get_brightness(void) { return s_brightness; }

void sys_set_brightness(uint8_t p)
{
    if (p < 5) p = 5;
    if (p > 100) p = 100;
    s_brightness = p;
    board_backlight_set(p);
    storage_set_i32(SYS_NS, "brightness", p);
}

bool sys_get_haptic_enabled(void) { return s_haptic_on; }

void sys_set_haptic_enabled(bool on)
{
    s_haptic_on = on;
    storage_set_i32(SYS_NS, "haptic", on);
}

uint32_t sys_get_boot_count(void) { return s_boot_count; }

const char *sys_firmware_version(void) { return esp_app_get_description()->version; }

bool sys_net_connected(void) { return net_is_connected(); }
bool sys_time_synced(void) { return net_time_synced(); }
sys_wifi_status_t sys_wifi_status(void) { return (sys_wifi_status_t)net_status(); }
bool sys_wifi_saved_ssid(char *out, size_t len) { return net_saved_ssid(out, len); }
bool sys_wifi_scan_start(void) { return net_scan_start(); }
void sys_wifi_connect(const char *ssid, const char *pass) { net_connect(ssid, pass); }
void sys_wifi_forget(void) { net_forget(); }

_Static_assert(sizeof(sys_wifi_ap_t) == sizeof(net_ap_t), "sys_wifi_ap_t y net_ap_t deben coincidir");
_Static_assert((int)SYS_WIFI_NOT_FOUND == (int)NET_NOT_FOUND, "sys_wifi_status_t y net_status_t deben coincidir");

int sys_wifi_scan_results(sys_wifi_ap_t *out, int max)
{
    return net_scan_results((net_ap_t *)out, max);
}

/* ------------------------------------------------------ standby */

void sys_wake(void) { s_wake_req = true; }
bool sys_is_standby(void) { return s_standby; }

uint16_t sys_get_standby_s(void) { return s_standby_s; }

void sys_set_standby_s(uint16_t sec)
{
    s_standby_s = sec;
    s_last_input = esp_timer_get_time() / 1000;   /* el conteo empieza de nuevo */
    storage_set_i32(SYS_NS, "standby_s", sec);
}

static void enter_standby(void)
{
    s_standby = true;
    board_backlight_set(0);
    ESP_LOGI(TAG, "standby (doble tap para despertar)");
}

static void leave_standby(int64_t now)
{
    s_standby = false;
    s_last_input = now;
    board_backlight_set(s_brightness);
    ESP_LOGI(TAG, "despierto");
}

/* En standby el touch solo busca un doble tap; nada llega a la app ni a LVGL. */
static void poll_touch_standby(int64_t now)
{
    static bool down, moved;
    static int64_t down_at, last_tap_at = -100000;
    static uint16_t sx, sy;

    uint16_t x, y;
    bool d = board_touch_read(&x, &y);
    if (d && !down) {
        down = true;
        moved = false;
        down_at = now;
        sx = x;
        sy = y;
    } else if (d && down) {
        if (abs(x - sx) > TAP_MAX_MOVE || abs(y - sy) > TAP_MAX_MOVE) moved = true;
    } else if (!d && down) {
        down = false;
        bool tap = !moved && now - down_at < LONG_PRESS_MS;
        if (!tap) return;
        if (now - last_tap_at <= DOUBLE_TAP_MS) {
            last_tap_at = -100000;
            leave_standby(now);
            sys_haptic(WAKE_HAPTIC_EFFECT);
        } else {
            last_tap_at = now;
        }
    }
}

void sys_haptic(uint8_t effect)
{
    if (s_haptic_on) board_haptic_play(effect);
}

void sys_haptic_click(void) { sys_haptic(KNOB_HAPTIC_EFFECT); }

static void load_settings(void)
{
    int32_t v;
    if (storage_get_i32(SYS_NS, "brightness", &v) == ESP_OK) s_brightness = (uint8_t)v;
    if (storage_get_i32(SYS_NS, "haptic", &v) == ESP_OK) s_haptic_on = v != 0;
    if (storage_get_i32(SYS_NS, "standby_s", &v) == ESP_OK && v >= 0 && v <= 3600) s_standby_s = (uint16_t)v;
    v = 0;
    storage_get_i32(SYS_NS, "boots", &v);
    s_boot_count = (uint32_t)v + 1;
    storage_set_i32(SYS_NS, "boots", (int32_t)s_boot_count);
}

/* ------------------------------------------------------ hilos */

bool sys_run_on_ui(sys_ui_fn_t fn, void *arg)
{
    ui_job_t job = {fn, arg};
    return xQueueSend(s_ui_q, &job, 0) == pdTRUE;
}

/* ------------------------------------------------------ entrada */

static void knob_from_timer(int dir)
{
    int8_t d = (int8_t)dir;
    xQueueSend(s_knob_q, &d, 0);
}

static void deliver_touch(touch_type_t type, int16_t dx, int16_t dy)
{
    if (!s_active || !s_active->on_touch) return;
    touch_event_t ev = {.type = type, .x = s_tx, .y = s_ty, .dx = dx, .dy = dy};
    s_active->on_touch(&ev);
}

static void poll_touch(void)
{
    static int64_t down_at;
    static int16_t start_x, start_y, last_x, last_y;
    static bool moved, long_sent;

    uint16_t x, y;
    bool down = board_touch_read(&x, &y);
    int64_t now = esp_timer_get_time() / 1000;

    if (down) {
        s_tx = x;
        s_ty = y;
        s_last_input = now;
    }

    if (down && !s_touch_down) {                       /* empieza un toque */
        s_touch_down = true;
        down_at = now;
        start_x = last_x = x;
        start_y = last_y = y;
        moved = long_sent = false;
        deliver_touch(TOUCH_PRESS, 0, 0);
        return;
    }

    if (down && s_touch_down) {                        /* el dedo sigue ahi */
        int16_t dx = x - last_x, dy = y - last_y;
        if (abs(x - start_x) > TAP_MAX_MOVE || abs(y - start_y) > TAP_MAX_MOVE) moved = true;
        if (dx || dy) {
            last_x = x;
            last_y = y;
            deliver_touch(TOUCH_MOVE, dx, dy);
        }
        if (!moved && !long_sent && now - down_at >= LONG_PRESS_MS) {
            long_sent = true;
            deliver_touch(TOUCH_LONG_PRESS, 0, 0);
        }
        return;
    }

    if (!down && s_touch_down) {                       /* se levanto el dedo */
        s_touch_down = false;
        int16_t tdx = last_x - start_x, tdy = last_y - start_y;

        /* gesto del sistema: deslizar hacia abajo desde el borde de arriba = menu */
        bool home = start_y < HOME_EDGE_Y && tdy > HOME_MIN_DY && abs(tdy) > abs(tdx)
                    && s_active != launcher_app()
                    && !(s_active->flags & APP_FLAG_NO_HOME_GESTURE);
        if (home) {
            ESP_LOGI(TAG, "gesto: deslizar desde arriba -> menu");
            deliver_touch(TOUCH_RELEASE, tdx, tdy);
            sys_haptic(KNOB_HAPTIC_EFFECT);
            sys_go_home();
            return;
        }

        if (!moved && !long_sent) {
            deliver_touch(TOUCH_TAP, 0, 0);
        } else if (moved && (abs(tdx) >= SWIPE_MIN_MOVE || abs(tdy) >= SWIPE_MIN_MOVE)) {
            touch_type_t t;
            if (abs(tdx) > abs(tdy)) t = tdx > 0 ? TOUCH_SWIPE_RIGHT : TOUCH_SWIPE_LEFT;
            else                     t = tdy > 0 ? TOUCH_SWIPE_DOWN : TOUCH_SWIPE_UP;
            deliver_touch(t, tdx, tdy);
        }
        deliver_touch(TOUCH_RELEASE, tdx, tdy);
    }
}

/* ------------------------------------------------------ tarea principal */

static void sys_task(void *arg)
{
    int64_t last_touch = 0, last_tick = esp_timer_get_time() / 1000, last_lv = last_tick;
    s_last_input = last_tick;

    for (;;) {
        int64_t now = esp_timer_get_time() / 1000;
        lv_tick_inc((uint32_t)(now - last_lv));      /* el reloj de LVGL avanza con el del sistema */
        last_lv = now;

        ui_job_t job;
        while (xQueueReceive(s_ui_q, &job, 0) == pdTRUE) {
            job.fn(job.arg);
        }

        if (s_wake_req) {
            s_wake_req = false;
            if (s_standby) leave_standby(now);
            else s_last_input = now;
        }

        int8_t dir;
        while (xQueueReceive(s_knob_q, &dir, 0) == pdTRUE) {
            if (s_standby) continue;                   /* en standby la perilla no hace nada */
            s_last_input = now;
            sys_haptic_click();
            if (s_active && s_active->on_knob) s_active->on_knob(dir);
        }

        if (now - last_touch >= TOUCH_POLL_MS) {
            last_touch = now;
            if (s_standby) poll_touch_standby(now);
            else poll_touch();
        }

        if (s_standby_s && !s_standby && !s_touch_down && now - s_last_input >= (int64_t)s_standby_s * 1000) {
            enter_standby();
        }

        if (s_pending) {
            const app_t *next = s_pending;
            s_pending = NULL;
            switch_to(next);
            last_tick = now;
        }

        uint32_t period = s_active->tick_ms ? s_active->tick_ms : 33;
        if (now - last_tick >= period) {
            uint32_t dt = (uint32_t)(now - last_tick);
            last_tick = now;
            if (s_active->on_tick) s_active->on_tick(dt);
        }

        uint32_t wait_ms = 5;
        if (!s_raw_mode) {
            uint32_t next = lv_timer_handler();
            if (next < wait_ms) wait_ms = next ? next : 1;
        }

        vTaskDelay(pdMS_TO_TICKS(wait_ms));
    }
}

/* ------------------------------------------------------ arranque */

static void check_registry(void)
{
    for (size_t i = 0; i < s_app_count; i++) {
        const app_t *a = s_apps[i];
        if (strlen(a->id) > 15) {
            ESP_LOGE(TAG, "el id '%s' mide mas de 15 letras: NVS no lo acepta", a->id);
        }
        if (strcmp(a->id, SYS_NS) == 0 || strcmp(a->id, "launcher") == 0 || strcmp(a->id, NET_NS) == 0) {
            ESP_LOGE(TAG, "el id '%s' esta reservado por el sistema", a->id);
        }
        for (size_t j = 0; j < i; j++) {
            if (strcmp(a->id, s_apps[j]->id) == 0) {
                ESP_LOGE(TAG, "dos apps usan el id '%s'", a->id);
            }
        }
        ESP_LOGI(TAG, "  %-12s v%-8s %s", a->id, a->version, a->name);
    }
}

void ui_core_start(const app_t *const *apps, size_t count)
{
    s_apps = apps;
    s_app_count = count;
    ESP_LOGI(TAG, "Hey Biddy firmware %s, %u apps:", sys_firmware_version(), (unsigned)count);
    check_registry();

    load_settings();
    ESP_LOGI(TAG, "arranque numero %lu", (unsigned long)s_boot_count);

    s_knob_q = xQueueCreate(32, sizeof(int8_t));
    s_ui_q = xQueueCreate(16, sizeof(ui_job_t));

    display_init();
    board_knob_set_cb(knob_from_timer);

    s_launcher_screen = new_black_screen();
    launcher_build(s_launcher_screen);
    s_active = NULL;
    s_pending = launcher_app();
    switch_to(s_pending);
    s_pending = NULL;

    /* pintar el primer cuadro y solo entonces encender la luz (sin basura en pantalla) */
    lv_refr_now(s_disp);
    lvgl_wait_idle();
    board_backlight_set(s_brightness);

    xTaskCreatePinnedToCore(sys_task, "sys", 8192, NULL, 5, NULL, 1);
}
