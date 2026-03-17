/*
 * ili9341.c — ILI9341 SPI driver for RP2350 (Pico 2)
 *
 * Full initialisation sequence including power control, VCOM,
 * and gamma — required by most ILI9341 modules to actually
 * produce an image.  Minimal init (just SLPOUT+DISPON) leaves
 * many modules with a lit backlight but a blank panel.
 *
 * Reference: ILI9341 datasheet §8 (command table).
 */

#include "ili9341.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

/* ---- ILI9341 command bytes ------------------------------------------- */
#define CMD_NOP         0x00
#define CMD_SWRESET     0x01
#define CMD_SLPOUT      0x11
#define CMD_INVOFF      0x20
#define CMD_DISPON      0x29
#define CMD_CASET       0x2A
#define CMD_RASET       0x2B
#define CMD_RAMWR       0x2C
#define CMD_MADCTL      0x36
#define CMD_COLMOD      0x3A

/* Extended commands */
#define CMD_FRMCTR1     0xB1    /* Frame rate control (normal mode)     */
#define CMD_DFUNCTR     0xB6    /* Display function control             */
#define CMD_PWCTR1      0xC0    /* Power control 1                      */
#define CMD_PWCTR2      0xC1    /* Power control 2                      */
#define CMD_VMCTR1      0xC5    /* VCOM control 1                       */
#define CMD_VMCTR2      0xC7    /* VCOM control 2                       */
#define CMD_PWCTRA      0xCB    /* Power control A                      */
#define CMD_PWCTRB      0xCF    /* Power control B                      */
#define CMD_DTMCTRA     0xE8    /* Driver timing control A              */
#define CMD_DTMCTRB     0xEA    /* Driver timing control B              */
#define CMD_PWONSEQ     0xED    /* Power on sequence control            */
#define CMD_PRC         0xF7    /* Pump ratio control                   */
#define CMD_GMCTRP1     0xE0    /* Positive gamma correction            */
#define CMD_GMCTRN1     0xE1    /* Negative gamma correction            */

/* MADCTL flags */
#define MADCTL_MY       0x80
#define MADCTL_MX       0x40
#define MADCTL_MV       0x20
#define MADCTL_ML       0x10
#define MADCTL_BGR      0x08
#define MADCTL_MH       0x04

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

/* Send command followed by parameter bytes. */
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

void ili9341_init(void)
{
    /* --- SPI peripheral ------------------------------------------------ */
    spi_init(DISP_SPI_INST, DISP_SPI_BAUD);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

    /* --- Control GPIOs ------------------------------------------------- */
    gpio_init(PIN_CS);   gpio_set_dir(PIN_CS,  GPIO_OUT); gpio_put(PIN_CS,  1);
    gpio_init(PIN_DC);   gpio_set_dir(PIN_DC,  GPIO_OUT);
    gpio_init(PIN_RST);  gpio_set_dir(PIN_RST, GPIO_OUT);
    gpio_init(PIN_BL);   gpio_set_dir(PIN_BL,  GPIO_OUT); gpio_put(PIN_BL,  0);

    /* --- Reset --------------------------------------------------------- */
    hard_reset();

    write_cmd(CMD_SWRESET);
    sleep_ms(150);

    write_cmd(CMD_SLPOUT);
    sleep_ms(150);

    /* --- Power control A ----------------------------------------------- */
    write_cmd_data(CMD_PWCTRA,  (const uint8_t[]){0x39, 0x2C, 0x00, 0x34, 0x02}, 5);

    /* --- Power control B ----------------------------------------------- */
    write_cmd_data(CMD_PWCTRB,  (const uint8_t[]){0x00, 0xC1, 0x30}, 3);

    /* --- Driver timing control A --------------------------------------- */
    write_cmd_data(CMD_DTMCTRA, (const uint8_t[]){0x85, 0x00, 0x78}, 3);

    /* --- Driver timing control B --------------------------------------- */
    write_cmd_data(CMD_DTMCTRB, (const uint8_t[]){0x00, 0x00}, 2);

    /* --- Power on sequence control ------------------------------------- */
    write_cmd_data(CMD_PWONSEQ, (const uint8_t[]){0x64, 0x03, 0x12, 0x81}, 4);

    /* --- Pump ratio control -------------------------------------------- */
    write_cmd_data(CMD_PRC,     (const uint8_t[]){0x20}, 1);

    /* --- Power control 1: GVDD = 4.60V -------------------------------- */
    write_cmd_data(CMD_PWCTR1,  (const uint8_t[]){0x23}, 1);

    /* --- Power control 2 ----------------------------------------------- */
    write_cmd_data(CMD_PWCTR2,  (const uint8_t[]){0x10}, 1);

    /* --- VCOM control 1 ------------------------------------------------ */
    write_cmd_data(CMD_VMCTR1,  (const uint8_t[]){0x3E, 0x28}, 2);

    /* --- VCOM control 2 ------------------------------------------------ */
    write_cmd_data(CMD_VMCTR2,  (const uint8_t[]){0x86}, 1);

    /* --- Memory access: portrait, BGR sub-pixel order ------------------ */
    write_cmd_data(CMD_MADCTL,  (const uint8_t[]){MADCTL_MX | MADCTL_BGR}, 1);

    /* --- Pixel format: 16-bit RGB565 ----------------------------------- */
    write_cmd_data(CMD_COLMOD,  (const uint8_t[]){0x55}, 1);

    /* --- Frame rate: 70 Hz (default division, 24 clocks/line) ---------- */
    write_cmd_data(CMD_FRMCTR1, (const uint8_t[]){0x00, 0x18}, 2);

    /* --- Display function control -------------------------------------- */
    write_cmd_data(CMD_DFUNCTR, (const uint8_t[]){0x08, 0x82, 0x27}, 3);

    /* --- Inversion off ------------------------------------------------- */
    write_cmd(CMD_INVOFF);

    /* --- Positive gamma correction ------------------------------------- */
    write_cmd_data(CMD_GMCTRP1, (const uint8_t[]){
        0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
        0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00
    }, 15);

    /* --- Negative gamma correction ------------------------------------- */
    write_cmd_data(CMD_GMCTRN1, (const uint8_t[]){
        0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
        0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F
    }, 15);

    /* --- Display ON ---------------------------------------------------- */
    write_cmd(CMD_DISPON);
    sleep_ms(100);

    /* --- Backlight ON -------------------------------------------------- */
    gpio_put(PIN_BL, 1);
}

/* ---- Drawing --------------------------------------------------------- */

void ili9341_fill(uint16_t color)
{
    ili9341_fill_rect(0, 0, DISP_WIDTH - 1, DISP_HEIGHT - 1, color);
}

void ili9341_fill_rect(uint16_t x0, uint16_t y0,
                       uint16_t x1, uint16_t y1,
                       uint16_t color)
{
    set_addr_window(x0, y0, x1, y1);

    uint16_t w = (x1 - x0) + 1;
    uint16_t h = (y1 - y0) + 1;

    uint8_t hi = color >> 8;
    uint8_t lo = color & 0xFF;

    /* Scanline buffer on stack — max 480 bytes for 240px wide */
    uint8_t line[DISP_WIDTH * 2];
    for (uint16_t i = 0; i < w; i++) {
        line[i * 2]     = hi;
        line[i * 2 + 1] = lo;
    }

    gpio_put(PIN_DC, 1);
    cs_select();
    for (uint16_t row = 0; row < h; row++) {
        spi_write_blocking(DISP_SPI_INST, line, w * 2);
    }
    cs_deselect();
}

void ili9341_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    ili9341_fill_rect(x, y, x, y, color);
}
