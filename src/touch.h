/*
 * touch.h — Capacitive touch (FT6236 / GT911 auto-detect)
 */
#ifndef TOUCH_H
#define TOUCH_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t x, y;
    bool pressed;
} touch_point_t;

bool touch_init(void);
touch_point_t touch_read(void);

/* Returns true on tap (press then release) — edge-triggered */
bool touch_tapped(void);

#endif
