/*
 * main.c — LAFVIN Pico 2 Dev Kit Interactive Demo
 *
 * Navigation: Touch tap / Joystick left-right = switch scenes
 * K2 = audio ON     K1 = audio OFF
 * Status bar shows current state at bottom of each scene.
 */

#include "pico/stdlib.h"
#include "hardware/clocks.h"
#include "framebuffer.h"
#include "display_spi.h"
#include "input.h"
#include "buzzer.h"
#include "joystick.h"
#include "touch.h"
#include "pins.h"

#include <stdio.h>

/* ---- Global state ---------------------------------------------------- */

static framebuffer_t fb = {0};
static bool audio_on     = false;
static bool touch_ok     = false;
static uint8_t scene     = 0;
static bool needs_redraw = true;

#define SCENE_COUNT 6

/* ---- Helpers --------------------------------------------------------- */

static void present(void) { display_spi_present(&fb); }

static void play_nav(void)
{
    if (audio_on) buzzer_beep(NOTE_E5, 30);
}

static void play_toggle_on(void)
{
    buzzer_beep(NOTE_C5, 60);
    sleep_ms(30);
    buzzer_beep(NOTE_E5, 60);
    sleep_ms(30);
    buzzer_beep(NOTE_G5, 80);
}

static void play_toggle_off(void)
{
    buzzer_beep(NOTE_G4, 60);
    sleep_ms(30);
    buzzer_beep(NOTE_E4, 80);
}

/* Status bar at bottom of every scene */
static void draw_status_bar(void)
{
    fb_fill_rect(&fb, 0, 450, 320, 30, FB_COLOR_BLACK);
    fb_draw_string(&fb, 8, 456, audio_on ? "Audio:ON " : "Audio:OFF",
                   audio_on ? FB_COLOR_SUCCESS : FB_COLOR_TEXT_DIM, FB_COLOR_BLACK);

    char buf[20];
    snprintf(buf, sizeof(buf), "Scene %d/%d", scene + 1, SCENE_COUNT);
    fb_draw_string(&fb, 200, 456, buf, FB_COLOR_TEXT_DIM, FB_COLOR_BLACK);

    fb_draw_string(&fb, 8, 468, "Tap/Joy:nav K2:snd+ K1:snd-",
                   FB_COLOR_DARK_GRAY, FB_COLOR_BLACK);
}

/* ---- Scene 0: Splash ------------------------------------------------- */

static void draw_splash(void)
{
    fb_clear(&fb, FB_COLOR_BG);
    fb_draw_string_scaled(&fb, 30, 50, "LAFVIN", FB_COLOR_CYAN, FB_COLOR_BG, 5);
    fb_draw_string_scaled(&fb, 30, 120, "Pico 2 Kit", FB_COLOR_WHITE, FB_COLOR_BG, 2);
    fb_fill_rect(&fb, 30, 155, 260, 2, FB_COLOR_CYAN);

    fb_draw_string(&fb, 30, 175, "Tutorial-OS Framebuffer", FB_COLOR_TEXT_DIM, FB_COLOR_BG);
    fb_draw_string(&fb, 30, 190, "320x480 RGB565 > RGB666 SPI", FB_COLOR_TEXT_DIM, FB_COLOR_BG);

    uint32_t mhz = clock_get_hz(clk_sys) / 1000000;
    char buf[32];
    snprintf(buf, sizeof(buf), "RP2350 @ %lu MHz", (unsigned long)mhz);
    fb_draw_string(&fb, 30, 210, buf, FB_COLOR_TEXT_DIM, FB_COLOR_BG);
    fb_draw_string(&fb, 30, 230, touch_ok ? "Touch: detected" : "Touch: not found",
                   touch_ok ? FB_COLOR_SUCCESS : FB_COLOR_WARNING, FB_COLOR_BG);

    /* Feature list in rounded boxes */
    fb_fill_rounded_rect(&fb, 20, 270, 280, 40, 8, FB_COLOR_PRIMARY);
    fb_draw_string_scaled(&fb, 35, 278, "Framebuffer OK", FB_COLOR_WHITE, FB_COLOR_PRIMARY, 2);

    /* Color circles */
    fb_fill_circle(&fb, 60,  360, 25, FB_COLOR_RED);
    fb_fill_circle(&fb, 120, 360, 25, FB_COLOR_GREEN);
    fb_fill_circle(&fb, 180, 360, 25, FB_COLOR_BLUE);
    fb_fill_circle(&fb, 240, 360, 25, FB_COLOR_YELLOW);

    fb_draw_string(&fb, 30, 410, "Swipe or tap to explore ->",
                   FB_COLOR_TEXT_DIM, FB_COLOR_BG);

    draw_status_bar();
}

