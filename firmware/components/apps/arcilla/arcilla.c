/*
 * Arcilla — juego táctico de criaturas de arcilla para dos jugadores (app de Hey Biddy).
 *
 * El juego vive en game/ (motor, interfaz y dibujo propios, sin LVGL) y aquí solo se conecta
 * con el sistema: la app dibuja la pantalla ella misma (APP_FLAG_RAW_DISPLAY), recibe la perilla
 * y el touch, y guarda idioma y vibración en su propio espacio de NVS ("arcilla").
 *
 *   perilla: elegir · tocar: confirmar · mantener: volver · deslizar: mover el mapa
 *   Salir: opción "Salir" del menú del juego, o deslizar hacia abajo desde el borde de arriba.
 *
 * Contra Biddy: la IA (game/brain.c) piensa en su propia tarea, en el núcleo 0, para que la
 * pantalla siga animando mientras decide. Su memoria (nivel, ejércitos recientes, marcador)
 * se guarda en el espacio de NVS "arcilla", clave "brain".
 *
 * Herramientas de PC (pruebas, simulador, laboratorio de balance, arte): ver README.md.
 */
#include <stdlib.h>
#include <string.h>
#include "app.h"
#include "sys.h"
#include "storage.h"
#include "game/engine.h"
#include "game/gfx.h"
#include "game/ui.h"
#include "game/brain.h"
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#else
#include <time.h>
#endif

#define ARCILLA_VERSION "0.7.2"
#define APP_ID "arcilla"
#define STRIP 48   /* filas por envío: sys_raw_draw manda dos tiras de 24 seguidas sin esperar entre ellas */

/* ---- Lo que el juego le pide a la plataforma ---- */
uint32_t hal_ms(void)
{
#ifdef ESP_PLATFORM
    return (uint32_t)(esp_timer_get_time() / 1000);
#else
    return (uint32_t)(clock() * 1000 / CLOCKS_PER_SEC);
#endif
}
uint8_t platform_random_byte(void)
{
#ifdef ESP_PLATFORM
    return (uint8_t)(esp_random() & 0xFF);
#else
    return (uint8_t)(rand() & 0xFF);
#endif
}
void hal_haptic(haptic_t k)
{
    /* números de efecto de la biblioteca del DRV2605 (sys_haptic respeta el ajuste del sistema) */
    static const uint8_t FX[] = {[HAP_TICK] = 24, [HAP_OK] = 1, [HAP_NO] = 10, [HAP_THUD] = 1, [HAP_HIT] = 12, [HAP_MAGIC] = 10, [HAP_WIN] = 14};
    sys_haptic(FX[k]);
}
typedef struct { uint8_t lang, haptic; } arcilla_cfg_t;
void hal_save_settings(lang_t lang, bool haptic_on)
{
    arcilla_cfg_t c = {(uint8_t)lang, haptic_on};
    storage_set_blob(APP_ID, "cfg", &c, sizeof c);
}
void hal_exit(void) { sys_go_home(); }

bool hal_load(const char *key, void *data, int len)
{
    size_t n = (size_t)len;
    return storage_get_blob(APP_ID, key, data, &n) == 0 && n == (size_t)len;
}
void hal_store(const char *key, const void *data, int len) { storage_set_blob(APP_ID, key, data, (size_t)len); }

/* ---- Biddy piensa en segundo plano ---- */
#ifdef ESP_PLATFORM
static TaskHandle_t s_think_task;
static void (*volatile s_think_fn)(void);
static int64_t s_last_yield;
static void think_yield(void)
{
    /* la IA llama esto seguido; cede la CPU cada ~40 ms para no bloquear el núcleo 0 */
    int64_t now = esp_timer_get_time();
    if (now - s_last_yield > 40000) { vTaskDelay(1); s_last_yield = esp_timer_get_time(); }
}
static void think_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        void (*fn)(void) = s_think_fn;
        if (!fn) continue;
        int64_t t0 = esp_timer_get_time(); s_last_yield = t0;
        fn();
        ESP_LOGI("arcilla", "Biddy pensó %d ms", (int)((esp_timer_get_time() - t0) / 1000));
    }
}
void hal_think_async(void (*fn)(void))
{
    if (!s_think_task) {
        ai_yield_hook = think_yield;
        if (xTaskCreatePinnedToCore(think_task, "biddy_ai", 12288, NULL, 2, &s_think_task, 0) != pdPASS) { s_think_task = NULL; fn(); return; }
    }
    s_think_fn = fn;
    xTaskNotifyGive(s_think_task);
}
#else
void hal_think_async(void (*fn)(void)) { fn(); }   /* en la PC piensa en el momento */
#endif

/* ---- Estado de la app ---- */
static uint16_t *s_fb, *s_bg, *s_strip;
static bool s_started;          /* el juego sigue en memoria al salir: al volver se continúa la partida */
static bool s_down, s_drag, s_long;
static int s_moved;
static int s_tests_pass, s_tests_total;
#ifdef ESP_PLATFORM
static int64_t s_perf_t0, s_draw_us, s_send_us; static int s_frames;
#endif

static void *fast_alloc(size_t n)
{
#ifdef ESP_PLATFORM
    void *p = heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);   /* RAM interna: mucho más rápida que la PSRAM */
    return p ? p : heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#else
    return malloc(n);
