/*
 * ili9488.c — ILI9488 SPI driver for RP2350 (Pico 2)
 *
 * CRITICAL: In SPI mode the ILI9488 only supports 18-bit colour
 * (RGB666).  Each pixel is 3 bytes on the wire.  COLMOD = 0x66.
 */

#include "ili9488.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

#include <stdlib.h>     /* abs() */
#include <string.h>     /* memset */

/* ---- ILI9488 command bytes ------------------------------------------- */
#define CMD_NOP         0x00
#define CMD_SWRESET     0x01
#define CMD_SLPOUT      0x11
#define CMD_INVOFF      0x20
#define CMD_INVON       0x21
#define CMD_DISPON      0x29
#define CMD_CASET       0x2A
#define CMD_RASET       0x2B
#define CMD_RAMWR       0x2C
#define CMD_MADCTL      0x36
#define CMD_COLMOD      0x3A

#define CMD_IFMODE      0xB0
#define CMD_FRMCTR1     0xB1
#define CMD_INVTR       0xB4
#define CMD_DFUNCTR     0xB6
#define CMD_ETMOD       0xB7
#define CMD_PWCTR1      0xC0
#define CMD_PWCTR2      0xC1
#define CMD_PWCTR3      0xC2
#define CMD_VMCTR1      0xC5
#define CMD_PGAMCTRL    0xE0
#define CMD_NGAMCTRL    0xE1
#define CMD_ADJCTL3     0xF7

/* MADCTL flags */
#define MADCTL_MY       0x80
#define MADCTL_MX       0x40
#define MADCTL_MV       0x20
#define MADCTL_ML       0x10
#define MADCTL_BGR      0x08
#define MADCTL_MH       0x04

/* ---- State ----------------------------------------------------------- */
static uint8_t  _rotation = 0;
static uint16_t _width    = DISP_WIDTH;
static uint16_t _height   = DISP_HEIGHT;

/* ---- RGB565 → RGB666 ------------------------------------------------- */

static inline void rgb565_to_666(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b)
{
    *r = (uint8_t)((c >> 8) & 0xF8) | ((c >> 13) & 0x07);
    *g = (uint8_t)((c >> 3) & 0xFC) | ((c >>  9) & 0x03);
    *b = (uint8_t)((c << 3) & 0xF8) | ((c >>  2) & 0x07);
}

/* ---- Low-level helpers ----------------------------------------------- */

static inline void cs_select(void)   { gpio_put(PIN_CS, 0); }
static inline void cs_deselect(void) { gpio_put(PIN_CS, 1); }

static void write_cmd(uint8_t cmd)
{
    gpio_put(PIN_DC, 0);
    cs_select();
    spi_write_blocking(DISP_SPI_INST, &cmd, 1);
    cs_deselect();
}

static void write_data(const uint8_t *buf, size_t len)
{
    gpio_put(PIN_DC, 1);
    cs_select();
    spi_write_blocking(DISP_SPI_INST, buf, len);
    cs_deselect();
}

static void write_cmd_data(uint8_t cmd, const uint8_t *params, size_t len)
{
    write_cmd(cmd);
    if (len > 0)
        write_data(params, len);
}

/* ---- Address window -------------------------------------------------- */

static void set_addr_window(uint16_t x0, uint16_t y0,
                            uint16_t x1, uint16_t y1)
{
    uint8_t buf[4];

    write_cmd(CMD_CASET);
    buf[0] = x0 >> 8; buf[1] = x0 & 0xFF;
    buf[2] = x1 >> 8; buf[3] = x1 & 0xFF;
    write_data(buf, 4);

    write_cmd(CMD_RASET);
    buf[0] = y0 >> 8; buf[1] = y0 & 0xFF;
    buf[2] = y1 >> 8; buf[3] = y1 & 0xFF;
    write_data(buf, 4);

    write_cmd(CMD_RAMWR);
}

/* ---- Hardware reset -------------------------------------------------- */

