/*
 * rgbled.c — WS2812 RGB LED via PIO
 */

#include "rgbled.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "ws2812.pio.h"

static PIO  _pio;
static uint _sm;

void rgbled_init(void)
{
    _pio = pio0;
    _sm  = pio_claim_unused_sm(_pio, true);
    uint offset = pio_add_program(_pio, &ws2812_program);
    ws2812_program_init(_pio, _sm, offset, PIN_RGB_LED, 800000, false);

    /* Start dark */
    rgbled_off();
}

static void put_pixel(uint32_t grb)
{
    pio_sm_put_blocking(_pio, _sm, grb << 8u);
}

void rgbled_set(uint32_t rgb)
{
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >>  8) & 0xFF;
    uint8_t b =  rgb        & 0xFF;
    /* WS2812 expects GRB order */
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    put_pixel(grb);
}

void rgbled_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    put_pixel(grb);
}

void rgbled_off(void)
{
    put_pixel(0);
}

void rgbled_set_hue(uint16_t hue)
{
    hue %= 360;
    uint8_t region = hue / 60;
    uint8_t frac   = (uint8_t)((hue % 60) * 255 / 60);
    uint8_t r, g, b;

    switch (region) {
        case 0:  r = 255; g = frac;       b = 0;           break;
        case 1:  r = 255 - frac; g = 255; b = 0;           break;
        case 2:  r = 0;   g = 255;        b = frac;        break;
        case 3:  r = 0;   g = 255 - frac; b = 255;         break;
        case 4:  r = frac; g = 0;         b = 255;         break;
        default: r = 255; g = 0;          b = 255 - frac;  break;
    }
    rgbled_set_rgb(r, g, b);
}
