/*
 * display_spi.c — ILI9488 SPI display backend for RP2350
 *
 * Provides strong fb_init() and fb_flip_display() that override the
 * weak stubs in framebuffer.c.
 *
 * Architecture:
 *   Drawing code → RGB565 shadow buffer (307 KB in SRAM)
 *   fb_present() → RGB565→RGB666 conversion → SPI → ILI9488
 *
 * The shadow buffer eliminates all stack-allocated line buffers.
 * SPI transfer uses a small static scanline buffer (960 bytes).
 */

#include "framebuffer.h"
#include "display_spi.h"
#include "fb_pixel.h"
#include "pins.h"

#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/gpio.h"

/* ---- ILI9488 commands ------------------------------------------------ */
#define CMD_SWRESET     0x01
#define CMD_SLPOUT      0x11
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
#define CMD_VMCTR1      0xC5
#define CMD_PGAMCTRL    0xE0
#define CMD_NGAMCTRL    0xE1
#define CMD_ADJCTL3     0xF7

#define MADCTL_MY       0x80
#define MADCTL_MX       0x40
#define MADCTL_MV       0x20
#define MADCTL_BGR      0x08

/* ---- Shadow buffer (static, in .bss → SRAM) ------------------------- */
/* 320 × 480 × 2 bytes = 307,200 bytes. RP2350 has 520 KB. */
static uint16_t shadow_buf[DISP_WIDTH * DISP_HEIGHT];

/* Scanline conversion buffer: 320 × 3 = 960 bytes (RGB666 wire format) */
static uint8_t spi_line[DISP_WIDTH * 3];

/* ---- SPI helpers ----------------------------------------------------- */

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
    if (len > 0) write_data(params, len);
}

/* ---- Hardware reset -------------------------------------------------- */

static void hard_reset(void)
{
    gpio_put(PIN_RST, 1); sleep_ms(10);
    gpio_put(PIN_RST, 0); sleep_ms(50);
    gpio_put(PIN_RST, 1); sleep_ms(150);
}

/* ---- ILI9488 initialisation ------------------------------------------ */

static void ili9488_hw_init(void)
{
    /* SPI */
    spi_init(DISP_SPI_INST, DISP_SPI_BAUD);
    gpio_set_function(PIN_SCK,  GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);

    /* Control GPIOs */
    gpio_init(PIN_CS);  gpio_set_dir(PIN_CS,  GPIO_OUT); gpio_put(PIN_CS,  1);
    gpio_init(PIN_DC);  gpio_set_dir(PIN_DC,  GPIO_OUT);
    gpio_init(PIN_RST); gpio_set_dir(PIN_RST, GPIO_OUT);

    hard_reset();

    write_cmd(CMD_SWRESET); sleep_ms(150);
    write_cmd(CMD_SLPOUT);  sleep_ms(150);

    /* Gamma */
    write_cmd_data(CMD_PGAMCTRL, (const uint8_t[]){
        0x00,0x03,0x09,0x08,0x16,0x0A,0x3F,0x78,
        0x4C,0x09,0x0A,0x08,0x16,0x1A,0x0F}, 15);
    write_cmd_data(CMD_NGAMCTRL, (const uint8_t[]){
        0x00,0x16,0x19,0x03,0x0F,0x05,0x32,0x45,
        0x46,0x04,0x0E,0x0D,0x35,0x37,0x0F}, 15);

    /* Power */
    write_cmd_data(CMD_PWCTR1, (const uint8_t[]){0x17, 0x15}, 2);
    write_cmd_data(CMD_PWCTR2, (const uint8_t[]){0x41}, 1);
    write_cmd_data(CMD_VMCTR1, (const uint8_t[]){0x00, 0x12, 0x80}, 3);

    /* MADCTL: portrait, BGR subpixel order (panel is physically BGR) */
    write_cmd_data(CMD_MADCTL, (const uint8_t[]){MADCTL_MX | MADCTL_BGR}, 1);

    /* 18-bit pixel format (RGB666, 3 bytes/pixel) — CRITICAL for SPI mode */
    write_cmd_data(CMD_COLMOD, (const uint8_t[]){0x66}, 1);

    /* Interface / timing */
    write_cmd_data(CMD_IFMODE,  (const uint8_t[]){0x00}, 1);
    write_cmd_data(CMD_FRMCTR1,(const uint8_t[]){0xA0}, 1);
    write_cmd_data(CMD_INVTR,  (const uint8_t[]){0x02}, 1);
    write_cmd_data(CMD_DFUNCTR,(const uint8_t[]){0x02, 0x02, 0x3B}, 3);
    write_cmd_data(CMD_ETMOD,  (const uint8_t[]){0xC6}, 1);
    write_cmd_data(CMD_ADJCTL3,(const uint8_t[]){0xA9, 0x51, 0x2C, 0x82}, 4);

    /* INVON: this panel's LC orientation requires inversion-on for
     * correct colours. Without it WHITE↔BLACK are swapped and all
     * colours are inverted. Common on ILI9488 modules. */
    write_cmd(0x21);  /* CMD_INVON */

    write_cmd(CMD_DISPON);
    sleep_ms(100);
}

