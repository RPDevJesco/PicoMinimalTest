/*
 * display_spi.h — ILI9488 SPI display backend
 */
#ifndef DISPLAY_SPI_H
#define DISPLAY_SPI_H

#include "framebuffer.h"

/* Init display hardware + configure framebuffer for RGB565 shadow buffer */
bool display_spi_init(framebuffer_t *fb);

/* Push entire shadow buffer to display over SPI (RGB565 → RGB666) */
void display_spi_present(framebuffer_t *fb);

#endif
