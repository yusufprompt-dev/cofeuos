/*
 * KEYBOARD.H - Klavye ve Fare Giriş Fonksiyonları
 */
#ifndef _KEYBOARD_H
#define _KEYBOARD_H

#include "types.h"

typedef struct {
    int dx;
    int dy;
    int dz;
    u8 left_btn;
    u8 right_btn;
} mouse_state_t;

typedef struct {
    char key;
    u16 scan_code;
} key_event_t;

char read_key(void);
char try_read_key(void);
key_event_t read_key_event(void);
key_event_t try_read_key_event(void);

void mouse_init(void *st);
int mouse_get_state(mouse_state_t *state);

void kbd_delay(u32 count);
void keyboard_init(void *st);

extern u8 kbd_shift;

#endif