/* ---- Scene 1: Color Test --------------------------------------------- */

static uint8_t color_idx = 0;

static const struct { uint32_t color; const char *name; } palette[] = {
    { FB_COLOR_RED,     "RED"     },
    { FB_COLOR_GREEN,   "GREEN"   },
    { FB_COLOR_BLUE,    "BLUE"    },
    { FB_COLOR_CYAN,    "CYAN"    },
    { FB_COLOR_MAGENTA, "MAGENTA" },
    { FB_COLOR_YELLOW,  "YELLOW"  },
    { FB_COLOR_WHITE,   "WHITE"   },
    { FB_COLOR_ORANGE,  "ORANGE"  },
};
#define PAL_LEN 8

static void draw_colors(void)
{
    fb_clear(&fb, palette[color_idx].color);
    uint32_t tfg = (color_idx == 6) ? FB_COLOR_BLACK : FB_COLOR_WHITE;
    fb_draw_string_scaled(&fb, 20, 20, palette[color_idx].name, tfg, palette[color_idx].color, 4);

    /* Next/prev color hint */
    uint8_t prev = (color_idx + PAL_LEN - 1) % PAL_LEN;
    uint8_t next = (color_idx + 1) % PAL_LEN;
    fb_fill_circle(&fb, 40, 240, 20, palette[prev].color);
    fb_draw_circle(&fb, 40, 240, 22, tfg);
    fb_fill_circle(&fb, 280, 240, 20, palette[next].color);
    fb_draw_circle(&fb, 280, 240, 22, tfg);

    fb_draw_string(&fb, 18, 270, "<", tfg, palette[color_idx].color);
    fb_draw_string(&fb, 270, 270, ">", tfg, palette[color_idx].color);

    fb_draw_string_scaled(&fb, 80, 200, "Tap/Joy", tfg, palette[color_idx].color, 2);
    fb_draw_string_scaled(&fb, 60, 230, "to cycle", tfg, palette[color_idx].color, 2);

    draw_status_bar();
}

/* ---- Scene 2: Shapes ------------------------------------------------- */

static void draw_shapes(void)
{
    fb_clear(&fb, FB_COLOR_BG);
    fb_draw_string_scaled(&fb, 20, 10, "Shapes", FB_COLOR_WHITE, FB_COLOR_BG, 3);

    fb_fill_rect(&fb, 20, 55, 80, 80, FB_COLOR_RED);
    fb_fill_rect(&fb, 40, 75, 80, 80, FB_RGB(200, 0, 0));
    fb_draw_rect(&fb, 15, 50, 115, 115, FB_COLOR_WHITE);

    fb_fill_circle(&fb, 230, 100, 50, FB_COLOR_BLUE);
    fb_fill_circle(&fb, 230, 100, 35, FB_COLOR_CYAN);
    fb_fill_circle(&fb, 230, 100, 20, FB_COLOR_WHITE);
    fb_draw_circle(&fb, 230, 100, 55, FB_COLOR_LIGHT_GRAY);

    fb_fill_triangle(&fb, 30, 300, 150, 190, 150, 300, FB_COLOR_GREEN);

    fb_fill_rounded_rect(&fb, 20, 330, 130, 60, 12, FB_COLOR_ORANGE);
    fb_draw_string(&fb, 32, 350, "Rounded!", FB_COLOR_WHITE, FB_COLOR_ORANGE);

    fb_fill_rounded_rect(&fb, 170, 330, 130, 60, 12, FB_COLOR_PURPLE);
    fb_draw_string(&fb, 185, 350, "Smooth!", FB_COLOR_WHITE, FB_COLOR_PURPLE);

    static const int16_t dx[] = {0, 42, 60, 42, 0, -42, -60, -42};
    static const int16_t dy[] = {-60, -42, 0, 42, 60, 42, 0, -42};
    for (int i = 0; i < 8; i++) {
        uint32_t hr = (i * 32) & 0xFF;
        fb_draw_line(&fb, 240, 260, 240 + dx[i], 260 + dy[i],
                     FB_RGB(hr, 100, 255 - hr));
    }

    draw_status_bar();
}

