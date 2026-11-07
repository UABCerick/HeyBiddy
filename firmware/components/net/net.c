/*
 * net.c — Wi-Fi en modo estacion + SNTP.
 *
 * Todo ocurre en las tareas propias de ESP-IDF (eventos de Wi-Fi, lwIP y esp_timer),
 * nunca en la tarea de la interfaz:
 *   - la red se elige en la placa (Ajustes -> Wi-Fi) y se guarda en NVS ("net": ssid, pass);
 *   - al arrancar, si hay red guardada, se conecta;
 *   - si se pierde la conexion, reintenta con un temporizador (2 s, 4 s, 8 s... maximo 30 s);
 *   - con IP, arranca SNTP; cuando llega la hora, net_time_synced() pasa a true.
 * La zona horaria (Londres) la fija main.c con TZ.
 */
#include <string.h>
#include <stdlib.h>
#include "net.h"
#include "storage.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "net";

#define RETRY_FIRST_MS 2000
#define RETRY_MAX_MS   30000
#define MAX_APS        20

static volatile bool s_connected;
static volatile bool s_synced;
static volatile net_status_t s_status = NET_NO_NETWORK;
static bool s_started;
static bool s_sntp_started;
static uint32_t s_retry_ms = RETRY_FIRST_MS;
static esp_timer_handle_t s_retry_timer;

static char s_ssid[33];
static char s_pass[65];
static bool s_have_net;

/* busqueda de redes */
static volatile bool s_scanning;
static int64_t s_scan_started_ms;
#define SCAN_TIMEOUT_MS 10000
static net_ap_t s_aps[MAX_APS];
static int s_nap;
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;

bool net_is_connected(void) { return s_connected; }
bool net_time_synced(void) { return s_synced; }
net_status_t net_status(void) { return s_status; }

bool net_saved_ssid(char *out, size_t len)
{
    if (!s_have_net) return false;
    strncpy(out, s_ssid, len);
    out[len - 1] = 0;
    return true;
}

static void on_time_sync(struct timeval *tv)
{
    s_synced = true;
    ESP_LOGI(TAG, "hora sincronizada por SNTP");
}

static void start_sntp(void)
{
    if (!s_sntp_started) {
        esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_HEYBIDDY_SNTP_SERVER);
        cfg.sync_cb = on_time_sync;
        if (esp_netif_sntp_init(&cfg) == ESP_OK) s_sntp_started = true;
    } else {
        esp_netif_sntp_start();          /* al reconectar: pedir la hora otra vez */
    }
}

static void schedule_retry(uint32_t ms)
{
    esp_timer_stop(s_retry_timer);       /* por si ya habia uno pendiente */
    esp_timer_start_once(s_retry_timer, (uint64_t)ms * 1000);
}

static void retry_cb(void *arg)
{
    if (!s_have_net || s_connected || s_scanning) return;
    ESP_LOGI(TAG, "conectando a \"%s\"...", s_ssid);
    esp_wifi_connect();
}

static void apply_config(void)
{
    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, s_ssid, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, s_pass, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = s_pass[0] ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;     /* hotspots WPA3 o mixtos */
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
}

static void collect_scan(void)
{
    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n > 40) n = 40;
    wifi_ap_record_t *rec = calloc(n ? n : 1, sizeof(*rec));
    if (!rec) {
        esp_wifi_clear_ap_list();
        n = 0;
    } else {
        esp_wifi_scan_get_ap_records(&n, rec);   /* vienen ordenadas por senal */
    }

    net_ap_t tmp[MAX_APS];
    int k = 0;
    for (int i = 0; i < n && k < MAX_APS; i++) {
        const char *ssid = (const char *)rec[i].ssid;
        if (!ssid[0]) continue;                  /* redes ocultas */
        bool dup = false;
        for (int j = 0; j < k; j++) if (!strcmp(tmp[j].ssid, ssid)) dup = true;
        if (dup) continue;
        strncpy(tmp[k].ssid, ssid, sizeof(tmp[k].ssid));
        tmp[k].ssid[32] = 0;
        tmp[k].rssi = rec[i].rssi;
        tmp[k].secure = rec[i].authmode != WIFI_AUTH_OPEN;
        k++;
    }
    free(rec);

    portENTER_CRITICAL(&s_lock);
    memcpy(s_aps, tmp, sizeof(tmp));
    s_nap = k;
    s_scanning = false;
    portEXIT_CRITICAL(&s_lock);
    ESP_LOGI(TAG, "busqueda: %d redes (de %u vistas)", k, (unsigned)n);
    for (int i = 0; i < k; i++) ESP_LOGI(TAG, "  %-32s %d dBm%s", tmp[i].ssid, tmp[i].rssi, tmp[i].secure ? "" : " abierta");
}

static bool is_wrong_password(uint8_t r)
{
    return r == WIFI_REASON_AUTH_FAIL || r == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
           r == WIFI_REASON_HANDSHAKE_TIMEOUT || r == WIFI_REASON_MIC_FAILURE;
}

