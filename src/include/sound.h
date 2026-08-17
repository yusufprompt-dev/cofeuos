#ifndef SOUND_H
#define SOUND_H

#include "types.h"

void sound_init(void);
void sound_beep(u16 freq_hz, u16 duration_ms);
void sound_note(u16 freq_hz, u16 duration_ms);
void sound_melody(const u16 *notes, const u16 *durations, int count);
void sound_off(void);

/* Preset melodies */
void sound_startup(void);
void sound_shutdown(void);
void sound_error(void);
void sound_notify(void);

#endif
