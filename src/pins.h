/*
 * pins.h — LAFVIN Pico 2 Dev Kit pin mapping
 */
#ifndef PINS_H
#define PINS_H

#define DISP_SPI_INST   spi0
#define DISP_SPI_BAUD   (10 * 1000 * 1000)

#define PIN_SCK         2
#define PIN_MOSI        3
#define PIN_MISO        4
#define PIN_CS          5
#define PIN_DC          6
#define PIN_RST         7

#define PIN_TP_SDA      8
#define PIN_TP_SCL      9
#define PIN_TP_RST      10
#define PIN_TP_INT      11

#define PIN_RGB_LED     12
#define PIN_BUZZER      13
#define PIN_BTN_K2      14
#define PIN_BTN_K1      15
#define PIN_LED         25

/* Joystick (analog stick — ADC) */
#define PIN_JOY_X       26      /* ADC0 */
#define PIN_JOY_Y       27      /* ADC1 */

#define DISP_WIDTH      320
#define DISP_HEIGHT     480

#endif