/* ---- Scene 3: Gradients ---------------------------------------------- */

static void draw_gradients(void)
{
    fb_clear(&fb, FB_COLOR_BLACK);
    fb_draw_string_scaled(&fb, 20, 10, "Gradients", FB_COLOR_WHITE, FB_COLOR_BLACK, 3);

    fb_fill_rect_gradient_h(&fb, 10, 50,  300, 50, FB_COLOR_RED, FB_COLOR_BLUE);
    fb_fill_rect_gradient_h(&fb, 10, 110, 300, 50, FB_COLOR_GREEN, FB_COLOR_MAGENTA);
    fb_fill_rect_gradient_h(&fb, 10, 170, 300, 50, FB_COLOR_YELLOW, FB_COLOR_CYAN);
    fb_fill_rect_gradient_v(&fb, 10, 230, 300, 50, FB_COLOR_BLACK, FB_COLOR_WHITE);

    /* Rainbow */
    for (uint32_t x = 10; x < 310; x++) {
        uint32_t hue = (x - 10) * 360 / 300;
        uint32_t region = hue / 60, frac = (hue % 60) * 255 / 60;
        uint32_t r, g, b;
        switch (region) {
            case 0: r=255;g=frac;b=0;break;
            case 1: r=255-frac;g=255;b=0;break;
            case 2: r=0;g=255;b=frac;break;
            case 3: r=0;g=255-frac;b=255;break;
            case 4: r=frac;g=0;b=255;break;
            default:r=255;g=0;b=255-frac;break;
        }
        fb_draw_vline(&fb, x, 300, 60, FB_RGB(r, g, b));
    }

    fb_draw_string(&fb, 12, 55,  "Red > Blue",     FB_COLOR_WHITE, FB_COLOR_RED);
    fb_draw_string(&fb, 12, 115, "Grn > Magenta",  FB_COLOR_BLACK, FB_COLOR_GREEN);
    fb_draw_string(&fb, 12, 175, "Yel > Cyan",     FB_COLOR_BLACK, FB_COLOR_YELLOW);
    fb_draw_string(&fb, 100, 248, "Black > White",  FB_COLOR_GRAY, FB_COLOR_BLACK);
    fb_draw_string(&fb, 100, 375, "Full Rainbow",   FB_COLOR_WHITE, FB_COLOR_BLACK);

    draw_status_bar();
}

/* ---- Scene 4: Text --------------------------------------------------- */

