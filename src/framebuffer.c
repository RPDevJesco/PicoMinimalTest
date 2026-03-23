/*
 * framebuffer.c — Portable framebuffer drawing (Pico SDK adaptation)
 *
 * Adapted from Tutorial-OS. All drawing functions accept ARGB8888 colors.
 * On RP2350, the shadow buffer is RGB565; fb_pixel helpers convert on write.
 * The display_spi.c present function converts RGB565→RGB666 for the wire.
 */

#include "framebuffer.h"
#include "fb_pixel.h"

#include <string.h>

/* RP2350: no cache maintenance needed */
static inline void clean_dcache_range(uintptr_t addr, size_t size)
{
    (void)addr; (void)size;
}

/* ---- fb_pixel inline implementations (need full struct) -------------- */

static inline uint8_t *fb_row_impl(const framebuffer_t *fb, uint32_t y)
{
    return (uint8_t *)fb->addr + y * fb->pitch;
}

static inline void fb_write_px_impl(const framebuffer_t *fb,
                                     uint8_t *row, uint32_t x, uint32_t color)
{
    if (fb->pixel_format == FB_FORMAT_RGB565) {
        uint16_t *px = (uint16_t *)row + x;
        *px = fb_argb_to_565(color);
    } else {
        uint32_t *px = (uint32_t *)row + x;
        *px = color;
    }
}

static inline uint32_t fb_read_px_impl(const framebuffer_t *fb,
                                        const uint8_t *row, uint32_t x)
{
    if (fb->pixel_format == FB_FORMAT_RGB565) {
        const uint16_t *px = (const uint16_t *)row + x;
        return fb_565_to_argb(*px);
    } else {
        const uint32_t *px = (const uint32_t *)row + x;
        return *px;
    }
}

/* Use these throughout this file */
#define FB_ROW(fb, y)            fb_row_impl(fb, y)
#define FB_WRITE_PX(fb, r, x, c) fb_write_px_impl(fb, r, x, c)
#define FB_READ_PX(fb, r, x)    fb_read_px_impl(fb, r, x)

/* ---- 8x8 bitmap font (ASCII 32-126) --------------------------------- */

