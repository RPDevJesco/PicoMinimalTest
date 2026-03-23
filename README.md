# LAFVIN Pico 2 Dev Kit — Interactive Display Demo

An interactive hardware showcase for the **LAFVIN Pico 2 Development Kit**, driving a 3.5" ILI9488 480×320 TFT display over SPI from the RP2350 microcontroller. Uses a framebuffer architecture adapted from [Tutorial-OS](https://github.com/nicktasios/tutorial-os) to render graphics into an RGB565 shadow buffer in SRAM, then push frames to the display as RGB666 over SPI.

## What It Does

Six interactive scenes demonstrate the board's peripherals:

| Scene | Content |
|-------|---------|
| **Splash** | Boot screen with system info, color circles, touch detection status |
| **Colors** | Tap or joystick to cycle through 8 solid color fills with labels |
| **Shapes** | Rectangles, circles, triangle, rounded rects, line starburst |
| **Gradients** | Horizontal, vertical, and full-rainbow gradient fills |
| **Text** | 8×8 bitmap font rendered at 5 different scales, full character set |
| **System Info** | Live readout of touch position, joystick ADC values, button state |

### Controls

- **Joystick left/right** — Navigate between scenes
- **Touch tap** — Next scene (cycles colors in the Colors scene)
- **K2 button (GP14)** — Audio ON (ascending chime confirms)
- **K1 button (GP15)** — Audio OFF (immediate silence)

Navigation sounds only play when audio is enabled. A status bar at the bottom of every scene shows audio state and current scene number.

https://github.com/user-attachments/assets/038a5f96-5b88-49f7-a4ca-1698bde3e856

## Hardware

**Board:** LAFVIN Pico 2 Development Kit with integrated 3.5" capacitive touch TFT

**MCU:** RP2350A (Dual Cortex-M33, 150 MHz, 520 KB SRAM)

| Peripheral | Connection | Pins |
|-----------|-----------|------|
| ILI9488 display | SPI0, 10 MHz | GP2 (SCK), GP3 (MOSI), GP4 (MISO), GP5 (CS), GP6 (DC), GP7 (RST) |
| Capacitive touch | I2C0, 400 kHz | GP8 (SDA), GP9 (SCL), GP10 (RST), GP11 (INT) |
| Buzzer | PWM | GP13 |
| Buttons K1/K2 | GPIO, active-low | GP15 (K1), GP14 (K2) |
| Analog joystick | ADC0/ADC1 | GP26 (X), GP27 (Y) |
| RGB LED (WS2812) | — (not used yet) | GP12 |
| Onboard LED | GPIO | GP25 |

## Architecture

```
  Drawing API (ARGB8888 colors)
       │
       ▼
  framebuffer.c ──► RGB565 shadow buffer (307 KB in SRAM)
                         │
                         ▼
  display_spi.c ──► RGB565→RGB666 scanline conversion (960-byte static buffer)
                         │
                         ▼
                    SPI0 @ 10 MHz ──► ILI9488
```

All drawing functions accept standard ARGB8888 colors (`0xAARRGGBB`). The framebuffer stores pixels as RGB565 (2 bytes each) to fit within the RP2350's 520 KB SRAM. When `display_spi_present()` is called, each scanline is converted from RGB565 to RGB666 (3 bytes per pixel — the only format the ILI9488 supports in SPI mode) and pushed over the bus.

### Memory Layout

| Region | Size | Purpose |
|--------|------|---------|
| `.text` | ~27 KB | Code (framebuffer drawing, SPI driver, font data, demo logic) |
| `.bss` | ~310 KB | Shadow buffer (307,200 bytes) + SPI line buffer (960 bytes) + state |
| **Total** | **~337 KB** | Leaves ~183 KB free of the 520 KB SRAM |

### Key Design Decisions

**Shadow buffer, not direct-to-SPI.** The original approach wrote pixels directly over SPI using stack-allocated line buffers. This caused stack overflows during nested draw calls and made multi-frame rendering unreliable. The shadow buffer eliminates all stack allocation in the drawing path — every buffer is static.

**Single buffer, explicit present.** The RP2350 has no scan-out hardware — there's no GPU reading from a framebuffer autonomously. The CPU *is* the scan-out. `display_spi_present()` is an explicit "push this frame now" call, not a buffer swap. Double-buffering would waste 307 KB of the 520 KB available.

