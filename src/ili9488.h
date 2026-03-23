/*
 * ili9488.h — ILI9488 SPI driver for RP2350 (Pico 2)
 *
 * The ILI9488 in SPI mode only supports 18-bit colour (RGB666,
 * 3 bytes per pixel).  API accepts RGB565 for convenience; the
 * driver converts internally.
 */

#ifndef ILI9488_H
#define ILI9488_H

#include <stdint.h>
#include <stdbool.h>

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
#define COLOR_ORANGE    RGB565(0xFF, 0xA5, 0x00)
#define COLOR_PURPLE    RGB565(0x80, 0x00, 0x80)
#define COLOR_PINK      RGB565(0xFF, 0x69, 0xB4)
#define COLOR_GREY      RGB565(0x80, 0x80, 0x80)
#define COLOR_DARKGREY  RGB565(0x40, 0x40, 0x40)
#define COLOR_LIGHTGREY RGB565(0xC0, 0xC0, 0xC0)

/* Darker / lighter helper — quick brightness shift */
#define COLOR_DIM(c)    (((c) >> 1) & 0x7BEF)
#define COLOR_BRIGHT(c) (((c) | 0x8410) & 0xFFFF)

/* ---- Public API ------------------------------------------------------ */

void ili9488_init(void);

/* Whole-screen fill */
void ili9488_fill(uint16_t color);

/* Filled rectangle (inclusive coords) */
void ili9488_fill_rect(uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1,
                       uint16_t color);

/* Single pixel */
void ili9488_pixel(uint16_t x, uint16_t y, uint16_t color);

/* Horizontal / vertical fast lines */
void ili9488_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void ili9488_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color);

/* Outlined rectangle */
void ili9488_rect(uint16_t x0, uint16_t y0,
                  uint16_t x1, uint16_t y1,
                  uint16_t color);

/* Rounded rectangle (filled) */
void ili9488_fill_rounded_rect(uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1,
                               uint16_t r, uint16_t color);

/* Arbitrary line (Bresenham) */
void ili9488_line(int16_t x0, int16_t y0,
                  int16_t x1, int16_t y1,
                  uint16_t color);

/* Circle (filled and outline) */
void ili9488_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);
void ili9488_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color);

/* Triangle (filled) */
void ili9488_fill_triangle(int16_t x0, int16_t y0,
                           int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2,
                           uint16_t color);

/* Set rotation (0–3, each +90°) */
void ili9488_set_rotation(uint8_t rot);
uint16_t ili9488_width(void);
uint16_t ili9488_height(void);

#endif /* ILI9488_H */