static const uint8_t font_8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
    {0x18,0x18,0x18,0x18,0x18,0x00,0x18,0x00},
    {0x6C,0x6C,0x24,0x00,0x00,0x00,0x00,0x00},
    {0x6C,0x6C,0xFE,0x6C,0xFE,0x6C,0x6C,0x00},
    {0x18,0x7E,0xC0,0x7C,0x06,0xFC,0x18,0x00},
    {0x00,0xC6,0xCC,0x18,0x30,0x66,0xC6,0x00},
    {0x38,0x6C,0x38,0x76,0xDC,0xCC,0x76,0x00},
    {0x18,0x18,0x30,0x00,0x00,0x00,0x00,0x00},
    {0x0C,0x18,0x30,0x30,0x30,0x18,0x0C,0x00},
    {0x30,0x18,0x0C,0x0C,0x0C,0x18,0x30,0x00},
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00},
    {0x00,0x18,0x18,0x7E,0x18,0x18,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x30},
    {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x18,0x18,0x00},
    {0x06,0x0C,0x18,0x30,0x60,0xC0,0x80,0x00},
    {0x7C,0xCE,0xDE,0xF6,0xE6,0xC6,0x7C,0x00},
    {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00},
    {0x7C,0xC6,0x06,0x7C,0xC0,0xC0,0xFE,0x00},
    {0xFC,0x06,0x06,0x3C,0x06,0x06,0xFC,0x00},
    {0x0C,0xCC,0xCC,0xCC,0xFE,0x0C,0x0C,0x00},
    {0xFE,0xC0,0xFC,0x06,0x06,0xC6,0x7C,0x00},
    {0x7C,0xC0,0xC0,0xFC,0xC6,0xC6,0x7C,0x00},
    {0xFE,0x06,0x06,0x0C,0x18,0x18,0x18,0x00},
    {0x7C,0xC6,0xC6,0x7C,0xC6,0xC6,0x7C,0x00},
    {0x7C,0xC6,0xC6,0x7E,0x06,0x06,0x7C,0x00},
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x00},
    {0x00,0x18,0x18,0x00,0x00,0x18,0x18,0x30},
    {0x0C,0x18,0x30,0x60,0x30,0x18,0x0C,0x00},
    {0x00,0x00,0x7E,0x00,0x7E,0x00,0x00,0x00},
    {0x30,0x18,0x0C,0x06,0x0C,0x18,0x30,0x00},
    {0x3C,0x66,0x0C,0x18,0x18,0x00,0x18,0x00},
    {0x7C,0xC6,0xDE,0xDE,0xDE,0xC0,0x7E,0x00},
    {0x38,0x6C,0xC6,0xC6,0xFE,0xC6,0xC6,0x00},
    {0xFC,0xC6,0xC6,0xFC,0xC6,0xC6,0xFC,0x00},
    {0x7C,0xC6,0xC0,0xC0,0xC0,0xC6,0x7C,0x00},
    {0xF8,0xCC,0xC6,0xC6,0xC6,0xCC,0xF8,0x00},
    {0xFE,0xC0,0xC0,0xF8,0xC0,0xC0,0xFE,0x00},
    {0xFE,0xC0,0xC0,0xF8,0xC0,0xC0,0xC0,0x00},
    {0x7C,0xC6,0xC0,0xCE,0xC6,0xC6,0x7C,0x00},
    {0xC6,0xC6,0xC6,0xFE,0xC6,0xC6,0xC6,0x00},
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x7E,0x00},
    {0x06,0x06,0x06,0x06,0xC6,0xC6,0x7C,0x00},
    {0xC6,0xCC,0xD8,0xF0,0xD8,0xCC,0xC6,0x00},
    {0xC0,0xC0,0xC0,0xC0,0xC0,0xC0,0xFE,0x00},
    {0xC6,0xEE,0xFE,0xD6,0xC6,0xC6,0xC6,0x00},
    {0xC6,0xE6,0xF6,0xDE,0xCE,0xC6,0xC6,0x00},
    {0x7C,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00},
    {0xFC,0xC6,0xC6,0xFC,0xC0,0xC0,0xC0,0x00},
    {0x7C,0xC6,0xC6,0xC6,0xD6,0xDE,0x7C,0x06},
    {0xFC,0xC6,0xC6,0xFC,0xD8,0xCC,0xC6,0x00},
    {0x7C,0xC6,0xC0,0x7C,0x06,0xC6,0x7C,0x00},
    {0x7E,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    {0xC6,0xC6,0xC6,0xC6,0xC6,0xC6,0x7C,0x00},
    {0xC6,0xC6,0xC6,0xC6,0x6C,0x38,0x10,0x00},
    {0xC6,0xC6,0xC6,0xD6,0xFE,0xEE,0xC6,0x00},
    {0xC6,0xC6,0x6C,0x38,0x6C,0xC6,0xC6,0x00},
    {0x66,0x66,0x66,0x3C,0x18,0x18,0x18,0x00},
    {0xFE,0x06,0x0C,0x18,0x30,0x60,0xFE,0x00},
    {0x3C,0x30,0x30,0x30,0x30,0x30,0x3C,0x00},
    {0xC0,0x60,0x30,0x18,0x0C,0x06,0x02,0x00},
    {0x3C,0x0C,0x0C,0x0C,0x0C,0x0C,0x3C,0x00},
    {0x10,0x38,0x6C,0xC6,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFE},
    {0x18,0x18,0x0C,0x00,0x00,0x00,0x00,0x00},
    {0x00,0x00,0x7C,0x06,0x7E,0xC6,0x7E,0x00},
    {0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xFC,0x00},
    {0x00,0x00,0x7C,0xC6,0xC0,0xC6,0x7C,0x00},
    {0x06,0x06,0x7E,0xC6,0xC6,0xC6,0x7E,0x00},
    {0x00,0x00,0x7C,0xC6,0xFE,0xC0,0x7C,0x00},
    {0x1C,0x30,0x30,0x7C,0x30,0x30,0x30,0x00},
    {0x00,0x00,0x7E,0xC6,0xC6,0x7E,0x06,0x7C},
    {0xC0,0xC0,0xFC,0xC6,0xC6,0xC6,0xC6,0x00},
    {0x18,0x00,0x38,0x18,0x18,0x18,0x3C,0x00},
    {0x18,0x00,0x38,0x18,0x18,0x18,0x18,0x70},
    {0xC0,0xC0,0xC6,0xCC,0xF8,0xCC,0xC6,0x00},
    {0x38,0x18,0x18,0x18,0x18,0x18,0x3C,0x00},
    {0x00,0x00,0xEC,0xFE,0xD6,0xC6,0xC6,0x00},
    {0x00,0x00,0xFC,0xC6,0xC6,0xC6,0xC6,0x00},
    {0x00,0x00,0x7C,0xC6,0xC6,0xC6,0x7C,0x00},
    {0x00,0x00,0xFC,0xC6,0xC6,0xFC,0xC0,0xC0},
    {0x00,0x00,0x7E,0xC6,0xC6,0x7E,0x06,0x06},
    {0x00,0x00,0xDC,0xE6,0xC0,0xC0,0xC0,0x00},
    {0x00,0x00,0x7E,0xC0,0x7C,0x06,0xFC,0x00},
    {0x30,0x30,0x7C,0x30,0x30,0x30,0x1C,0x00},
    {0x00,0x00,0xC6,0xC6,0xC6,0xC6,0x7E,0x00},
    {0x00,0x00,0xC6,0xC6,0xC6,0x6C,0x38,0x00},
    {0x00,0x00,0xC6,0xC6,0xD6,0xFE,0x6C,0x00},
    {0x00,0x00,0xC6,0x6C,0x38,0x6C,0xC6,0x00},
    {0x00,0x00,0xC6,0xC6,0xC6,0x7E,0x06,0x7C},
    {0x00,0x00,0xFE,0x0C,0x38,0x60,0xFE,0x00},
    {0x0E,0x18,0x18,0x70,0x18,0x18,0x0E,0x00},
    {0x18,0x18,0x18,0x18,0x18,0x18,0x18,0x00},
    {0x70,0x18,0x18,0x0E,0x18,0x18,0x70,0x00},
    {0x72,0x9C,0x00,0x00,0x00,0x00,0x00,0x00},
};