static bool is_not_found(uint8_t r)
{
    return r == WIFI_REASON_NO_AP_FOUND || r == WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY ||
           r == WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD || r == WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD;
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        if (s_have_net) schedule_retry(10);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = data;
        if (s_connected) ESP_LOGW(TAG, "se perdio la conexion (motivo %d)", d->reason);
        else ESP_LOGI(TAG, "no se pudo conectar (motivo %d)", d->reason);
        s_connected = false;
        if (!s_have_net) {
            s_status = NET_NO_NETWORK;
            return;
        }
        if (is_wrong_password(d->reason)) s_status = NET_WRONG_PASSWORD;
        else if (is_not_found(d->reason)) s_status = NET_NOT_FOUND;
        else if (s_status == NET_CONNECTED) s_status = NET_CONNECTING;
        if (s_scanning) return;                  /* se reintenta al terminar la busqueda */
        schedule_retry(s_retry_ms);
        s_retry_ms = s_retry_ms * 2 > RETRY_MAX_MS ? RETRY_MAX_MS : s_retry_ms * 2;
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        collect_scan();
        if (s_have_net && !s_connected) schedule_retry(300);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "conectado a \"%s\", IP " IPSTR, s_ssid, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_status = NET_CONNECTED;
        s_retry_ms = RETRY_FIRST_MS;
        start_sntp();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        s_connected = false;
    }
}

bool net_scan_start(void)
{
    if (!s_started) return false;
    portENTER_CRITICAL(&s_lock);
    bool busy = s_scanning;
    s_scanning = true;
    portEXIT_CRITICAL(&s_lock);
    if (busy) {
        /* una busqueda que no termina en 10 s se cancela y se vuelve a pedir */
        if (esp_timer_get_time() / 1000 - s_scan_started_ms < SCAN_TIMEOUT_MS) return true;
        ESP_LOGW(TAG, "la busqueda no termino: se cancela");
        esp_wifi_scan_stop();
    }
    s_scan_started_ms = esp_timer_get_time() / 1000;
    ESP_LOGI(TAG, "buscando redes...");

    if (!s_connected) {
        /* un intento de conexion en curso no deja buscar: se pausa y se retoma al terminar */
        esp_timer_stop(s_retry_timer);
        esp_wifi_disconnect();
    }
    wifi_scan_config_t sc = {.show_hidden = false};
    esp_err_t err = esp_wifi_scan_start(&sc, false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "no se pudo buscar redes: %s", esp_err_to_name(err));
        portENTER_CRITICAL(&s_lock);
        s_scanning = false;
        portEXIT_CRITICAL(&s_lock);
        if (s_have_net && !s_connected) schedule_retry(1000);
        return false;
    }
    return true;
}

int net_scan_results(net_ap_t *out, int max)
{
    if (s_scanning && esp_timer_get_time() / 1000 - s_scan_started_ms > SCAN_TIMEOUT_MS) {
        ESP_LOGW(TAG, "la busqueda no termino en %d s", SCAN_TIMEOUT_MS / 1000);
        esp_wifi_scan_stop();
        portENTER_CRITICAL(&s_lock);
        s_scanning = false;
        s_nap = 0;
        portEXIT_CRITICAL(&s_lock);
        if (s_have_net && !s_connected) schedule_retry(300);
    }
    portENTER_CRITICAL(&s_lock);
    int n = -1;
    if (!s_scanning) {
        n = s_nap < max ? s_nap : max;
        memcpy(out, s_aps, (size_t)n * sizeof(net_ap_t));
    }
    portEXIT_CRITICAL(&s_lock);
    return n;
}

void net_connect(const char *ssid, const char *pass)
{
    strncpy(s_ssid, ssid, sizeof(s_ssid));
    s_ssid[sizeof(s_ssid) - 1] = 0;
    strncpy(s_pass, pass ? pass : "", sizeof(s_pass));
    s_pass[sizeof(s_pass) - 1] = 0;
    s_have_net = true;
    storage_set_str(NET_NS, "ssid", s_ssid);
    storage_set_str(NET_NS, "pass", s_pass);
    ESP_LOGI(TAG, "red guardada: \"%s\"", s_ssid);
    if (!s_started) return;

    s_status = NET_CONNECTING;
    s_retry_ms = RETRY_FIRST_MS;
    esp_timer_stop(s_retry_timer);
    s_connected = false;
    esp_wifi_disconnect();
    apply_config();
    schedule_retry(300);
}

void net_forget(void)
{
    s_have_net = false;
    s_ssid[0] = s_pass[0] = 0;
    storage_erase_key(NET_NS, "ssid");
    storage_erase_key(NET_NS, "pass");
    s_status = NET_NO_NETWORK;
    ESP_LOGI(TAG, "red olvidada");
    if (!s_started) return;
    esp_timer_stop(s_retry_timer);
    esp_wifi_disconnect();
    s_connected = false;
}

void net_start(void)
{
    if (storage_get_str(NET_NS, "ssid", s_ssid, sizeof(s_ssid)) == ESP_OK && s_ssid[0]) {
        if (storage_get_str(NET_NS, "pass", s_pass, sizeof(s_pass)) != ESP_OK) s_pass[0] = 0;
        s_have_net = true;
        s_status = NET_CONNECTING;
    } else {
        ESP_LOGI(TAG, "sin red guardada: elige una en Ajustes -> Wi-Fi");
    }

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    esp_netif_create_default_wifi_sta();

    const esp_timer_create_args_t targs = {.callback = retry_cb, .name = "wifi_retry"};
    ESP_ERROR_CHECK(esp_timer_create(&targs, &s_retry_timer));

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));   /* la red la guardamos nosotros en "net" */

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (s_have_net) apply_config();
    ESP_ERROR_CHECK(esp_wifi_start());
    s_started = true;
}
