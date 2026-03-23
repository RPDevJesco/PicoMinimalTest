/*
 * rgbled.h — WS2812 (NeoPixel) RGB LED driver via PIO
 */

#ifndef RGBLED_H
#define RGBLED_H

#include <stdint.h>

/* Initialise PIO for WS2812 on the RGB LED pin */
void rgbled_init(void);

/* Set colour (0xRRGGBB) */
void rgbled_set(uint32_t rgb);

/* Set colour from separate R, G, B (0–255 each) */
void rgbled_set_rgb(uint8_t r, uint8_t g, uint8_t b);

/* Turn off */
void rgbled_off(void);

/* Set from hue (0–359), full saturation & brightness */
void rgbled_set_hue(uint16_t hue);

#endif /* RGBLED_H */
