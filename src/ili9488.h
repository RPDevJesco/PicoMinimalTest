/*
 * ili9488.h — Minimal ILI9488 driver for RP2350 over SPI
 *
 * Key difference from ILI9341: the ILI9488 in SPI mode only
 * supports 18-bit colour (RGB666, 3 bytes per pixel).  The
 * API still accepts RGB565 for convenience; the driver converts
 * internally.
 */

#ifndef ILI9488_H
#define ILI9488_H

#include <stdint.h>

/* ---- Colour helpers (RGB565 for API convenience) --------------------- */
#define RGB565(r, g, b) \
    ((uint16_t)(((r) & 0xF8) << 8 | ((g) & 0xFC) << 3 | ((b) >> 3)))

#define COLOR_BLACK     RGB565(0x00, 0x00, 0x00)
#define COLOR_WHITE     RGB565(0xFF, 0xFF, 0xFF)
#define COLOR_RED       RGB565(0xFF, 0x00, 0x00)
#define COLOR_GREEN     RGB565(0x00, 0xFF, 0x00)
#define COLOR_BLUE      RGB565(0x00, 0x00, 0xFF)
#define COLOR_CYAN      RGB565(0x00, 0xFF, 0xFF)
#define COLOR_MAGENTA   RGB565(0xFF, 0x00, 0xFF)
#define COLOR_YELLOW    RGB565(0xFF, 0xFF, 0x00)

/* ---- Public API ------------------------------------------------------ */

void ili9488_init(void);
void ili9488_fill(uint16_t color);
void ili9488_fill_rect(uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1,
                       uint16_t color);
void ili9488_pixel(uint16_t x, uint16_t y, uint16_t color);

#endif /* ILI9488_H */