static void hard_reset(void)
{
    gpio_put(PIN_RST, 1);
    sleep_ms(10);
    gpio_put(PIN_RST, 0);
    sleep_ms(50);
    gpio_put(PIN_RST, 1);
    sleep_ms(150);
}

/* ---- Initialisation -------------------------------------------------- */

void ili9488_init(void)
{
    spi_init(DISP_SPI_INST, DISP_SPI_BAUD);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    gpio_init(PIN_CS);   gpio_set_dir(PIN_CS,  GPIO_OUT); gpio_put(PIN_CS,  1);
    gpio_init(PIN_DC);   gpio_set_dir(PIN_DC,  GPIO_OUT);
    gpio_init(PIN_RST);  gpio_set_dir(PIN_RST, GPIO_OUT);

    hard_reset();

    write_cmd(CMD_SWRESET);
    sleep_ms(150);
    write_cmd(CMD_SLPOUT);
    sleep_ms(150);

    /* Positive gamma */
    write_cmd_data(CMD_PGAMCTRL, (const uint8_t[]){
        0x00,0x03,0x09,0x08,0x16,0x0A,0x3F,0x78,
        0x4C,0x09,0x0A,0x08,0x16,0x1A,0x0F }, 15);

    /* Negative gamma */
    write_cmd_data(CMD_NGAMCTRL, (const uint8_t[]){
        0x00,0x16,0x19,0x03,0x0F,0x05,0x32,0x45,
        0x46,0x04,0x0E,0x0D,0x35,0x37,0x0F }, 15);

    write_cmd_data(CMD_PWCTR1,  (const uint8_t[]){0x17, 0x15}, 2);
    write_cmd_data(CMD_PWCTR2,  (const uint8_t[]){0x41}, 1);
    write_cmd_data(CMD_VMCTR1,  (const uint8_t[]){0x00, 0x12, 0x80}, 3);
    write_cmd_data(CMD_MADCTL,  (const uint8_t[]){MADCTL_MX | MADCTL_BGR}, 1);
    write_cmd_data(CMD_COLMOD,  (const uint8_t[]){0x66}, 1);   /* 18-bit! */
    write_cmd_data(CMD_IFMODE,  (const uint8_t[]){0x00}, 1);
    write_cmd_data(CMD_FRMCTR1, (const uint8_t[]){0xA0}, 1);
    write_cmd_data(CMD_INVTR,   (const uint8_t[]){0x02}, 1);
    write_cmd_data(CMD_DFUNCTR, (const uint8_t[]){0x02, 0x02, 0x3B}, 3);
    write_cmd_data(CMD_ETMOD,   (const uint8_t[]){0xC6}, 1);
    write_cmd_data(CMD_ADJCTL3, (const uint8_t[]){0xA9, 0x51, 0x2C, 0x82}, 4);

    write_cmd(CMD_DISPON);
    sleep_ms(100);

    _rotation = 0;
    _width    = DISP_WIDTH;
    _height   = DISP_HEIGHT;
}

/* ---- Rotation -------------------------------------------------------- */

void ili9488_set_rotation(uint8_t rot)
{
    _rotation = rot & 3;
    uint8_t m;
    switch (_rotation) {
        case 0:  m = MADCTL_MX | MADCTL_BGR;
                 _width = DISP_WIDTH; _height = DISP_HEIGHT; break;
        case 1:  m = MADCTL_MV | MADCTL_BGR;
                 _width = DISP_HEIGHT; _height = DISP_WIDTH; break;
        case 2:  m = MADCTL_MY | MADCTL_BGR;
                 _width = DISP_WIDTH; _height = DISP_HEIGHT; break;
        default: m = MADCTL_MX | MADCTL_MY | MADCTL_MV | MADCTL_BGR;
                 _width = DISP_HEIGHT; _height = DISP_WIDTH; break;
    }
    write_cmd_data(CMD_MADCTL, &m, 1);
}

