/*
 * pins.h — SPI0 + ILI9488 pin mapping for LAFVIN Pico 2 Dev Kit
 *
 * Read from the board silkscreen:
 *   GP2=SCLK  GP3=MOSI  GP4=MISO  GP5=CS
 *   GP6=DC    GP7=RST
 *   GP8=SDA(I2C touch)  GP9=SCL(I2C touch)  GP11=TPINT
 *
 * Backlight appears to be hardwired on (no BL GPIO labelled).
 */

#ifndef PINS_H
#define PINS_H

/* ---- SPI peripheral -------------------------------------------------- */
#define DISP_SPI_INST   spi0
#define DISP_SPI_BAUD   (10 * 1000 * 1000)   /* 10 MHz — conservative */

/* ---- SPI0 data lines (alternate pin set) ----------------------------- */
#define PIN_SCK         2       /* SPI0 SCK  */
#define PIN_MOSI        3       /* SPI0 TX   */
#define PIN_MISO        4       /* SPI0 RX   */

/* ---- Display control lines ------------------------------------------- */
#define PIN_CS          5       /* Chip-select                           */
#define PIN_DC          6       /* Data / Command                        */
#define PIN_RST         7       /* Hardware reset (active-low)           */

/* ---- Touch (I2C — not used yet) -------------------------------------- */
#define PIN_TP_SDA      8
#define PIN_TP_SCL      9
#define PIN_TP_INT      11

/* ---- Display geometry (ILI9488: 320 wide × 480 tall) ----------------- */
#define DISP_WIDTH      320
#define DISP_HEIGHT     480

#endif /* PINS_H */
