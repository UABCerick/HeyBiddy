/*
 * net.c — Wi-Fi en modo estacion + SNTP.
 *
 * Todo ocurre en las tareas propias de ESP-IDF (eventos de Wi-Fi, lwIP y esp_timer),
 * nunca en la tarea de la interfaz:
 *   - al arrancar se conecta al SSID de menuconfig;
 *   - si se pierde la conexion, reintenta con un temporizador (2 s, 4 s, 8 s... maximo 30 s);
 *   - con IP, arranca SNTP; cuando llega la hora, net_time_synced() pasa a true.
 * La zona horaria (Londres) la fija main.c con TZ.
 * La configuracion del Wi-Fi se queda en RAM: no escribe nada en NVS.
 */
#include <string.h>
#include "net.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_event.h"
#include "esp_timer.h"

static const char *TAG = "net";

#define RETRY_FIRST_MS 2000
#define RETRY_MAX_MS   30000

static volatile bool s_connected;
static volatile bool s_synced;
static bool s_sntp_started;
static uint32_t s_retry_ms = RETRY_FIRST_MS;
static esp_timer_handle_t s_retry_timer;

bool net_is_connected(void) { return s_connected; }
bool net_time_synced(void) { return s_synced; }

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

static void retry_cb(void *arg)
{
    ESP_LOGI(TAG, "reintentando conectar a \"%s\"", CONFIG_HEYBIDDY_WIFI_SSID);
    esp_wifi_connect();
}

static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = data;
        if (s_connected) ESP_LOGW(TAG, "se perdio la conexion (motivo %d)", d->reason);
        s_connected = false;
        esp_timer_stop(s_retry_timer);   /* por si ya habia uno pendiente */
        esp_timer_start_once(s_retry_timer, (uint64_t)s_retry_ms * 1000);
        s_retry_ms = s_retry_ms * 2 > RETRY_MAX_MS ? RETRY_MAX_MS : s_retry_ms * 2;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "conectado, IP " IPSTR, IP2STR(&e->ip_info.ip));
        s_connected = true;
        s_retry_ms = RETRY_FIRST_MS;
        start_sntp();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        s_connected = false;
    }
}

void net_start(void)
{
    if (strlen(CONFIG_HEYBIDDY_WIFI_SSID) == 0) {
        ESP_LOGW(TAG, "sin SSID: pon el hotspot en idf.py menuconfig -> Hey Biddy: Wi-Fi");
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) ESP_ERROR_CHECK(err);
    esp_netif_create_default_wifi_sta();

    const esp_timer_create_args_t targs = {.callback = retry_cb, .name = "wifi_retry"};
    ESP_ERROR_CHECK(esp_timer_create(&targs, &s_retry_timer));

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));

    wifi_config_t cfg = {0};
    strncpy((char *)cfg.sta.ssid, CONFIG_HEYBIDDY_WIFI_SSID, sizeof(cfg.sta.ssid));
    strncpy((char *)cfg.sta.password, CONFIG_HEYBIDDY_WIFI_PASSWORD, sizeof(cfg.sta.password));
    cfg.sta.threshold.authmode = strlen(CONFIG_HEYBIDDY_WIFI_PASSWORD) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    cfg.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;     /* hotspots WPA3 o mixtos */

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "conectando a \"%s\"...", CONFIG_HEYBIDDY_WIFI_SSID);
}