/* ---- Helpers --------------------------------------------------------- */

static inline int32_t min_i32(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t max_i32(int32_t a, int32_t b) { return a > b ? a : b; }
static inline uint32_t min_u32(uint32_t a, uint32_t b) { return a < b ? a : b; }
static inline uint32_t max_u32(uint32_t a, uint32_t b) { return a > b ? a : b; }
static inline int32_t abs_i32(int32_t x) { return x < 0 ? -x : x; }

static inline bool is_clipped(const framebuffer_t *fb, uint32_t x, uint32_t y)
{
    const fb_clip_t *clip = &fb->clip_stack[fb->clip_depth];
    return x < clip->x || y < clip->y ||
           x >= clip->x + clip->w || y >= clip->y + clip->h;
}

static inline size_t strlen_local(const char *s)
{
    size_t len = 0;
    while (*s++) len++;
    return len;
}

/* ---- Color utilities ------------------------------------------------- */

uint32_t fb_color_lerp(uint32_t c1, uint32_t c2, uint8_t t)
{
    uint32_t inv = 255 - t;
    uint32_t r = (FB_RED(c1) * inv + FB_RED(c2) * t) / 255;
    uint32_t g = (FB_GREEN(c1) * inv + FB_GREEN(c2) * t) / 255;
    uint32_t b = (FB_BLUE(c1) * inv + FB_BLUE(c2) * t) / 255;
    return FB_RGB(r, g, b);
}

uint32_t fb_blend_alpha(uint32_t src, uint32_t dst)
{
    uint32_t sa = FB_ALPHA(src);
    if (sa == 0) return dst;
    if (sa == 255) return src;
    uint32_t inv = 255 - sa;
    uint32_t r = (FB_RED(src) * sa + FB_RED(dst) * inv) / 255;
    uint32_t g = (FB_GREEN(src) * sa + FB_GREEN(dst) * inv) / 255;
    uint32_t b = (FB_BLUE(src) * sa + FB_BLUE(dst) * inv) / 255;
    return FB_RGB(r, g, b);
}

/* ---- Init (weak — overridden by display_spi.c) ---------------------- */

__attribute__((weak)) bool fb_init(framebuffer_t *fb)
{
    (void)fb;
    return false;
}

/* Weak fb_flip_display — overridden by display_spi.c */
__attribute__((weak)) void fb_flip_display(framebuffer_t *fb)
{
    (void)fb;
}

/* ---- Present --------------------------------------------------------- */

void fb_present(framebuffer_t *fb)
{
    if (!fb->initialized) return;
    HAL_DSB();
    fb_flip_display(fb);
    fb_clear_dirty(fb);
    fb->frame_count++;
}

/* ---- Clipping -------------------------------------------------------- */

bool fb_push_clip(framebuffer_t *fb, fb_rect_t rect)
{
    if (fb->clip_depth >= FB_MAX_CLIP_DEPTH - 1) return false;
    fb_clip_t *cur = &fb->clip_stack[fb->clip_depth];
    int32_t x1 = max_i32(rect.x, (int32_t)cur->x);
    int32_t y1 = max_i32(rect.y, (int32_t)cur->y);
    int32_t x2 = min_i32(rect.x + (int32_t)rect.w, (int32_t)(cur->x + cur->w));
    int32_t y2 = min_i32(rect.y + (int32_t)rect.h, (int32_t)(cur->y + cur->h));
    fb->clip_depth++;
    fb_clip_t *nc = &fb->clip_stack[fb->clip_depth];
    if (x2 <= x1 || y2 <= y1) { nc->x = nc->y = nc->w = nc->h = 0; }
    else { nc->x = x1; nc->y = y1; nc->w = x2 - x1; nc->h = y2 - y1; }
    return true;
}

void fb_pop_clip(framebuffer_t *fb)  { if (fb->clip_depth > 0) fb->clip_depth--; }
void fb_reset_clip(framebuffer_t *fb)
{
    fb->clip_depth = 0;
    fb->clip_stack[0] = (fb_clip_t){0, 0, fb->width, fb->height};
}

/* ---- Dirty tracking -------------------------------------------------- */

void fb_mark_dirty(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    (void)x; (void)y; (void)w; (void)h;
    fb->full_dirty = true;  /* simplified: always full-dirty on RP2350 */
}
void fb_mark_all_dirty(framebuffer_t *fb) { fb->full_dirty = true; }
void fb_clear_dirty(framebuffer_t *fb) { fb->dirty_count = 0; fb->full_dirty = false; }
bool fb_is_dirty(const framebuffer_t *fb) { return fb->full_dirty || fb->dirty_count > 0; }

/* ---- Basic drawing --------------------------------------------------- */

void fb_clear(framebuffer_t *fb, uint32_t color)
{
    uint16_t c565 = fb_argb_to_565(color);
    uint16_t *buf = (uint16_t *)(void *)fb->addr;
    uint32_t total = (fb->pitch / 2) * fb->height;
    for (uint32_t i = 0; i < total; i++) buf[i] = c565;
    fb_mark_all_dirty(fb);
}

void fb_put_pixel(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb->width || y >= fb->height || is_clipped(fb, x, y)) return;
    uint8_t *row = FB_ROW(fb, y);
    FB_WRITE_PX(fb, row, x, color);
}