uint16_t ili9488_width(void)  { return _width; }
uint16_t ili9488_height(void) { return _height; }

/* ---- Basic drawing --------------------------------------------------- */

void ili9488_fill(uint16_t color)
{
    ili9488_fill_rect(0, 0, _width - 1, _height - 1, color);
}

void ili9488_fill_rect(uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1,
                       uint16_t color)
{
    if (x0 > x1 || y0 > y1) return;
    if (x1 >= _width)  x1 = _width  - 1;
    if (y1 >= _height) y1 = _height - 1;

    set_addr_window(x0, y0, x1, y1);

    uint16_t w = (x1 - x0) + 1;
    uint16_t h = (y1 - y0) + 1;

    uint8_t r, g, b;
    rgb565_to_666(color, &r, &g, &b);

    /* Build one scanline (max 480 px × 3 = 1440 bytes) */
    uint8_t line[480 * 3];
    for (uint16_t i = 0; i < w; i++) {
        line[i * 3]     = r;
        line[i * 3 + 1] = g;
        line[i * 3 + 2] = b;
    }

    gpio_put(PIN_DC, 1);
    cs_select();
    for (uint16_t row = 0; row < h; row++)
        spi_write_blocking(DISP_SPI_INST, line, w * 3);
    cs_deselect();
}

void ili9488_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= _width || y >= _height) return;
    set_addr_window(x, y, x, y);
    uint8_t r, g, b;
    rgb565_to_666(color, &r, &g, &b);
    uint8_t buf[3] = {r, g, b};
    write_data(buf, 3);
}

/* ---- Fast lines ------------------------------------------------------ */

void ili9488_hline(uint16_t x, uint16_t y, uint16_t w, uint16_t color)
{
    if (y >= _height || x >= _width) return;
    if (x + w > _width) w = _width - x;
    ili9488_fill_rect(x, y, x + w - 1, y, color);
}

void ili9488_vline(uint16_t x, uint16_t y, uint16_t h, uint16_t color)
{
    if (x >= _width || y >= _height) return;
    if (y + h > _height) h = _height - y;
    ili9488_fill_rect(x, y, x, y + h - 1, color);
}

/* ---- Outlined rectangle ---------------------------------------------- */

void ili9488_rect(uint16_t x0, uint16_t y0,
                  uint16_t x1, uint16_t y1,
                  uint16_t color)
{
    ili9488_hline(x0, y0, x1 - x0 + 1, color);
    ili9488_hline(x0, y1, x1 - x0 + 1, color);
    ili9488_vline(x0, y0, y1 - y0 + 1, color);
    ili9488_vline(x1, y0, y1 - y0 + 1, color);
}

/* ---- Bresenham line -------------------------------------------------- */

