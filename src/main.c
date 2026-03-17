/*
 * main.c — Pico 2 + ILI9488 proof-of-life
 *
 * Initialises the display then cycles through solid colours so you
 * can verify the SPI link, reset, and panel are all working.
 */

#include "pico/stdlib.h"
#include "ili9488.h"

static const uint16_t palette[] = {
    COLOR_RED,
    COLOR_GREEN,
    COLOR_BLUE,
    COLOR_CYAN,
    COLOR_MAGENTA,
    COLOR_YELLOW,
    COLOR_WHITE,
    COLOR_BLACK,
};

#define PALETTE_LEN (sizeof(palette) / sizeof(palette[0]))

int main(void)
{
    stdio_init_all();
    ili9488_init();

    uint8_t idx = 0;

    while (1) {
        ili9488_fill(palette[idx]);
        idx = (idx + 1) % PALETTE_LEN;
        sleep_ms(1000);
    }

    return 0;
}