void fb_put_pixel_blend(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t color)
{
    if (x >= fb->width || y >= fb->height || is_clipped(fb, x, y)) return;
    uint8_t *row = FB_ROW(fb, y);
    uint32_t dst = FB_READ_PX(fb, row, x);
    FB_WRITE_PX(fb, row, x, fb_blend_alpha(color, dst));
}

void fb_put_pixel_unchecked(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t color)
{
    uint8_t *row = FB_ROW(fb, y);
    FB_WRITE_PX(fb, row, x, color);
}

uint32_t fb_get_pixel(const framebuffer_t *fb, uint32_t x, uint32_t y)
{
    if (x >= fb->width || y >= fb->height) return FB_COLOR_TRANSPARENT;
    const uint8_t *row = (const uint8_t *)fb->addr + y * fb->pitch;
    return FB_READ_PX(fb, row, x);
}

void fb_fill_rect(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    const fb_clip_t *clip = &fb->clip_stack[fb->clip_depth];
    uint32_t x1 = max_u32(x, clip->x);
    uint32_t y1 = max_u32(y, clip->y);
    uint32_t x2 = min_u32(x + w, clip->x + clip->w);
    uint32_t y2 = min_u32(y + h, clip->y + clip->h);
    if (x2 <= x1 || y2 <= y1) return;

    uint16_t c565 = fb_argb_to_565(color);
    for (uint32_t py = y1; py < y2; py++) {
        uint16_t *row = (uint16_t *)FB_ROW(fb, py);
        for (uint32_t px = x1; px < x2; px++) row[px] = c565;
    }
    fb_mark_dirty(fb, x1, y1, x2-x1, y2-y1);
}

