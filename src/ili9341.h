/*
 * ili9341.h — Minimal ILI9341 driver for RP2350 over SPI
 */

#ifndef ILI9341_H
#define ILI9341_H

#include <stdint.h>

/* ---- Colour helpers (RGB565, big-endian wire order) ------------------ */
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

/*  Initialise SPI, GPIO, reset the panel, configure MADCTL/COLMOD.
 *  Call once at boot before any drawing functions.                       */
void ili9341_init(void);

/*  Fill the entire screen with a single RGB565 colour.                  */
void ili9341_fill(uint16_t color);

/*  Fill a rectangular region (inclusive coords).
 *  Caller must ensure x0 <= x1 < DISP_WIDTH, y0 <= y1 < DISP_HEIGHT.  */
void ili9341_fill_rect(uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1,
                       uint16_t color);

/*  Set a single pixel.                                                  */
void ili9341_pixel(uint16_t x, uint16_t y, uint16_t color);

#endif /* ILI9341_H */