static void draw_text(void)
{
    fb_clear(&fb, FB_COLOR_BG);
    fb_draw_string_scaled(&fb, 20, 10, "Text", FB_COLOR_WHITE, FB_COLOR_BG, 3);

    fb_draw_string(&fb, 10, 50, "Scale 1 (8x8 font)", FB_COLOR_WHITE, FB_COLOR_BG);
    fb_draw_string_scaled(&fb, 10, 70, "Scale 2", FB_COLOR_CYAN, FB_COLOR_BG, 2);
    fb_draw_string_scaled(&fb, 10, 100, "Scale 3", FB_COLOR_GREEN, FB_COLOR_BG, 3);
    fb_draw_string_scaled(&fb, 10, 140, "Scl 4", FB_COLOR_YELLOW, FB_COLOR_BG, 4);
    fb_draw_string_scaled(&fb, 10, 190, "BIG", FB_COLOR_RED, FB_COLOR_BG, 5);

    fb_draw_string(&fb, 10, 250, "ABCDEFGHIJKLMNOPQRST", FB_COLOR_ORANGE, FB_COLOR_BG);
    fb_draw_string(&fb, 10, 265, "UVWXYZ 0123456789", FB_COLOR_ORANGE, FB_COLOR_BG);
    fb_draw_string(&fb, 10, 285, "abcdefghijklmnopqrst", FB_COLOR_PINK, FB_COLOR_BG);
    fb_draw_string(&fb, 10, 300, "uvwxyz !@#$%^&*()", FB_COLOR_PINK, FB_COLOR_BG);

    fb_fill_rect(&fb, 10, 340, 140, 35, FB_COLOR_RED);
    fb_draw_string(&fb, 20, 350, "On Red", FB_COLOR_WHITE, FB_COLOR_RED);
    fb_fill_rect(&fb, 170, 340, 140, 35, FB_COLOR_BLUE);
    fb_draw_string(&fb, 180, 350, "On Blue", FB_COLOR_YELLOW, FB_COLOR_BLUE);

    draw_status_bar();
}

/* ---- Scene 5: System Info (live) ------------------------------------- */

static void draw_sysinfo(void)
{
    fb_clear(&fb, FB_RGB(0, 8, 0));
    uint32_t bg = FB_RGB(0, 8, 0);
    fb_draw_string_scaled(&fb, 20, 10, "System", FB_COLOR_GREEN, bg, 3);

    uint16_t y = 55;
    char buf[40];

    uint32_t mhz = clock_get_hz(clk_sys) / 1000000;
    snprintf(buf, sizeof(buf), "CPU: RP2350 %lu MHz", (unsigned long)mhz);
    fb_draw_string(&fb, 10, y, buf, FB_COLOR_GREEN, bg); y += 16;

    fb_draw_string(&fb, 10, y, "Display: ILI9488 320x480", FB_COLOR_GREEN, bg); y += 16;
    fb_draw_string(&fb, 10, y, "Format: RGB565 > RGB666", FB_COLOR_GREEN, bg); y += 16;

    snprintf(buf, sizeof(buf), "SPI: %d MHz", DISP_SPI_BAUD / 1000000);
    fb_draw_string(&fb, 10, y, buf, FB_COLOR_GREEN, bg); y += 16;

    fb_draw_string(&fb, 10, y, touch_ok ? "Touch: detected" : "Touch: not found",
                   touch_ok ? FB_COLOR_CYAN : FB_COLOR_ERROR, bg); y += 16;

    snprintf(buf, sizeof(buf), "Audio: %s", audio_on ? "ON" : "OFF");
    fb_draw_string(&fb, 10, y, buf, audio_on ? FB_COLOR_CYAN : FB_COLOR_TEXT_DIM, bg); y += 24;

    fb_fill_rect(&fb, 10, y, 300, 1, FB_COLOR_GREEN); y += 10;
    fb_draw_string(&fb, 10, y, "-- Live Input --", FB_COLOR_GREEN, bg); y += 18;

    /* These will be updated live in sysinfo_update */
    draw_status_bar();
}

