/*
 * Hey Biddy — arranque.
 * 1. memoria de datos (NVS)  2. hardware  3. sistema con todas las apps  4. Wi-Fi y hora
 */
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "storage.h"
#include "board.h"
#include "ui_core.h"
#include "registry.h"
#include "net.h"

void app_main(void)
{
    ESP_ERROR_CHECK(storage_init());

    setenv("TZ", "GMT0BST,M3.5.0/1,M10.5.0", 1);   /* Londres */
    tzset();

    ESP_ERROR_CHECK(board_init());
    ui_core_start(g_apps, g_app_count);
    net_start();        /* despues de la pantalla: sus bufferes de RAM interna van primero */
}