**CS held low for entire transaction.** The ILI9488 on this board requires CS to stay asserted for the full CASET→RASET→RAMWR→pixel_data sequence. Bouncing CS between commands causes the controller to lose its write window — the first frame renders but subsequent frames silently fail. This was the hardest bug to find.

## ILI9488 Panel-Specific Findings

These settings were determined empirically through iterative testing on this specific LAFVIN board:

| Setting | Value | Why |
|---------|-------|-----|
| `COLMOD` | `0x66` (18-bit) | The ILI9488 **only supports RGB666 in SPI mode**. Setting `0x55` (RGB565) produces garbage. Each pixel is 3 bytes on the wire. |
| `MADCTL` | `0x48` (MX + BGR) | The panel's subpixel order is physically BGR. Without the BGR flag, red and blue are swapped. |
| `INVON` | `0x21` | This panel's LC cell orientation requires display inversion enabled. Without it, white↔black are swapped and all colors are bitwise-inverted. |
| SPI speed | 10 MHz | Higher speeds (40 MHz) produce noise — the board traces can't handle it. |
| CS protocol | Held low | CS must stay asserted for the entire command+data transaction. See above. |

## Building

### Prerequisites

- Docker (recommended), **or**
- `arm-none-eabi-gcc`, CMake 3.13+, Python 3, and the [Pico SDK 2.1.1](https://github.com/raspberrypi/pico-sdk)

### With Docker (any OS)

```bash
./build.bat         # Build only
./build.sh          # Build only
./build.sh flash    # Build + copy .uf2 to mounted Pico
./build.sh clean    # Remove build directory
```

Or using Make:

```bash
make                # Build
make flash          # Build + flash
make clean          # Clean
```

The Docker image (`Dockerfile`) includes the ARM cross-compiler and Pico SDK. It's cached after the first build.

### Without Docker

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir build && cd build
cmake -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s ..
make -j$(nproc)
```

The output is `build/src/pico2_ili9341.uf2`.

### Flashing

1. Hold **BOOTSEL** on the Pico 2
2. Plug in USB
3. Copy `pico2_ili9341.uf2` to the mounted `RP2350` drive

On macOS: `cp build/src/pico2_ili9341.uf2 /Volumes/RP2350/`

On Linux: `cp build/src/pico2_ili9341.uf2 /media/$USER/RP2350/`

## Project Structure

```
├── CMakeLists.txt            Root CMake (SDK init, C11/C++17)
├── pico_sdk_import.cmake     Pico SDK import helper
├── Dockerfile                Ubuntu 24.04 + ARM cross-compiler + SDK 2.1.1
├── Makefile                  Docker build wrapper
├── build.sh                  Shell build wrapper
└── src/
    ├── CMakeLists.txt        Build target, libraries, compiler flags
    │
    │── hal_types.h           HAL shim (barriers, volatile, pixel format enum)
    │── fb_pixel.h            RGB565 ↔ ARGB8888 inline converters
    │── framebuffer.h         Framebuffer struct + full drawing API
    │── framebuffer.c         Drawing primitives (adapted from Tutorial-OS)
    │
    │── pins.h                Board pin definitions
    │── display_spi.h         ILI9488 SPI backend API
    │── display_spi.c         Hardware init, shadow buffer, SPI present
    │
    │── input.h / input.c     Debounced K1/K2 buttons (GPIO, active-low)
    │── buzzer.h / buzzer.c   PWM tone generation (GP13)
    │── joystick.h / .c       Analog joystick via ADC (GP26/27, auto-calibrated)
    │── touch.h / touch.c     Capacitive touch (FT6236/GT911 auto-detect, I2C)
    │
    └── main.c                6-scene interactive demo
```

## Framebuffer API

The framebuffer API is a subset of the [Tutorial-OS](https://github.com/nicktasios/tutorial-os) framebuffer driver, adapted for standalone Pico SDK use. All functions take ARGB8888 colors (`0xAARRGGBB`). On RP2350, these are converted to RGB565 on write via `fb_pixel.h` helpers.

Full-screen present takes approximately 100ms at 10 MHz SPI (320×480×3 bytes = 460,800 bytes ÷ 10 Mbit/s).

## USB stdio

USB stdio is **disabled** at compile time (`pico_enable_stdio_usb 0`). Early testing revealed that enabling it caused hard faults — likely from unhandled USB interrupts firing before TinyUSB was fully initialized. For UART debug output, change the CMakeLists.txt settings and connect a serial adapter to the UART pins.

## License

This project adapts framebuffer code from Tutorial-OS. The ILI9488 driver, peripheral drivers, and demo code are original work for this board.
