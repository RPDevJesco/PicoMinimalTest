/*
 * ili9488.c — ILI9488 SPI driver for RP2350 (Pico 2)
 *
 * CRITICAL: In SPI mode the ILI9488 only supports 18-bit colour
 * (RGB666).  Each pixel is 3 bytes on the wire.  The COLMOD
 * register must be set to 0x66, NOT 0x55.  This is the single
 * biggest difference from the ILI9341.
 *
 * Reference: ILI9488 datasheet rev 1.0, §6 (command table).
 */

#include "ili9488.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

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

/* Extended commands */
#define CMD_IFMODE      0xB0    /* Interface mode control               */
#define CMD_FRMCTR1     0xB1    /* Frame rate control (normal mode)     */
#define CMD_INVTR       0xB4    /* Display inversion control            */
#define CMD_DFUNCTR     0xB6    /* Display function control             */
#define CMD_ETMOD       0xB7    /* Entry mode set                       */
#define CMD_PWCTR1      0xC0    /* Power control 1                      */
#define CMD_PWCTR2      0xC1    /* Power control 2                      */
#define CMD_PWCTR3      0xC2    /* Power control 3                      */
#define CMD_VMCTR1      0xC5    /* VCOM control 1                       */
#define CMD_PGAMCTRL    0xE0    /* Positive gamma control               */
#define CMD_NGAMCTRL    0xE1    /* Negative gamma control               */
#define CMD_ADJCTL3     0xF7    /* Adjust control 3                     */

/* MADCTL flags */
#define MADCTL_MY       0x80
#define MADCTL_MX       0x40
#define MADCTL_MV       0x20
#define MADCTL_ML       0x10
#define MADCTL_BGR      0x08
#define MADCTL_MH       0x04

/* ---- RGB565 → RGB666 conversion -------------------------------------- */

static inline void rgb565_to_666(uint16_t c, uint8_t *r, uint8_t *g, uint8_t *b)
{
    /* Expand 5-6-5 back to 8-8-8; the ILI9488 uses top 6 bits. */
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

static void write_data_byte(uint8_t val)
{
    write_data(&val, 1);
}

static void write_cmd_data(uint8_t cmd, const uint8_t *params, size_t len)
{
    write_cmd(cmd);
    if (len > 0) {
        write_data(params, len);
    }
}

/* ---- Column / Row address window ------------------------------------- */

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

/* ---- Full initialisation --------------------------------------------- */

void ili9488_init(void)
{
    /* --- SPI peripheral ------------------------------------------------ */
    spi_init(DISP_SPI_INST, DISP_SPI_BAUD);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    /* MISO not strictly needed for write-only, but claim the pin */
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    /* --- Control GPIOs ------------------------------------------------- */
    gpio_init(PIN_CS);   gpio_set_dir(PIN_CS,  GPIO_OUT); gpio_put(PIN_CS,  1);
    gpio_init(PIN_DC);   gpio_set_dir(PIN_DC,  GPIO_OUT);
    gpio_init(PIN_RST);  gpio_set_dir(PIN_RST, GPIO_OUT);

    /* --- Reset --------------------------------------------------------- */
    hard_reset();

    write_cmd(CMD_SWRESET);
    sleep_ms(150);

    write_cmd(CMD_SLPOUT);
    sleep_ms(150);

    /* --- Positive gamma control ---------------------------------------- */
    write_cmd_data(CMD_PGAMCTRL, (const uint8_t[]){
        0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78,
        0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F
    }, 15);

    /* --- Negative gamma control ---------------------------------------- */
    write_cmd_data(CMD_NGAMCTRL, (const uint8_t[]){
        0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45,
        0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F
    }, 15);

    /* --- Power control 1: VREG1OUT = 4.50V ----------------------------- */
    write_cmd_data(CMD_PWCTR1, (const uint8_t[]){0x17, 0x15}, 2);

    /* --- Power control 2 ----------------------------------------------- */
    write_cmd_data(CMD_PWCTR2, (const uint8_t[]){0x41}, 1);

    /* --- VCOM control -------------------------------------------------- */
    write_cmd_data(CMD_VMCTR1, (const uint8_t[]){0x00, 0x12, 0x80}, 3);

    /* --- Memory access: portrait, BGR ---------------------------------- */
    write_cmd_data(CMD_MADCTL, (const uint8_t[]){MADCTL_MX | MADCTL_BGR}, 1);

    /* --- CRITICAL: 18-bit pixel format (RGB666, 3 bytes/pixel) --------- */
    /*     0x66 = 18 bit/pixel.  0x55 (RGB565) does NOT work in SPI mode! */
    write_cmd_data(CMD_COLMOD, (const uint8_t[]){0x66}, 1);

    /* --- Interface mode control ---------------------------------------- */
    write_cmd_data(CMD_IFMODE, (const uint8_t[]){0x00}, 1);

    /* --- Frame rate: 60 Hz --------------------------------------------- */
    write_cmd_data(CMD_FRMCTR1, (const uint8_t[]){0xA0}, 1);

    /* --- Display inversion control: column inversion ------------------- */
    write_cmd_data(CMD_INVTR, (const uint8_t[]){0x02}, 1);

    /* --- Display function control -------------------------------------- */
    write_cmd_data(CMD_DFUNCTR, (const uint8_t[]){0x02, 0x02, 0x3B}, 3);

    /* --- Entry mode set ------------------------------------------------ */
    write_cmd_data(CMD_ETMOD, (const uint8_t[]){0xC6}, 1);

    /* --- Adjust control 3 ---------------------------------------------- */
    write_cmd_data(CMD_ADJCTL3, (const uint8_t[]){0xA9, 0x51, 0x2C, 0x82}, 4);

    /* --- Display ON ---------------------------------------------------- */
    write_cmd(CMD_DISPON);
    sleep_ms(100);
}

/* ---- Drawing --------------------------------------------------------- */

void ili9488_fill(uint16_t color)
{
    ili9488_fill_rect(0, 0, DISP_WIDTH - 1, DISP_HEIGHT - 1, color);
}

void ili9488_fill_rect(uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1,
                       uint16_t color)
{
    set_addr_window(x0, y0, x1, y1);

    uint16_t w = (x1 - x0) + 1;
    uint16_t h = (y1 - y0) + 1;

    /* Convert RGB565 → three 8-bit channels for RGB666 wire format */
    uint8_t r, g, b;
    rgb565_to_666(color, &r, &g, &b);

    /* Scanline buffer: 3 bytes per pixel, max 320 pixels = 960 bytes */
    uint8_t line[DISP_WIDTH * 3];
    for (uint16_t i = 0; i < w; i++) {
        line[i * 3]     = r;
        line[i * 3 + 1] = g;
        line[i * 3 + 2] = b;
    }

    gpio_put(PIN_DC, 1);
    cs_select();
    for (uint16_t row = 0; row < h; row++) {
        spi_write_blocking(DISP_SPI_INST, line, w * 3);
    }
    cs_deselect();
}

void ili9488_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    ili9488_fill_rect(x, y, x, y, color);
}
