/*
 * board.c — drivers de la Waveshare ESP32-S3-Knob-Touch-LCD-1.8
 *
 * Basado en los demos oficiales de Waveshare (08_LVGL_Test, 04_Encoder_Test,
 * 03_DRV2605_Test). Aqui no hay nada de interfaz: solo hardware.
 */
#include <string.h>
#include "board.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_sh8601.h"
#include "bidi_switch_knob.h"

static const char *TAG = "board";

extern const sh8601_lcd_init_cmd_t board_lcd_init_cmds[];
extern const unsigned board_lcd_init_cmds_count;

static esp_lcd_panel_handle_t s_panel;
static board_lcd_done_cb_t s_done_cb;
static void *s_done_ctx;

static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_touch_dev;
static i2c_master_dev_handle_t s_haptic_dev;
static SemaphoreHandle_t s_i2c_lock;

static board_knob_cb_t s_knob_cb;

/* ------------------------------------------------------------------ I2C */

static esp_err_t i2c_write_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    xSemaphoreTake(s_i2c_lock, portMAX_DELAY);
    esp_err_t err = i2c_master_transmit(dev, buf, 2, 50);
    xSemaphoreGive(s_i2c_lock);
    return err;
}

static esp_err_t i2c_read_reg(i2c_master_dev_handle_t dev, uint8_t reg, uint8_t *out, size_t len)
{
    xSemaphoreTake(s_i2c_lock, portMAX_DELAY);
    esp_err_t err = i2c_master_transmit_receive(dev, &reg, 1, out, len, 50);
    xSemaphoreGive(s_i2c_lock);
    return err;
}

static esp_err_t i2c_init(void)
{
    s_i2c_lock = xSemaphoreCreateMutex();
    i2c_master_bus_config_t bus_cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = BOARD_I2C_PORT,
        .scl_io_num = BOARD_PIN_I2C_SCL,
        .sda_io_num = BOARD_PIN_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&bus_cfg, &s_i2c_bus), TAG, "bus I2C");

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .scl_speed_hz = 300000,
    };
    dev_cfg.device_address = BOARD_TOUCH_ADDR;
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_touch_dev), TAG, "touch");

    if (i2c_master_probe(s_i2c_bus, BOARD_DRV2605_ADDR, 50) == ESP_OK) {
        dev_cfg.device_address = BOARD_DRV2605_ADDR;
        ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_haptic_dev), TAG, "drv2605");
    } else {
        ESP_LOGW(TAG, "DRV2605 no encontrado: no habra vibracion");
    }
    return ESP_OK;
}

/* ------------------------------------------------------------- Pantalla */

static bool IRAM_ATTR lcd_done_isr(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    if (s_done_cb) {
        s_done_cb(s_done_ctx);
    }
    return false;
}

static esp_err_t lcd_init(void)
{
    const spi_bus_config_t buscfg = {
        .data0_io_num = BOARD_PIN_LCD_D0,
        .data1_io_num = BOARD_PIN_LCD_D1,
        .sclk_io_num = BOARD_PIN_LCD_PCLK,
        .data2_io_num = BOARD_PIN_LCD_D2,
        .data3_io_num = BOARD_PIN_LCD_D3,
        .max_transfer_sz = BOARD_LCD_H_RES * BOARD_LCD_V_RES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BOARD_LCD_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO), TAG, "bus SPI");

    esp_lcd_panel_io_handle_t io = NULL;
    const esp_lcd_panel_io_spi_config_t io_cfg =
        SH8601_PANEL_IO_QSPI_CONFIG(BOARD_PIN_LCD_CS, lcd_done_isr, NULL);
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)BOARD_LCD_SPI_HOST, &io_cfg, &io), TAG, "panel io");

    sh8601_vendor_config_t vendor_cfg = {
        .init_cmds = board_lcd_init_cmds,
        .init_cmds_size = board_lcd_init_cmds_count,
        .flags.use_qspi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BOARD_PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = &vendor_cfg,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_sh8601(io, &panel_cfg, &s_panel), TAG, "panel");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), TAG, "reset");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), TAG, "init");
    return ESP_OK;
}

void board_lcd_set_done_cb(board_lcd_done_cb_t cb, void *ctx)
{
    s_done_ctx = ctx;
    s_done_cb = cb;
}

esp_err_t board_lcd_draw(int x1, int y1, int x2, int y2, const void *pixels)
{
    return esp_lcd_panel_draw_bitmap(s_panel, x1, y1, x2, y2, pixels);
}

/* ------------------------------------------------------- Luz de fondo */

static void backlight_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_3,
        .freq_hz = 50 * 1000,
        .clk_cfg = LEDC_SLOW_CLK_RC_FAST,
    };
    ledc_channel_config_t ch = {
        .gpio_num = BOARD_PIN_LCD_BL,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_1,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_3,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_timer_config(&timer));
    ESP_ERROR_CHECK_WITHOUT_ABORT(ledc_channel_config(&ch));
}

