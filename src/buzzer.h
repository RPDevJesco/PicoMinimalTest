/*
 * buzzer.h — PWM buzzer driver
 */
#ifndef BUZZER_H
#define BUZZER_H

#include <stdint.h>

void buzzer_init(void);
void buzzer_tone(uint16_t freq_hz);
void buzzer_off(void);
void buzzer_beep(uint16_t freq_hz, uint16_t duration_ms);
void buzzer_click(void);

#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_REST 0

#endif