void fb_fill_rect_blend(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    const fb_clip_t *clip = &fb->clip_stack[fb->clip_depth];
    uint32_t x1 = max_u32(x, clip->x), y1 = max_u32(y, clip->y);
    uint32_t x2 = min_u32(x+w, clip->x+clip->w), y2 = min_u32(y+h, clip->y+clip->h);
    if (x2<=x1 || y2<=y1) return;
    for (uint32_t py = y1; py < y2; py++) {
        uint8_t *row = FB_ROW(fb, py);
        for (uint32_t px = x1; px < x2; px++) {
            uint32_t dst = FB_READ_PX(fb, row, px);
            FB_WRITE_PX(fb, row, px, fb_blend_alpha(color, dst));
        }
    }
}

void fb_draw_rect(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color)
{
    if (w == 0 || h == 0) return;
    fb_draw_hline(fb, x, y, w, color);
    fb_draw_hline(fb, x, y + h - 1, w, color);
    fb_draw_vline(fb, x, y, h, color);
    fb_draw_vline(fb, x + w - 1, y, h, color);
}

void fb_draw_hline(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t len, uint32_t color)
{
    const fb_clip_t *clip = &fb->clip_stack[fb->clip_depth];
    if (y < clip->y || y >= clip->y + clip->h) return;
    uint32_t x1 = max_u32(x, clip->x);
    uint32_t x2 = min_u32(x + len, clip->x + clip->w);
    if (x2 <= x1) return;
    uint16_t c565 = fb_argb_to_565(color);
    uint16_t *row = (uint16_t *)FB_ROW(fb, y);
    for (uint32_t px = x1; px < x2; px++) row[px] = c565;
}

static void fb_draw_hline_safe(framebuffer_t *fb, int32_t x, int32_t y, int32_t len, uint32_t color)
{
    if (y < 0 || y >= (int32_t)fb->height || x >= (int32_t)fb->width || x+len <= 0) return;
    if (x < 0) { len += x; x = 0; }
    if (x + len > (int32_t)fb->width) len = (int32_t)fb->width - x;
    if (len > 0) fb_draw_hline(fb, (uint32_t)x, (uint32_t)y, (uint32_t)len, color);
}

void fb_draw_vline(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t len, uint32_t color)
{
    const fb_clip_t *clip = &fb->clip_stack[fb->clip_depth];
    if (x < clip->x || x >= clip->x + clip->w) return;
    uint32_t y1 = max_u32(y, clip->y);
    uint32_t y2 = min_u32(y + len, clip->y + clip->h);
    if (y2 <= y1) return;
    for (uint32_t py = y1; py < y2; py++) {
        uint8_t *row = FB_ROW(fb, py);
        FB_WRITE_PX(fb, row, x, color);
    }
}

