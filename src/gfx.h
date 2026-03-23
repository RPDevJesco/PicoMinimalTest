/*
 * gfx.h — Text rendering and higher-level graphics
 *
 * Provides bitmap font rendering (built-in 5×7 + scaled variants)
 * and convenience drawing helpers on top of ili9488.
 */

#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <stdbool.h>

/* ---- Text rendering -------------------------------------------------- */

/* Draw a single character at (x,y) with given scale (1 = 5×7 px) */
void gfx_draw_char(uint16_t x, uint16_t y, char c,
                   uint16_t fg, uint16_t bg, uint8_t scale);

/* Draw a null-terminated string.  Returns X position after last char. */
uint16_t gfx_draw_string(uint16_t x, uint16_t y, const char *str,
                         uint16_t fg, uint16_t bg, uint8_t scale);

/* Draw string centred horizontally on screen */
void gfx_draw_string_centered(uint16_t y, const char *str,
                               uint16_t fg, uint16_t bg, uint8_t scale);

/* String width in pixels */
uint16_t gfx_string_width(const char *str, uint8_t scale);

/* ---- Convenience wrappers -------------------------------------------- */

/* Progress bar */
void gfx_progress_bar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      uint8_t pct, uint16_t fg, uint16_t bg, uint16_t border);

/* Colour gradient fill (horizontal, from c1 to c2) */
void gfx_gradient_h(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                    uint16_t c1, uint16_t c2);

/* Colour gradient fill (vertical) */
void gfx_gradient_v(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                    uint16_t c1, uint16_t c2);

/* Colour wheel: hue 0–359 → RGB565 */
uint16_t gfx_color_wheel(uint16_t hue);

/* Interpolate between two RGB565 colours (t = 0..255) */
uint16_t gfx_lerp_color(uint16_t c1, uint16_t c2, uint8_t t);

#endif /* GFX_H */
