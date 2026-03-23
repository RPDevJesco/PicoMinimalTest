/*
 * joystick.h — Analog joystick via ADC
 */
#ifndef JOYSTICK_H
#define JOYSTICK_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    JOY_NONE  = 0,
    JOY_LEFT  = 1,
    JOY_RIGHT = 2,
    JOY_UP    = 3,
    JOY_DOWN  = 4,
} joy_dir_t;

void joystick_init(void);

/* Raw ADC values (0–4095) */
uint16_t joystick_x_raw(void);
uint16_t joystick_y_raw(void);

/* Thresholded direction with edge detection.
 * Call every ~10-20 ms. Returns direction on first trigger,
 * JOY_NONE while held (prevents rapid repeat). */
joy_dir_t joystick_read(void);

#endif