/* ---- Set full-screen address window ---------------------------------- */

static void set_full_window(void)
{
    uint8_t buf[4];

    write_cmd(CMD_CASET);
    buf[0] = 0; buf[1] = 0;
    buf[2] = (DISP_WIDTH - 1) >> 8; buf[3] = (DISP_WIDTH - 1) & 0xFF;
    write_data(buf, 4);

    write_cmd(CMD_RASET);
    buf[0] = 0; buf[1] = 0;
    buf[2] = (DISP_HEIGHT - 1) >> 8; buf[3] = (DISP_HEIGHT - 1) & 0xFF;
    write_data(buf, 4);

    write_cmd(CMD_RAMWR);
}

/* ---- Present: push shadow buffer over SPI as RGB666 ------------------ */
/* CS stays LOW for the entire CASET → RASET → RAMWR → pixel data sequence.
 * Only DC toggles between command and data phases. Some ILI9488 modules
 * reset the write window if CS bounces between RAMWR and pixel data. */

void display_spi_present(framebuffer_t *fb)
{
    const uint16_t *src = (const uint16_t *)fb->addr;
    uint8_t cmd, buf[4];

    cs_select();   /* CS low for entire transaction */

    /* CASET (column address set) */
    gpio_put(PIN_DC, 0);
    cmd = CMD_CASET;
    spi_write_blocking(DISP_SPI_INST, &cmd, 1);
    gpio_put(PIN_DC, 1);
    buf[0] = 0; buf[1] = 0;
    buf[2] = (DISP_WIDTH - 1) >> 8; buf[3] = (DISP_WIDTH - 1) & 0xFF;
    spi_write_blocking(DISP_SPI_INST, buf, 4);

    /* RASET (row address set) */
    gpio_put(PIN_DC, 0);
    cmd = CMD_RASET;
    spi_write_blocking(DISP_SPI_INST, &cmd, 1);
    gpio_put(PIN_DC, 1);
    buf[0] = 0; buf[1] = 0;
    buf[2] = (DISP_HEIGHT - 1) >> 8; buf[3] = (DISP_HEIGHT - 1) & 0xFF;
    spi_write_blocking(DISP_SPI_INST, buf, 4);

    /* RAMWR (memory write) */
    gpio_put(PIN_DC, 0);
    cmd = CMD_RAMWR;
    spi_write_blocking(DISP_SPI_INST, &cmd, 1);

    /* Pixel data — DC stays high, CS stays low */
    gpio_put(PIN_DC, 1);

    for (uint32_t y = 0; y < DISP_HEIGHT; y++) {
        const uint16_t *row = src + y * DISP_WIDTH;

        for (uint32_t x = 0; x < DISP_WIDTH; x++) {
            uint16_t c = row[x];
            uint8_t r = ((c >> 11) & 0x1F); r = (r << 3) | (r >> 2);
            uint8_t g = ((c >>  5) & 0x3F); g = (g << 2) | (g >> 4);
            uint8_t b = ( c        & 0x1F); b = (b << 3) | (b >> 2);
            spi_line[x * 3 + 0] = r;
            spi_line[x * 3 + 1] = g;
            spi_line[x * 3 + 2] = b;
        }

        spi_write_blocking(DISP_SPI_INST, spi_line, DISP_WIDTH * 3);
    }

    cs_deselect();  /* CS high — transaction complete */
}

/* Also wire it as fb_flip_display for fb_present() */
void fb_flip_display(framebuffer_t *fb)
{
    display_spi_present(fb);
}

/* ---- Init for RP2350 ------------------------------------------------- */

bool display_spi_init(framebuffer_t *fb)
{
    /* Init display hardware */
    ili9488_hw_init();

    /* Configure framebuffer struct */
    fb->width          = DISP_WIDTH;
    fb->height         = DISP_HEIGHT;
    fb->pitch          = DISP_WIDTH * 2;   /* 2 bytes per pixel (RGB565) */
    fb->pixel_format   = FB_FORMAT_RGB565;
    fb->addr           = (uint32_t *)(void *)shadow_buf;
    fb->buffers[0]     = fb->addr;
    fb->front_buffer   = 0;
    fb->back_buffer    = 0;
    fb->buffer_size    = DISP_WIDTH * DISP_HEIGHT * 2;
    fb->virtual_height = DISP_HEIGHT;

    /* Init clip stack to full screen */
    fb->clip_depth     = 0;
    fb->clip_stack[0]  = (fb_clip_t){0, 0, DISP_WIDTH, DISP_HEIGHT};

    fb->dirty_count    = 0;
    fb->full_dirty     = true;
    fb->frame_count    = 0;
    fb->vsync_enabled  = false;
    fb->initialized    = true;

    return true;
}