void board_backlight_set(uint8_t percent)
{
    if (percent > 100) percent = 100;
    uint32_t duty = (uint32_t)percent * 255 / 100;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

/* ---------------------------------------------------------------- Touch */

static void touch_init(void)
{
    /* Igual que el demo: registro 0x00 = 0 pone el CST816 en modo normal */
    if (i2c_write_reg(s_touch_dev, 0x00, 0x00) != ESP_OK) {
        ESP_LOGW(TAG, "el touch no respondio al iniciar");
    }
}

bool board_touch_read(uint16_t *x, uint16_t *y)
{
    uint8_t d[7] = {0};
    if (i2c_read_reg(s_touch_dev, 0x00, d, sizeof(d)) != ESP_OK) {
        return false;
    }
    if (d[2] == 0) {
        return false;
    }
    uint16_t tx = ((uint16_t)(d[3] & 0x0F) << 8) | d[4];
    uint16_t ty = ((uint16_t)(d[5] & 0x0F) << 8) | d[6];
    if (tx >= BOARD_LCD_H_RES) tx = BOARD_LCD_H_RES - 1;
    if (ty >= BOARD_LCD_V_RES) ty = BOARD_LCD_V_RES - 1;
    *x = tx;
    *y = ty;
    return true;
}

/* ---------------------------------------------------------------- Perilla */

static void knob_left(void *arg, void *data)
{
    if (s_knob_cb) s_knob_cb(-1);
}

static void knob_right(void *arg, void *data)
{
    if (s_knob_cb) s_knob_cb(+1);
}

static void knob_init(void)
{
    knob_config_t cfg = {
        .gpio_encoder_a = BOARD_PIN_KNOB_A,
        .gpio_encoder_b = BOARD_PIN_KNOB_B,
    };
    knob_handle_t knob = iot_knob_create(&cfg);
    if (knob == NULL) {
        ESP_LOGE(TAG, "no se pudo crear la perilla");
        return;
    }
    iot_knob_register_cb(knob, KNOB_LEFT, knob_left, NULL);
    iot_knob_register_cb(knob, KNOB_RIGHT, knob_right, NULL);
}

void board_knob_set_cb(board_knob_cb_t cb)
{
    s_knob_cb = cb;
}

/* -------------------------------------------------------- Vibracion */

/* Registros del DRV2605 */
#define DRV_STATUS     0x00
#define DRV_MODE       0x01
#define DRV_RTPIN      0x02
#define DRV_LIBRARY    0x03
#define DRV_WAVESEQ1   0x04
#define DRV_WAVESEQ2   0x05
#define DRV_GO         0x0C
#define DRV_OVERDRIVE  0x0D
#define DRV_SUSTAINPOS 0x0E
#define DRV_SUSTAINNEG 0x0F
#define DRV_BREAK      0x10
#define DRV_AUDIOMAX   0x13
#define DRV_FEEDBACK   0x1A
#define DRV_CONTROL3   0x1D

static void haptic_init(void)
{
    if (!s_haptic_dev) return;
    uint8_t v = 0;
    /* Misma configuracion que SensorDRV2605::begin() + selectLibrary(5) del demo 03 */
    i2c_write_reg(s_haptic_dev, DRV_MODE, 0x00);        /* salir de standby, disparo interno */
    i2c_write_reg(s_haptic_dev, DRV_RTPIN, 0x00);
    i2c_write_reg(s_haptic_dev, DRV_WAVESEQ1, 1);
    i2c_write_reg(s_haptic_dev, DRV_WAVESEQ2, 0);
    i2c_write_reg(s_haptic_dev, DRV_OVERDRIVE, 0);
    i2c_write_reg(s_haptic_dev, DRV_SUSTAINPOS, 0);
    i2c_write_reg(s_haptic_dev, DRV_SUSTAINNEG, 0);
    i2c_write_reg(s_haptic_dev, DRV_BREAK, 0);
    i2c_write_reg(s_haptic_dev, DRV_AUDIOMAX, 0x64);
    if (i2c_read_reg(s_haptic_dev, DRV_FEEDBACK, &v, 1) == ESP_OK) {
        i2c_write_reg(s_haptic_dev, DRV_FEEDBACK, v & 0x7F);
    }
    if (i2c_read_reg(s_haptic_dev, DRV_CONTROL3, &v, 1) == ESP_OK) {
        i2c_write_reg(s_haptic_dev, DRV_CONTROL3, v | 0x20);
    }
    i2c_write_reg(s_haptic_dev, DRV_LIBRARY, 5);
    i2c_write_reg(s_haptic_dev, DRV_MODE, 0x00);
}

bool board_haptic_present(void)
{
    return s_haptic_dev != NULL;
}

void board_haptic_play(uint8_t effect)
{
    if (!s_haptic_dev) return;
    i2c_write_reg(s_haptic_dev, DRV_WAVESEQ1, effect);
    i2c_write_reg(s_haptic_dev, DRV_WAVESEQ2, 0);
    i2c_write_reg(s_haptic_dev, DRV_GO, 1);
}

/* -------------------------------------------------------------- Inicio */

esp_err_t board_init(void)
{
    backlight_init();                 /* empieza apagada: se enciende tras el primer cuadro */
    ESP_RETURN_ON_ERROR(lcd_init(), TAG, "pantalla");
    ESP_RETURN_ON_ERROR(i2c_init(), TAG, "i2c");
    touch_init();
    haptic_init();
    knob_init();
    ESP_LOGI(TAG, "placa lista (vibracion: %s)", s_haptic_dev ? "si" : "no");
    return ESP_OK;
}
