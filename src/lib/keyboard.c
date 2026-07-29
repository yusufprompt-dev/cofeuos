/*
 * KEYBOARD.C - UEFI Simple Text Input & Simple Pointer Protokolü
 */

#include "../include/keyboard.h"
#include <efi.h>
#include <efilib.h>
#include <efipoint.h>

u8 kbd_shift = 0;
u8 kbd_ctrl  = 0;

static EFI_SYSTEM_TABLE *g_st = (void*)0;
#define ST ((EFI_SYSTEM_TABLE*)g_st)

static EFI_SIMPLE_POINTER_PROTOCOL *g_pointer = NULL;

void keyboard_init(void *st) {
    g_st = (EFI_SYSTEM_TABLE*)st;
}

void mouse_init(void *st) {
    if (!st) return;
    EFI_SYSTEM_TABLE *sys = (EFI_SYSTEM_TABLE*)st;
    EFI_GUID pointer_guid = EFI_SIMPLE_POINTER_PROTOCOL_GUID;
    EFI_STATUS status = sys->BootServices->LocateProtocol(&pointer_guid, NULL, (void**)&g_pointer);
    if (!EFI_ERROR(status) && g_pointer) {
        g_pointer->Reset(g_pointer, 0);
    }
}

int mouse_get_state(mouse_state_t *state) {
    if (!state) return -1;
    state->dx = 0;
    state->dy = 0;
    state->dz = 0;
    state->left_btn = 0;
    state->right_btn = 0;

    if (g_pointer) {
        EFI_SIMPLE_POINTER_STATE st;
        EFI_STATUS status = g_pointer->GetState(g_pointer, &st);
        if (!EFI_ERROR(status)) {
            int dx = (int)st.RelativeMovementX;
            int dy = (int)st.RelativeMovementY;
            /* Clamp delta to reasonable range */
            if (dx > 30) dx = 30;
            if (dx < -30) dx = -30;
            if (dy > 30) dy = 30;
            if (dy < -30) dy = -30;

            state->dx = dx;
            state->dy = dy;
            state->dz = (int)st.RelativeMovementZ;
            state->left_btn = st.LeftButton ? 1 : 0;
            state->right_btn = st.RightButton ? 1 : 0;
            return 0;
        }
    }

    return -1;
}

key_event_t try_read_key_event(void) {
    key_event_t ev = {0, 0};
    if (!g_st) return ev;

    EFI_INPUT_KEY key;
    EFI_STATUS status = g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key);
    if (EFI_ERROR(status)) return ev;

    ev.scan_code = key.ScanCode;
    if (key.UnicodeChar == 0x000D) ev.key = '\n';
    else if (key.UnicodeChar == 0x0008) ev.key = '\b';
    else if (key.UnicodeChar < 0x80) ev.key = (char)key.UnicodeChar;

    return ev;
}

key_event_t read_key_event(void) {
    key_event_t ev = {0, 0};
    if (!g_st) {
        while (1) __asm__ volatile("hlt");
    }

    EFI_INPUT_KEY key;
    EFI_STATUS status;

    while (1) {
        UINTN index;
        g_st->BootServices->WaitForEvent(1, &g_st->ConIn->WaitForKey, &index);

        status = g_st->ConIn->ReadKeyStroke(g_st->ConIn, &key);
        if (EFI_ERROR(status)) continue;

        ev.scan_code = key.ScanCode;
        if (key.UnicodeChar == 0x000D) ev.key = '\n';
        else if (key.UnicodeChar == 0x0008) ev.key = '\b';
        else if (key.UnicodeChar == 0x0010) ev.key = 16; /* Ctrl+P */
        else if (key.UnicodeChar == 0x0018) ev.key = 24; /* Ctrl+X */
        else if (key.UnicodeChar < 0x80) ev.key = (char)key.UnicodeChar;

        return ev;
    }
}

char read_key(void) {
    key_event_t ev = read_key_event();
    return ev.key;
}

char try_read_key(void) {
    key_event_t ev = try_read_key_event();
    return ev.key;
}

void kbd_delay(u32 count) {
    volatile u32 i;
    for (i = 0; i < count; i++)
        __asm__ volatile("nop");
}