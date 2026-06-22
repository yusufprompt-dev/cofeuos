/*
 * KEYBOARD.C - Klavye Giriş İşlemleri
 */

#include "../include/keyboard.h"
#include "../include/io.h"

u8 kbd_shift = 0;
u8 kbd_ctrl = 0;

static const char key_normal[59] = {
    0, 27, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    9, 'q','w','e','r','t','y','u','i','o','p','[',']',10,
    0,'a','s','d','f','g','h','j','k','l',';','\'', '`', 0,
    '\\','z','x','c','v','b','n','m',',','.','/', 0,42,0,' '
};

static const char key_shift[59] = {
    0, 27, '!','@','#','$','%','^','&','*','(',')','_','+','\b',
    9, 'Q','W','E','R','T','Y','U','I','O','P','{','}',10,
    0,'A','S','D','F','G','H','J','K','L',':','"','~', 0,
    '|','Z','X','C','V','B','N','M','<','>','?', 0,42,0,' '
};

/* Bloklamayan klavye okuma — 0 döndürür eğer tuş yoksa */
char try_read_key(void) {
    if (!(inb(0x64) & 1)) return 0;
    u8 sc = inb(0x60);
    if (sc == 0x2A || sc == 0x36) { kbd_shift = 1; return 0; }
    if (sc == 0xAA || sc == 0xB6) { kbd_shift = 0; return 0; }
    if (sc == 0x1D) { kbd_ctrl = 1; return 0; }
    if (sc == 0x9D) { kbd_ctrl = 0; return 0; }
    if (sc == 0x0E) return '\b';
    if (sc < 59 && !(sc & 0x80)) {
        char c = kbd_shift ? key_shift[sc] : key_normal[sc];
        if (kbd_ctrl) {
            if (c >= 'a' && c <= 'z') return c - 'a' + 1;
            if (c >= 'A' && c <= 'Z') return c - 'A' + 1;
        }
        return c;
    }
    return 0;
}

/* Bloklamalı klavye okuma */
char read_key(void) {
    while (1) {
        char c = try_read_key();
        if (c) return c;
    }
}

/* Basit busy-loop gecikme (yaklaşık) */
void kbd_delay(u32 count) {
    volatile u32 i;
    for (i = 0; i < count; i++) {
        __asm__ volatile("nop");
    }
}