void ili9488_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                  uint16_t color)
{
    /* Fast-path for axis-aligned */
    if (y0 == y1) {
        if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
        ili9488_hline(x0, y0, x1 - x0 + 1, color);
        return;
    }
    if (x0 == x1) {
        if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
        ili9488_vline(x0, y0, y1 - y0 + 1, color);
        return;
    }

    int16_t dx =  abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy;

    for (;;) {
        ili9488_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ---- Circle (midpoint algorithm) ------------------------------------- */

void ili9488_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
    int16_t x = r, y = 0, err = 1 - r;
    while (x >= y) {
        ili9488_pixel(cx + x, cy + y, color);
        ili9488_pixel(cx - x, cy + y, color);
        ili9488_pixel(cx + x, cy - y, color);
        ili9488_pixel(cx - x, cy - y, color);
        ili9488_pixel(cx + y, cy + x, color);
        ili9488_pixel(cx - y, cy + x, color);
        ili9488_pixel(cx + y, cy - x, color);
        ili9488_pixel(cx - y, cy - x, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void ili9488_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t color)
{
    ili9488_vline(cx, cy - r, 2 * r + 1, color);
    int16_t x = r, y = 0, err = 1 - r;
    while (x >= y) {
        ili9488_hline(cx - x, cy + y, 2 * x + 1, color);
        ili9488_hline(cx - x, cy - y, 2 * x + 1, color);
        ili9488_hline(cx - y, cy + x, 2 * y + 1, color);
        ili9488_hline(cx - y, cy - x, 2 * y + 1, color);
        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/* ---- Filled rounded rectangle ---------------------------------------- */

void ili9488_fill_rounded_rect(uint16_t x0, uint16_t y0,
                               uint16_t x1, uint16_t y1,
                               uint16_t r, uint16_t color)
{
    /* Central body */
    ili9488_fill_rect(x0, y0 + r, x1, y1 - r, color);
    /* Top and bottom bars */
    ili9488_fill_rect(x0 + r, y0, x1 - r, y0 + r - 1, color);
    ili9488_fill_rect(x0 + r, y1 - r + 1, x1 - r, y1, color);

    /* Four corner quarter-circles */
    int16_t cx, cy;
    int16_t x = r, y = 0, err = 1 - (int16_t)r;
    while (x >= y) {
        /* Top-left */
        cx = x0 + r; cy = y0 + r;
        ili9488_hline(cx - x, cy - y, x, color);
        ili9488_hline(cx - y, cy - x, y, color);
        /* Top-right */
        cx = x1 - r; cy = y0 + r;
        ili9488_hline(cx, cy - y, x + 1, color);
        ili9488_hline(cx, cy - x, y + 1, color);
        /* Bottom-left */
        cx = x0 + r; cy = y1 - r;
        ili9488_hline(cx - x, cy + y, x, color);
        ili9488_hline(cx - y, cy + x, y, color);
        /* Bottom-right */
        cx = x1 - r; cy = y1 - r;
        ili9488_hline(cx, cy + y, x + 1, color);
        ili9488_hline(cx, cy + x, y + 1, color);

        y++;
        if (err < 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

/* ---- Filled triangle (scanline) -------------------------------------- */

void ili9488_fill_triangle(int16_t x0, int16_t y0,
                           int16_t x1, int16_t y1,
                           int16_t x2, int16_t y2,
                           uint16_t color)
{
    /* Sort by Y ascending */
    if (y0 > y1) { int16_t t; t=x0;x0=x1;x1=t; t=y0;y0=y1;y1=t; }
    if (y1 > y2) { int16_t t; t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t; }
    if (y0 > y1) { int16_t t; t=x0;x0=x1;x1=t; t=y0;y0=y1;y1=t; }

    if (y0 == y2) {
        int16_t lo = x0, hi = x0;
        if (x1 < lo) lo = x1; else if (x1 > hi) hi = x1;
        if (x2 < lo) lo = x2; else if (x2 > hi) hi = x2;
        ili9488_hline(lo, y0, hi - lo + 1, color);
        return;
    }

    int32_t dx01 = x1-x0, dy01 = y1-y0, dx02 = x2-x0, dy02 = y2-y0,
            dx12 = x2-x1, dy12 = y2-y1;
    int32_t sa = 0, sb = 0;

    int16_t last = (y1 == y2) ? y1 : y1 - 1;
    for (int16_t y = y0; y <= last; y++) {
        int16_t a = x0 + sa / dy01;
        int16_t b = x0 + sb / dy02;
        sa += dx01; sb += dx02;
        if (a > b) { int16_t t = a; a = b; b = t; }
        ili9488_hline(a, y, b - a + 1, color);
    }

    sa = (int32_t)dx12 * (last + 1 - y1);
    sb = (int32_t)dx02 * (last + 1 - y0);
    for (int16_t y = last + 1; y <= y2; y++) {
        int16_t a = x1 + sa / dy12;
        int16_t b = x0 + sb / dy02;
        sa += dx12; sb += dx02;
        if (a > b) { int16_t t = a; a = b; b = t; }
        ili9488_hline(a, y, b - a + 1, color);
    }
}
