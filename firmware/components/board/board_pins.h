#pragma once
/*
 * Pines de la Waveshare ESP32-S3-Knob-Touch-LCD-1.8.
 * Fuente: main/user_config.h de los demos oficiales 04_Encoder_Test y 08_LVGL_Test.
 */
#include "driver/gpio.h"

/* Pantalla 360x360, controlador ST77916 por QSPI (se maneja con el driver esp_lcd_sh8601) */
#define BOARD_LCD_H_RES     360
#define BOARD_LCD_V_RES     360
#define BOARD_LCD_SPI_HOST  SPI2_HOST
#define BOARD_PIN_LCD_CS    GPIO_NUM_14
#define BOARD_PIN_LCD_PCLK  GPIO_NUM_13
#define BOARD_PIN_LCD_D0    GPIO_NUM_15
#define BOARD_PIN_LCD_D1    GPIO_NUM_16
#define BOARD_PIN_LCD_D2    GPIO_NUM_17
#define BOARD_PIN_LCD_D3    GPIO_NUM_18
#define BOARD_PIN_LCD_RST   GPIO_NUM_21
#define BOARD_PIN_LCD_BL    GPIO_NUM_47

/* Bus I2C compartido: touch CST816 (0x15) y driver de vibracion DRV2605 (0x5A) */
#define BOARD_I2C_PORT      I2C_NUM_0
#define BOARD_PIN_I2C_SCL   GPIO_NUM_12
#define BOARD_PIN_I2C_SDA   GPIO_NUM_11
#define BOARD_TOUCH_ADDR    0x15
#define BOARD_DRV2605_ADDR  0x5A

/* Perilla (encoder de dos interruptores, un evento por click) */
#define BOARD_PIN_KNOB_A    8
#define BOARD_PIN_KNOB_B    7
