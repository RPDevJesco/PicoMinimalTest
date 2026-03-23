/*
 * input.c — Debounced button input (K1=GP15, K2=GP14, active-low)
 */
#include "input.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#define DEBOUNCE_COUNT 3

static const uint _pin[BTN_COUNT] = { PIN_BTN_K1, PIN_BTN_K2 };
static uint8_t _count[BTN_COUNT];
static bool _state[BTN_COUNT];
static bool _press_evt[BTN_COUNT];
static bool _release_evt[BTN_COUNT];

void input_init(void)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        gpio_init(_pin[i]);
        gpio_set_dir(_pin[i], GPIO_IN);
        gpio_pull_up(_pin[i]);
        _count[i] = 0;
        _state[i] = false;
        _press_evt[i] = false;
        _release_evt[i] = false;
    }
}

void input_update(void)
{
    for (int i = 0; i < BTN_COUNT; i++) {
        bool raw = !gpio_get(_pin[i]);
        if (raw == _state[i]) {
            _count[i] = 0;
        } else {
            _count[i]++;
            if (_count[i] >= DEBOUNCE_COUNT) {
                bool prev = _state[i];
                _state[i] = raw;
                _count[i] = 0;
                if (_state[i] && !prev) _press_evt[i] = true;
                if (!_state[i] && prev) _release_evt[i] = true;
            }
        }
    }
}

bool input_pressed(button_id_t btn)
{
    if (btn >= BTN_COUNT) return false;
    bool p = _press_evt[btn];
    _press_evt[btn] = false;
    return p;
}

bool input_released(button_id_t btn)
{
    if (btn >= BTN_COUNT) return false;
    bool r = _release_evt[btn];
    _release_evt[btn] = false;
    return r;
}

bool input_held(button_id_t btn)
{
    if (btn >= BTN_COUNT) return false;
    return _state[btn];
}
