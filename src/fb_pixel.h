/*
 * fb_pixel.h — Format-Aware Pixel Access Helpers
 * Adapted from Tutorial-OS for Pico SDK.
 */
#ifndef FB_PIXEL_H
#define FB_PIXEL_H

#include "hal_types.h"

struct framebuffer;  /* forward declaration */

/* RGB565 ↔ ARGB8888 */
static inline uint16_t fb_argb_to_565(uint32_t argb)
{
    uint16_t r = (argb >> 19) & 0x1F;
    uint16_t g = (argb >> 10) & 0x3F;
    uint16_t b = (argb >>  3) & 0x1F;
    return (r << 11) | (g << 5) | b;
}

static inline uint32_t fb_565_to_argb(uint16_t c)
{
    uint8_t r = ((c >> 11) & 0x1F);
    uint8_t g = ((c >>  5) & 0x3F);
    uint8_t b = ( c        & 0x1F);
    r = (r << 3) | (r >> 2);
    g = (g << 2) | (g >> 4);
    b = (b << 3) | (b >> 2);
    return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

/* Row access and pixel read/write are implemented in framebuffer.c
 * as FB_ROW / FB_WRITE_PX / FB_READ_PX macros that call _impl functions
 * with access to the full framebuffer_t struct. */

#endif /* FB_PIXEL_H */
