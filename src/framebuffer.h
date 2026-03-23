/*
 * framebuffer.h — Portable Framebuffer Driver (Pico SDK adaptation)
 *
 * Adapted from Tutorial-OS for standalone Pico SDK use.
 * Includes are flattened; BCM/RISC-V/x86 paths removed.
 */
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "hal_types.h"

/* =============================================================================
 * CONFIGURATION
 * =============================================================================
 */
#ifndef FB_BUFFER_COUNT
#define FB_BUFFER_COUNT     1       /* Single-buffer on RP2350 (limited RAM) */
#endif

#ifndef FB_MAX_CLIP_DEPTH
#define FB_MAX_CLIP_DEPTH   8
#endif

#ifndef FB_MAX_DIRTY_RECTS
#define FB_MAX_DIRTY_RECTS  16
#endif

#ifndef FB_DEFAULT_WIDTH
#define FB_DEFAULT_WIDTH    320
#endif
#ifndef FB_DEFAULT_HEIGHT
#define FB_DEFAULT_HEIGHT   480
#endif

#define FB_BITS_PER_PIXEL   16      /* RGB565 on RP2350 */

#define FB_CHAR_WIDTH       8
#define FB_CHAR_HEIGHT      8
#define FB_CHAR_WIDTH_LG    8
#define FB_CHAR_HEIGHT_LG   16

/* =============================================================================
 * COLOR MACROS (ARGB8888 — drawing API always uses these)
 * =============================================================================
 */
#define FB_ARGB(a,r,g,b)    ((uint32_t)(((uint32_t)(a)<<24)|((uint32_t)(r)<<16)|((uint32_t)(g)<<8)|(uint32_t)(b)))
#define FB_RGB(r,g,b)       FB_ARGB(255, r, g, b)
#define FB_ALPHA(c)         (((uint32_t)(c) >> 24) & 0xFF)
#define FB_RED(c)           (((uint32_t)(c) >> 16) & 0xFF)
#define FB_GREEN(c)         (((uint32_t)(c) >>  8) & 0xFF)
#define FB_BLUE(c)          ( (uint32_t)(c)        & 0xFF)
#define FB_WITH_ALPHA(c,a)  (((uint32_t)(c) & 0x00FFFFFF) | ((uint32_t)(a) << 24))

/* =============================================================================
 * COLOR CONSTANTS
 * =============================================================================
 */
#define FB_COLOR_TRANSPARENT    0x00000000
#define FB_COLOR_BLACK          0xFF000000
#define FB_COLOR_WHITE          0xFFFFFFFF
#define FB_COLOR_RED            0xFFFF0000
#define FB_COLOR_GREEN          0xFF00FF00
#define FB_COLOR_BLUE           0xFF0000FF
#define FB_COLOR_YELLOW         0xFFFFFF00
#define FB_COLOR_CYAN           0xFF00FFFF
#define FB_COLOR_MAGENTA        0xFFFF00FF
#define FB_COLOR_GRAY           0xFF808080
#define FB_COLOR_LIGHT_GRAY     0xFFC0C0C0
#define FB_COLOR_DARK_GRAY      0xFF404040
#define FB_COLOR_ORANGE         0xFFFF8000
#define FB_COLOR_PURPLE         0xFF800080
#define FB_COLOR_TEAL           0xFF008080
#define FB_COLOR_PINK           0xFFFFC0CB
#define FB_COLOR_BROWN          0xFF8B4513

/* UI palette */
#define FB_COLOR_BG             0xFF1A1A2E
#define FB_COLOR_PRIMARY        0xFF4080FF
#define FB_COLOR_SUCCESS        0xFF40C080
#define FB_COLOR_WARNING        0xFFFFAA00
#define FB_COLOR_ERROR          0xFFFF4040
#define FB_COLOR_TEXT           0xFFE0E0E0
#define FB_COLOR_TEXT_DIM       0xFF808080

/* =============================================================================
 * BLEND MODES
 * =============================================================================
 */
typedef enum {
    FB_BLEND_OPAQUE,
    FB_BLEND_ALPHA,
    FB_BLEND_ADDITIVE,
    FB_BLEND_MULTIPLY,
} fb_blend_mode_t;

/* =============================================================================
 * DATA STRUCTURES
 * =============================================================================
 */
typedef struct { int32_t x; int32_t y; uint32_t w; uint32_t h; } fb_rect_t;
typedef struct { uint32_t x; uint32_t y; uint32_t w; uint32_t h; } fb_clip_t;
typedef struct { uint32_t x; uint32_t y; uint32_t w; uint32_t h; } fb_dirty_t;
typedef struct { uint32_t width; uint32_t height; const uint32_t *data; } fb_bitmap_t;

