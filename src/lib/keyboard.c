/*
 * KEYBOARD.C - UEFI Simple Text Input Protokolü ile Klavye
 */

#include "../include/keyboard.h"
#include <efi.h>
#include <efilib.h>

u8 kbd_shift = 0;
u8 kbd_ctrl  = 0;

/* UEFI SystemTable pointer — efi_main'den set edilir */
static EFI_SYSTEM_TABLE *g_st = (void*)0;
#define ST ((EFI_SYSTEM_TABLE*)g_st)

void keyboard_init(void *st) {
    g_st = (EFI_SYSTEM_TABLE*)st;
}

char read_key(void) {
    if (!g_st) {
        /* Fallback: sonsuz döngü */
        while (1) __asm__ volatile("hlt");
    }

    EFI_INPUT_KEY key;
    EFI_STATUS status;

    /* Tuş basılana kadar bekle */
    while (1) {
        /* Wait for key event */
        UINTN index;
        g_st->BootServices->WaitForEvent(
            1,
            &g_st->ConIn->WaitForKey,
            &index
        );

        status = g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key);
        if (EFI_ERROR(status)) continue;

        /* Özel tuşlar */
        if (key.UnicodeChar == 0) {
            /* Scan code — ok tuşları vs */
            continue;
        }

        /* Enter */
        if (key.UnicodeChar == 0x000D) return '\n';

        /* Backspace */
        if (key.UnicodeChar == 0x0008) return '\b';

        /* Ctrl+P (DLE = 0x10) */
        if (key.UnicodeChar == 0x0010) return 16;

        /* Ctrl+X (CAN = 0x18) */
        if (key.UnicodeChar == 0x0018) return 24;

        /* Normal ASCII */
        if (key.UnicodeChar < 0x80) {
            return (char)key.UnicodeChar;
        }
    }
}

char try_read_key(void) {
    if (!g_st) return 0;

    EFI_INPUT_KEY key;
    EFI_STATUS status = g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key);
    if (EFI_ERROR(status)) return 0;

    if (key.UnicodeChar == 0x000D) return '\n';
    if (key.UnicodeChar == 0x0008) return '\b';
    if (key.UnicodeChar < 0x80) return (char)key.UnicodeChar;
    return 0;
}

void kbd_delay(u32 count) {
    volatile u32 i;
    for (i = 0; i < count; i++)
        __asm__ volatile("nop");
}