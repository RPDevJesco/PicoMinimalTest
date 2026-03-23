/*
 * buzzer.c — PWM buzzer on GP13
 */
#include "buzzer.h"
#include "pins.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"

static uint _slice;
static uint _channel;

void buzzer_init(void)
{
    gpio_set_function(PIN_BUZZER, GPIO_FUNC_PWM);
    _slice   = pwm_gpio_to_slice_num(PIN_BUZZER);
    _channel = pwm_gpio_to_channel(PIN_BUZZER);
    pwm_set_enabled(_slice, false);
}

void buzzer_tone(uint16_t freq_hz)
{
    if (freq_hz == 0) { buzzer_off(); return; }

    uint32_t clock = clock_get_hz(clk_sys);
    uint32_t wrap  = clock / freq_hz;
    uint16_t div   = 1;
    while (wrap > 65535) { div++; wrap = clock / (freq_hz * div); }

    pwm_set_clkdiv_int_frac(_slice, div, 0);
    pwm_set_wrap(_slice, (uint16_t)wrap);
    pwm_set_chan_level(_slice, _channel, (uint16_t)(wrap / 2));
    pwm_set_enabled(_slice, true);
}

void buzzer_off(void)
{
    pwm_set_enabled(_slice, false);
    gpio_set_function(PIN_BUZZER, GPIO_FUNC_PWM);
}

void buzzer_beep(uint16_t freq_hz, uint16_t duration_ms)
{
    buzzer_tone(freq_hz);
    sleep_ms(duration_ms);
    buzzer_off();
}

void buzzer_click(void)
{
    buzzer_beep(4000, 5);
}