void fb_draw_line(framebuffer_t *fb, int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)
{
    int32_t dx = abs_i32(x1-x0), dy = -abs_i32(y1-y0);
    int32_t sx = x0<x1?1:-1, sy = y0<y1?1:-1, err = dx+dy;
    while (1) {
        if (x0>=0 && y0>=0) fb_put_pixel(fb, (uint32_t)x0, (uint32_t)y0, color);
        if (x0==x1 && y0==y1) break;
        int32_t e2 = 2*err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ---- Circle ---------------------------------------------------------- */

void fb_fill_circle(framebuffer_t *fb, int32_t cx, int32_t cy, uint32_t radius, uint32_t color)
{
    if (radius == 0) { if (cx>=0&&cy>=0) fb_put_pixel(fb,cx,cy,color); return; }
    int32_t r=(int32_t)radius, x=0, y=r, d=1-r;
    while (x <= y) {
        fb_draw_hline_safe(fb, cx-x, cy+y, 2*x+1, color);
        fb_draw_hline_safe(fb, cx-x, cy-y, 2*x+1, color);
        fb_draw_hline_safe(fb, cx-y, cy+x, 2*y+1, color);
        fb_draw_hline_safe(fb, cx-y, cy-x, 2*y+1, color);
        if (d<0) { d+=2*x+3; } else { d+=2*(x-y)+5; y--; }
        x++;
    }
}

void fb_draw_circle(framebuffer_t *fb, int32_t cx, int32_t cy, uint32_t radius, uint32_t color)
{
    if (radius==0) { if(cx>=0&&cy>=0) fb_put_pixel(fb,cx,cy,color); return; }
    int32_t x=0, y=(int32_t)radius, d=1-(int32_t)radius;
    while (x<=y) {
        fb_put_pixel(fb,cx+x,cy+y,color); fb_put_pixel(fb,cx-x,cy+y,color);
        fb_put_pixel(fb,cx+x,cy-y,color); fb_put_pixel(fb,cx-x,cy-y,color);
        fb_put_pixel(fb,cx+y,cy+x,color); fb_put_pixel(fb,cx-y,cy+x,color);
        fb_put_pixel(fb,cx+y,cy-x,color); fb_put_pixel(fb,cx-y,cy-x,color);
        if (d<0) d+=2*x+3; else { d+=2*(x-y)+5; y--; }
        x++;
    }
}

/* ---- Triangle -------------------------------------------------------- */

void fb_fill_triangle(framebuffer_t *fb, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                      int32_t x2, int32_t y2, uint32_t color)
{
    if(y0>y1){int32_t t;t=x0;x0=x1;x1=t;t=y0;y0=y1;y1=t;}
    if(y1>y2){int32_t t;t=x1;x1=x2;x2=t;t=y1;y1=y2;y2=t;}
    if(y0>y1){int32_t t;t=x0;x0=x1;x1=t;t=y0;y0=y1;y1=t;}
    if(y0==y2) return;

    for (int32_t y = y0; y <= y2; y++) {
        int32_t ax = x0 + (int32_t)((int64_t)(x2-x0)*(y-y0)/(y2-y0));
        int32_t bx;
        if (y < y1) bx = x0 + (int32_t)((y1!=y0) ? (int64_t)(x1-x0)*(y-y0)/(y1-y0) : 0);
        else        bx = x1 + (int32_t)((y2!=y1) ? (int64_t)(x2-x1)*(y-y1)/(y2-y1) : 0);
        if (ax>bx) { int32_t t=ax; ax=bx; bx=t; }
        fb_draw_hline_safe(fb, ax, y, bx-ax+1, color);
    }
}

/* ---- Rounded rect ---------------------------------------------------- */

void fb_fill_rounded_rect(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint32_t radius, uint32_t color)
{
    if (radius==0) { fb_fill_rect(fb,x,y,w,h,color); return; }
    if (radius>w/2) radius=w/2;
    if (radius>h/2) radius=h/2;
    fb_fill_rect(fb, x+radius, y, w-2*radius, h, color);
    fb_fill_rect(fb, x, y+radius, radius, h-2*radius, color);
    fb_fill_rect(fb, x+w-radius, y+radius, radius, h-2*radius, color);
    int32_t r=(int32_t)radius;
    for (int32_t dy=0;dy<=r;dy++) for (int32_t dx=0;dx<=r;dx++) {
        if (dx*dx+dy*dy<=r*r) {
            fb_put_pixel(fb,x+radius-dx,y+radius-dy,color);
            fb_put_pixel(fb,x+w-1-radius+dx,y+radius-dy,color);
            fb_put_pixel(fb,x+radius-dx,y+h-1-radius+dy,color);
            fb_put_pixel(fb,x+w-1-radius+dx,y+h-1-radius+dy,color);
        }
    }
}

void fb_draw_rounded_rect(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                          uint32_t radius, uint32_t color)
{
    if (radius==0) { fb_draw_rect(fb,x,y,w,h,color); return; }
    if (radius>w/2) radius=w/2; if (radius>h/2) radius=h/2;
    fb_draw_hline(fb,x+radius,y,w-2*radius,color);
    fb_draw_hline(fb,x+radius,y+h-1,w-2*radius,color);
    fb_draw_vline(fb,x,y+radius,h-2*radius,color);
    fb_draw_vline(fb,x+w-1,y+radius,h-2*radius,color);
    int32_t r=(int32_t)radius, px=0, py=r, d=1-r;
    while (px<=py) {
        fb_put_pixel(fb,x+radius-px,y+radius-py,color);
        fb_put_pixel(fb,x+radius-py,y+radius-px,color);
        fb_put_pixel(fb,x+w-1-radius+px,y+radius-py,color);
        fb_put_pixel(fb,x+w-1-radius+py,y+radius-px,color);
        fb_put_pixel(fb,x+radius-px,y+h-1-radius+py,color);
        fb_put_pixel(fb,x+radius-py,y+h-1-radius+px,color);
        fb_put_pixel(fb,x+w-1-radius+px,y+h-1-radius+py,color);
        fb_put_pixel(fb,x+w-1-radius+py,y+h-1-radius+px,color);
        if (d<0) d+=2*px+3; else { d+=2*(px-py)+5; py--; }
        px++;
    }
}

/* ---- Gradients ------------------------------------------------------- */

void fb_fill_rect_gradient_v(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                             uint32_t top, uint32_t bot)
{
    for (uint32_t r=0;r<h;r++) {
        uint8_t t = (h>1) ? (r*255)/(h-1) : 0;
        fb_draw_hline(fb, x, y+r, w, fb_color_lerp(top, bot, t));
    }
}

void fb_fill_rect_gradient_h(framebuffer_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                             uint32_t left, uint32_t right)
{
    for (uint32_t c=0;c<w;c++) {
        uint8_t t = (w>1) ? (c*255)/(w-1) : 0;
        fb_draw_vline(fb, x+c, y, h, fb_color_lerp(left, right, t));
    }
}

/* ---- Copy / Scroll --------------------------------------------------- */

void fb_copy_rect(framebuffer_t *fb, uint32_t sx, uint32_t sy, uint32_t dx, uint32_t dy, uint32_t w, uint32_t h)
{
    if (sy < dy || (sy==dy && sx < dx)) {
        for (uint32_t r=h;r>0;r--) {
            uint16_t *sr = (uint16_t*)FB_ROW(fb,sy+r-1);
            uint16_t *dr = (uint16_t*)FB_ROW(fb,dy+r-1);
            for (uint32_t c=w;c>0;c--) dr[dx+c-1]=sr[sx+c-1];
        }
    } else {
        for (uint32_t r=0;r<h;r++) {
            uint16_t *sr = (uint16_t*)FB_ROW(fb,sy+r);
            uint16_t *dr = (uint16_t*)FB_ROW(fb,dy+r);
            for (uint32_t c=0;c<w;c++) dr[dx+c]=sr[sx+c];
        }
    }
}

void fb_scroll_v(framebuffer_t *fb, int32_t px, uint32_t fill)
{
    if (px==0) return;
    uint32_t a = abs_i32(px);
    if (a>=fb->height) { fb_clear(fb,fill); return; }
    if (px>0) { fb_copy_rect(fb,0,0,0,a,fb->width,fb->height-a); fb_fill_rect(fb,0,0,fb->width,a,fill); }
    else      { fb_copy_rect(fb,0,a,0,0,fb->width,fb->height-a); fb_fill_rect(fb,0,fb->height-a,fb->width,a,fill); }
}

/* ---- Text ------------------------------------------------------------ */

void fb_draw_char(framebuffer_t *fb, uint32_t x, uint32_t y, char c, uint32_t fg, uint32_t bg)
{
    uint8_t ch = (uint8_t)c;
    if (ch<32||ch>126) ch='?';
    const uint8_t *g = font_8x8[ch-32];
    for (uint32_t r=0;r<8;r++) {
        uint8_t bits=g[r];
        for (uint32_t col=0;col<8;col++)
            fb_put_pixel(fb, x+col, y+r, (bits & (0x80>>col)) ? fg : bg);
    }
}

void fb_draw_char_transparent(framebuffer_t *fb, uint32_t x, uint32_t y, char c, uint32_t fg)
{
    uint8_t ch = (uint8_t)c;
    if (ch<32||ch>126) ch='?';
    const uint8_t *g = font_8x8[ch-32];
    for (uint32_t r=0;r<8;r++) {
        uint8_t bits=g[r];
        for (uint32_t col=0;col<8;col++)
            if (bits & (0x80>>col)) fb_put_pixel(fb, x+col, y+r, fg);
    }
}

void fb_draw_char_scaled(framebuffer_t *fb, uint32_t x, uint32_t y, char c,
                         uint32_t fg, uint32_t bg, uint32_t scale)
{
    uint8_t ch = (uint8_t)c;
    if (ch<32||ch>126) ch='?';
    const uint8_t *g = font_8x8[ch-32];
    for (uint32_t r=0;r<8;r++) {
        uint8_t bits=g[r];
        for (uint32_t col=0;col<8;col++) {
            uint32_t color = (bits & (0x80>>col)) ? fg : bg;
            for (uint32_t sy=0;sy<scale;sy++)
                for (uint32_t sx=0;sx<scale;sx++)
                    fb_put_pixel(fb, x+col*scale+sx, y+r*scale+sy, color);
        }
    }
}

void fb_draw_string(framebuffer_t *fb, uint32_t x, uint32_t y, const char *str, uint32_t fg, uint32_t bg)
{
    while (*str) { if (x+8>fb->width) break; fb_draw_char(fb,x,y,*str++,fg,bg); x+=8; }
}

void fb_draw_string_transparent(framebuffer_t *fb, uint32_t x, uint32_t y, const char *str, uint32_t fg)
{
    while (*str) { if (x+8>fb->width) break; fb_draw_char_transparent(fb,x,y,*str++,fg); x+=8; }
}

void fb_draw_string_scaled(framebuffer_t *fb, uint32_t x, uint32_t y, const char *str,
                           uint32_t fg, uint32_t bg, uint32_t scale)
{
    uint32_t cw = 8*scale;
    while (*str) { if (x+cw>fb->width) break; fb_draw_char_scaled(fb,x,y,*str++,fg,bg,scale); x+=cw; }
}

void fb_draw_string_scaled_transparent(framebuffer_t *fb, uint32_t x, uint32_t y,
                                       const char *str, uint32_t fg, uint32_t scale)
{
    if (scale<=1) { fb_draw_string_transparent(fb,x,y,str,fg); return; }
    uint32_t cw = 8*scale;
    while (*str) {
        if (x+cw>fb->width) break;
        uint8_t ch = (uint8_t)*str; if (ch<32||ch>126) ch='?';
        const uint8_t *g = font_8x8[ch-32];
        for (uint32_t r=0;r<8;r++) { uint8_t bits=g[r];
            for (uint32_t col=0;col<8;col++) if (bits & (0x80>>col))
                for (uint32_t sy=0;sy<scale;sy++) for (uint32_t sx=0;sx<scale;sx++)
                    fb_put_pixel(fb, x+col*scale+sx, y+r*scale+sy, fg);
        }
        x += cw; str++;
    }
}

void fb_draw_string_centered(framebuffer_t *fb, uint32_t y, const char *str, uint32_t fg, uint32_t bg)
{
    uint32_t w = fb_text_width(str);
    uint32_t x = (w < fb->width) ? (fb->width - w) / 2 : 0;
    fb_draw_string(fb, x, y, str, fg, bg);
}

uint32_t fb_text_width(const char *str) { return (uint32_t)strlen_local(str) * 8; }
uint32_t fb_text_width_large(const char *str) { return (uint32_t)strlen_local(str) * 8; }
uint32_t fb_measure_string(const char *str, bool large) { (void)large; return fb_text_width(str); }