#endif
}

static void *big_alloc(size_t n)
{
#ifdef ESP_PLATFORM
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p ? p : malloc(n);
#else
    return malloc(n);
#endif
}

static void flush(void)
{
    /* el juego dibuja en RGB565 normal; la pantalla lo quiere con los bytes invertidos */
    for (int y0 = 0; y0 < SCR; y0 += STRIP) {
        int h = SCR - y0 < STRIP ? SCR - y0 : STRIP;
        const uint16_t *src = s_fb + y0 * SCR;
        /* de dos en dos píxeles: una lectura de 32 bits de la PSRAM en lugar de dos de 16 */
        const uint32_t *s32 = (const uint32_t *)src; uint32_t *d32 = (uint32_t *)s_strip; int n2 = SCR * h / 2;
        for (int i = 0; i < n2; i++) { uint32_t c = s32[i]; d32[i] = ((c & 0x00FF00FFu) << 8) | ((c >> 8) & 0x00FF00FFu); }
        sys_raw_draw(0, y0, SCR, h, s_strip);
    }
}

static void quiet_test(bool ok, const char *name, const char *detail) { (void)ok; (void)name; (void)detail; }

static void arcilla_enter(lv_obj_t *screen)
{
    (void)screen;
    if (!s_fb) {
        s_fb = big_alloc(SCR * SCR * 2);
        s_bg = big_alloc(SCR * SCR * 2);
        s_strip = fast_alloc(SCR * STRIP * 2);  /* sys_raw_draw copia a sus propias tiras DMA */
        if (!s_fb || !s_bg || !s_strip) { sys_go_home(); return; }
        gfx_init(s_fb, s_bg);
    }
    if (!s_started) {
        arcilla_cfg_t c = {LANG_ES, 1}; size_t n = sizeof c;
        storage_get_blob(APP_ID, "cfg", &c, &n);
        s_tests_pass = run_tests(quiet_test, &s_tests_total);
        ui_init((lang_t)c.lang, c.haptic, s_tests_pass, s_tests_total);
        s_started = true;
    }
    s_down = s_drag = s_long = false;
#ifdef ESP_PLATFORM
    s_perf_t0 = 0; s_draw_us = s_send_us = 0; s_frames = 0;
#endif
    ui_frame(); flush();
}

static void arcilla_knob(int dir) { ui_input(dir > 0 ? IN_R : IN_L); }

static void arcilla_touch(const touch_event_t *ev)
{
    switch (ev->type) {
    case TOUCH_PRESS: s_down = true; s_drag = s_long = false; s_moved = 0; ui_holding(true, hal_ms()); break;
    case TOUCH_MOVE:
        s_moved += abs(ev->dx) + abs(ev->dy);
        if (!s_drag && s_moved > 10) { s_drag = true; ui_holding(false, 0); }
        if (s_drag) ui_drag(ev->dx, ev->dy);
        break;
    case TOUCH_TAP: if (!s_drag) ui_input(IN_TAP); break;
    case TOUCH_LONG_PRESS: if (!s_drag) { s_long = true; ui_holding(false, 0); ui_input(IN_LONG); } break;
    case TOUCH_RELEASE: ui_holding(false, 0); if (s_drag) ui_drag_end(); s_down = s_drag = false; break;
    default: break;   /* los SWIPE ya se atienden con MOVE (mover el mapa) */
    }
}

#ifdef ESP_PLATFORM
/* Cada 5 s imprime en el monitor los cuadros por segundo y en qué se va el tiempo. */
static void arcilla_tick(uint32_t dt_ms)
{
    (void)dt_ms;
    int64_t a = esp_timer_get_time(); ui_frame();
    int64_t b = esp_timer_get_time(); flush();
    int64_t c = esp_timer_get_time();
    s_draw_us += b - a; s_send_us += c - b; s_frames++;
    if (!s_perf_t0) s_perf_t0 = a;
    if (c - s_perf_t0 >= 5000000) {
        ESP_LOGI("arcilla", "%.1f cuadros/s · dibujo %d ms · envío %d ms (por cuadro)", s_frames * 1e6 / (double)(c - s_perf_t0),
                 (int)(s_draw_us / s_frames / 1000), (int)(s_send_us / s_frames / 1000));
        s_perf_t0 = c; s_draw_us = s_send_us = 0; s_frames = 0;
    }
}
#else
static void arcilla_tick(uint32_t dt_ms) { (void)dt_ms; ui_frame(); flush(); }
#endif

static void arcilla_exit(void) { ui_holding(false, 0); /* la partida queda en memoria hasta apagar la placa */ }

const app_t app_arcilla = {
    .id = APP_ID,
    .name = "Arcilla",
    .version = ARCILLA_VERSION,
    .icon = LV_SYMBOL_SHUFFLE,
    .color = 0xD08850,
    .flags = APP_FLAG_RAW_DISPLAY,
    .tick_ms = 33,
    .on_enter = arcilla_enter,
    .on_knob = arcilla_knob,
    .on_touch = arcilla_touch,
    .on_tick = arcilla_tick,
    .on_exit = arcilla_exit,
};
