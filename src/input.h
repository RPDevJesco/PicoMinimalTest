/*
 * input.h — Debounced button input
 */
#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>

typedef enum { BTN_K1 = 0, BTN_K2 = 1, BTN_COUNT } button_id_t;

void input_init(void);
void input_update(void);       /* call every ~10 ms */
bool input_pressed(button_id_t btn);   /* edge: just pressed */
bool input_released(button_id_t btn);
bool input_held(button_id_t btn);

#endif