/* =============================================================================
 * FRAMEBUFFER STRUCTURE
 * =============================================================================
 */
typedef struct framebuffer {
    uint32_t *addr;
    uint32_t  width;
    uint32_t  height;
    uint32_t  pitch;

    uint32_t *buffers[FB_BUFFER_COUNT];
    uint32_t  front_buffer;
    uint32_t  back_buffer;
    size_t    buffer_size;
    uint32_t  virtual_height;

    fb_dirty_t dirty_rects[FB_MAX_DIRTY_RECTS];
    uint32_t   dirty_count;
    bool       full_dirty;

    fb_clip_t clip_stack[FB_MAX_CLIP_DEPTH];
    uint32_t  clip_depth;

    uint64_t         frame_count;
    bool             vsync_enabled;
    bool             initialized;
    fb_pixel_format_t pixel_format;
} framebuffer_t;

/* =============================================================================
 * API
 * =============================================================================
 */

/* Init / present */
bool fb_init(framebuffer_t *fb);
void fb_present(framebuffer_t *fb);

/* Accessors */
static inline uint32_t fb_width(const framebuffer_t *fb)  { return fb->width; }
static inline uint32_t fb_height(const framebuffer_t *fb) { return fb->height; }
static inline size_t   fb_size(const framebuffer_t *fb)   { return fb->buffer_size; }

/* Clipping */
bool     fb_push_clip(framebuffer_t *fb, fb_rect_t rect);
void     fb_pop_clip(framebuffer_t *fb);
void     fb_reset_clip(framebuffer_t *fb);

/* Dirty tracking */
void fb_mark_dirty(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
void fb_mark_all_dirty(framebuffer_t *fb);
void fb_clear_dirty(framebuffer_t *fb);
bool fb_is_dirty(const framebuffer_t *fb);

/* Drawing */
void fb_clear(framebuffer_t *fb, uint32_t color);
void fb_put_pixel(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t color);
void fb_put_pixel_blend(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t color);
void fb_put_pixel_unchecked(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t color);
uint32_t fb_get_pixel(const framebuffer_t *fb, uint32_t x, uint32_t y);

void fb_fill_rect(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_fill_rect_blend(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_rect(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);
void fb_draw_hline(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t len, uint32_t color);
void fb_draw_vline(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t len, uint32_t color);
void fb_draw_line(framebuffer_t *fb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);

void fb_fill_circle(framebuffer_t *fb, int32_t cx, int32_t cy, uint32_t radius, uint32_t color);
void fb_draw_circle(framebuffer_t *fb, int32_t cx, int32_t cy, uint32_t radius, uint32_t color);
void fb_fill_triangle(framebuffer_t *fb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
void fb_fill_rounded_rect(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t radius, uint32_t color);
void fb_draw_rounded_rect(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t radius, uint32_t color);

void fb_fill_rect_gradient_v(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t top, uint32_t bot);
void fb_fill_rect_gradient_h(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t left, uint32_t right);

/* Color utilities */
uint32_t fb_blend_alpha(uint32_t src, uint32_t dst);
uint32_t fb_color_lerp(uint32_t c1, uint32_t c2, uint8_t t);

/* Text */
void fb_draw_char(framebuffer_t *fb, uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg);
void fb_draw_char_transparent(framebuffer_t *fb, uint32_t x, uint32_t y, char c, uint32_t fg);
void fb_draw_string(framebuffer_t *fb, uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg);
void fb_draw_string_transparent(framebuffer_t *fb, uint32_t x, uint32_t y, const char *str, uint32_t fg);
void fb_draw_string_scaled(framebuffer_t *fb, uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg, uint32_t scale);
void fb_draw_string_scaled_transparent(framebuffer_t *fb, uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t scale);
void fb_draw_string_centered(framebuffer_t *fb, uint32_t y, const char *str, uint32_t fg, uint32_t bg);
void fb_draw_char_scaled(framebuffer_t *fb, uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg, uint32_t scale);
uint32_t fb_text_width(const char *str);
uint32_t fb_text_width_large(const char *str);
uint32_t fb_measure_string(const char *str, bool large);

/* Scroll */
void fb_scroll_v(framebuffer_t *fb, int32_t pixels, uint32_t fill_color);
void fb_copy_rect(framebuffer_t *fb, uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, uint32_t w, uint32_t h);

#endif /* FRAMEBUFFER_H */