static void sysinfo_update_live(void)
{
    uint32_t bg = FB_RGB(0, 8, 0);
    uint16_t y = 55 + 16 * 6 + 24 + 10 + 18;
    char buf[40];

    /* Clear live area */
    fb_fill_rect(&fb, 10, y, 300, 100, bg);

    /* Touch */
    touch_point_t tp = touch_read();
    if (tp.pressed) {
        snprintf(buf, sizeof(buf), "Touch: X=%d Y=%d", tp.x, tp.y);
        fb_draw_string(&fb, 10, y, buf, FB_COLOR_YELLOW, bg);
    } else {
        fb_draw_string(&fb, 10, y, "Touch: (none)", FB_COLOR_TEXT_DIM, bg);
    }
    y += 16;

    /* Joystick */
    uint16_t jx = joystick_x_raw();
    uint16_t jy = joystick_y_raw();
    snprintf(buf, sizeof(buf), "Joy: X=%4d Y=%4d", jx, jy);
    fb_draw_string(&fb, 10, y, buf, FB_COLOR_CYAN, bg);
    y += 16;

    /* Buttons */
    snprintf(buf, sizeof(buf), "K1:%s  K2:%s",
             input_held(BTN_K1) ? "HELD" : "----",
             input_held(BTN_K2) ? "HELD" : "----");
    fb_draw_string(&fb, 10, y, buf,
                   (input_held(BTN_K1) || input_held(BTN_K2)) ? FB_COLOR_YELLOW : FB_COLOR_TEXT_DIM, bg);

    draw_status_bar();
}

/* ---- Scene dispatch -------------------------------------------------- */

static void draw_scene(void)
{
    switch (scene) {
        case 0: draw_splash();    break;
        case 1: draw_colors();    break;
        case 2: draw_shapes();    break;
        case 3: draw_gradients(); break;
        case 4: draw_text();      break;
        case 5: draw_sysinfo();   break;
    }
    present();
}

static void next_scene(void)
{
    scene = (scene + 1) % SCENE_COUNT;
    needs_redraw = true;
    play_nav();
}

static void prev_scene(void)
{
    scene = (scene + SCENE_COUNT - 1) % SCENE_COUNT;
    needs_redraw = true;
    play_nav();
}

/* ---- Main loop ------------------------------------------------------- */

int main(void)
{
    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);

    /* Init peripherals one at a time */
    display_spi_init(&fb);
    input_init();
    buzzer_init();
    joystick_init();
    touch_ok = touch_init();

    /* Draw first scene */
    draw_scene();

    uint32_t tick = 0;
    uint32_t last_sysinfo_update = 0;
    uint32_t joy_cooldown = 0;  /* suppress buttons for N ticks after joystick */

    while (1) {
        input_update();

        /* Read joystick FIRST — if it moved, the physical press-switch
         * on the joystick may false-trigger K2. Suppress button events
         * for a cooldown period after joystick activity. */
        joy_dir_t joy = joystick_read();

        if (joy != JOY_NONE) {
            joy_cooldown = 10;  /* ~160 ms cooldown */
            if (joy == JOY_RIGHT || joy == JOY_DOWN) next_scene();
            if (joy == JOY_LEFT  || joy == JOY_UP)   prev_scene();
        }

        bool buttons_ok = (joy_cooldown == 0);
        if (joy_cooldown > 0) {
            joy_cooldown--;
            /* Drain stale button events during cooldown */
            input_pressed(BTN_K1);
            input_pressed(BTN_K2);
        }

        /* K1 = audio OFF — immediate silence, no confirmation tone */
        if (buttons_ok && input_pressed(BTN_K1)) {
            audio_on = false;
            buzzer_off();
            needs_redraw = true;
        }

        /* K2 = audio ON */
        if (buttons_ok && input_pressed(BTN_K2)) {
            if (!audio_on) {
                audio_on = true;
                play_toggle_on();
                needs_redraw = true;
            }
        }

        /* Touch tap = next scene */
        touch_read();  /* update tap state */
        if (touch_tapped()) {
            /* In color scene, cycle colors instead of changing scene */
            if (scene == 1) {
                color_idx = (color_idx + 1) % PAL_LEN;
                needs_redraw = true;
                play_nav();
            } else {
                next_scene();
            }
        }

        /* Redraw if needed */
        if (needs_redraw) {
            draw_scene();
            needs_redraw = false;
        }

        /* Live update for sysinfo scene (~4 fps) */
        if (scene == 5 && (tick - last_sysinfo_update) > 15) {
            sysinfo_update_live();
            present();
            last_sysinfo_update = tick;
        }

        /* LED heartbeat */
        gpio_put(PIN_LED, (tick / 30) & 1);

        tick++;
        sleep_ms(16);  /* ~60 Hz poll rate */
    }
}
