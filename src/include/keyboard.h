/*
 * KEYBOARD.H - Klavye Giriş Fonksiyonları
 */
#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"

char read_key(void);
char try_read_key(void);
void kbd_delay(u32 count);
void keyboard_init(void *st);

extern u8 kbd_shift;

#endif
