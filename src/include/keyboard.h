/*
 * KEYBOARD.H - Klavye Giriş Fonksiyonları
 */
#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"

/* Okuma fonksiyonları */
char read_key(void);        /* Bloklar, karakter döndürür */
char try_read_key(void);    /* Bloklamaz, 0 döndürür eğer tuş yoksa */
void kbd_delay(u32 count);  /* Basit busy-loop gecikme */

/* Shift durumu (extern erişim için) */
extern u8 kbd_shift;

#endif /* _KEYBOARD_H */
