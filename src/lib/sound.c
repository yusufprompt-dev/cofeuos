#include "../include/sound.h"
#include "../include/io.h"

#define PIT_FREQ     1193182
#define PIT_CHANNEL  0
#define PIT_CMD      0x43
#define PIT_DATA     0x42
#define SPEAKER_PORT 0x61

void sound_init(void) {
    sound_off();
}

void sound_beep(u16 freq_hz, u16 duration_ms) {
    if (freq_hz == 0) return;
    u32 divisor = PIT_FREQ / freq_hz;
    outb(PIT_CMD, 0xB6);
    outb(PIT_DATA, (u8)(divisor & 0xFF));
    outb(PIT_DATA, (u8)((divisor >> 8) & 0xFF));
    u8 tmp = inb(SPEAKER_PORT);
    if (tmp != (tmp | 0x03)) outb(SPEAKER_PORT, tmp | 0x03);
    for (volatile u32 i = 0; i < (u32)duration_ms * 1000; i++) {
        for (volatile int j = 0; j < 10; j++) { __asm__ volatile("nop"); }
    }
}

void sound_note(u16 freq_hz, u16 duration_ms) {
    sound_beep(freq_hz, duration_ms);
}

void sound_off(void) {
    u8 tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp & 0xFC);
}

void sound_melody(const u16 *notes, const u16 *durations, int count) {
    for (int i = 0; i < count; i++) {
        if (notes[i] == 0) {
            sound_off();
        } else {
            sound_beep(notes[i], durations[i]);
        }
        sound_off();
        for (volatile int d = 0; d < 50; d++) { __asm__ volatile("nop"); }
    }
}

void sound_startup(void) {
    static const u16 notes[] = {523, 659, 784, 1047};
    static const u16 durs[]  = {150, 150, 150, 300};
    sound_melody(notes, durs, 4);
}

void sound_shutdown(void) {
    static const u16 notes[] = {784, 659, 523, 262};
    static const u16 durs[]  = {150, 150, 150, 400};
    sound_melody(notes, durs, 4);
}

void sound_error(void) {
    static const u16 notes[] = {200, 150};
    static const u16 durs[]  = {200, 300};
    sound_melody(notes, durs, 2);
}

void sound_notify(void) {
    static const u16 notes[] = {880, 1175};
    static const u16 durs[]  = {100, 150};
    sound_melody(notes, durs, 2);
}
