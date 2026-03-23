/*
 * joystick.c — Analog joystick on ADC0 (X) / ADC1 (Y)
 *
 * Auto-calibrates center on init. Threshold ±700 from center.
 * Edge-triggered: returns direction once on deflection,
 * then JOY_NONE until returned to center.
 */
#include "joystick.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define THRESHOLD   700

static uint16_t _center_x = 2048;
static uint16_t _center_y = 2048;
static joy_dir_t _prev = JOY_NONE;

void joystick_init(void)
{
    adc_init();
    adc_gpio_init(PIN_JOY_X);
    adc_gpio_init(PIN_JOY_Y);

    /* Auto-calibrate: read center at rest */
    sleep_ms(10);
    _center_x = joystick_x_raw();
    _center_y = joystick_y_raw();
}

uint16_t joystick_x_raw(void)
{
    adc_select_input(0);
    return adc_read();
}

uint16_t joystick_y_raw(void)
{
    adc_select_input(1);
    return adc_read();
}

joy_dir_t joystick_read(void)
{
    uint16_t x = joystick_x_raw();
    uint16_t y = joystick_y_raw();

    joy_dir_t dir = JOY_NONE;

    int16_t dx = (int16_t)x - (int16_t)_center_x;
    int16_t dy = (int16_t)y - (int16_t)_center_y;

    /* X axis: left/right */
    if (dx < -THRESHOLD)      dir = JOY_LEFT;
    else if (dx > THRESHOLD)  dir = JOY_RIGHT;
    /* Y axis: up/down (only if X is centered) */
    else if (dy < -THRESHOLD) dir = JOY_UP;
    else if (dy > THRESHOLD)  dir = JOY_DOWN;

    /* Edge detection: only fire once per deflection */
    if (dir == _prev) return JOY_NONE;  /* still held or still centered */
    _prev = dir;

    return dir;  /* JOY_NONE when returned to center, direction on first deflect */
}
