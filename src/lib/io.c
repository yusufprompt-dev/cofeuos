/*
 * IO.C - UEFI Runtime Services ile giriş/çıkış işlemleri
 */
#include "../include/io.h"
#include <efi.h>

static EFI_RUNTIME_SERVICES *g_rs = NULL;
static EFI_SYSTEM_TABLE *g_st = NULL;

void io_init(void *st, void *rs) {
    g_st = (EFI_SYSTEM_TABLE*)st;
    g_rs = (EFI_RUNTIME_SERVICES*)rs;
}

void *io_get_system_table(void) {
    return (void*)g_st;
}

void *io_get_runtime_services(void) {
    return (void*)g_rs;
}

/* UEFI ile sistem reinisi */
void uefi_reset_system(void) {
    dbg_write("[KRN] uefi_reset_system\n");
    if (g_rs) {
        g_rs->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);
    }
    while (1) __asm__ volatile("hlt");
}

/* UEFI ile sistem kapatma */
void uefi_shutdown(void) {
    dbg_write("[KRN] uefi_shutdown\n");
    if (g_rs) {
        g_rs->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
    }
    while (1) __asm__ volatile("hlt");
}

/* QEMU debugcon (port 0xE9) hata ayıklama çıktısı */
void dbg_write(const char *s) {
    while (*s) {
        __asm__ volatile("outb %0, %1" :: "a"((unsigned char)*s), "Nd"((unsigned short)0xe9));
        s++;
    }
}

/* Port I/O - freestanding modda gerçek port erişimi (PS/2 için) */
void outb(u16 port, u8 data) {
    __asm__ volatile("outb %0, %w1" : : "a"(data), "Nd"(port));
}
void outw(u16 port, u16 data) {
    __asm__ volatile("outw %0, %w1" : : "a"(data), "Nd"(port));
}
void outl(u16 port, u32 data) {
    __asm__ volatile("outl %0, %w1" : : "a"(data), "Nd"(port));
}
u8 inb(u16 port) {
    u8 v;
    __asm__ volatile("inb %w1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
u16 inw(u16 port) {
    u16 v;
    __asm__ volatile("inw %w1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
u32 inl(u16 port) {
    u32 v;
    __asm__ volatile("inl %w1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

/* KVM/TCG'de PIT sayacı tutarsız çalışabilir ve OVMF BS->Stall (TSC tabanlı)
   bu yüzden asılabilir. Güvenilir gecikme: ham CMOS RTC (gerçek zaman) ile
   İLK çağrıda kalibre edilen PAUSE döngüsü. */
static unsigned char cmos_read(unsigned char reg) {
    outb(0x70, reg);
    return inb(0x71);
}

/* UIP (güncelleme sürüyor) bayrağını yoklayıp saniyeyi oku */
static unsigned char cmos_second(void) {
    unsigned char uip = 0;
    for (int i = 0; i < 1000; i++) {
        uip = cmos_read(0x0a);
        if (!(uip & 0x80)) break;
    }
    return cmos_read(0x00);
}

void pit_delay_ms(unsigned int ms) {
    static unsigned long nops_per_ms = 0;

    if (nops_per_ms == 0) {
        unsigned char s = cmos_second();
        unsigned long guard = 0;
        /* kısmi saniyeyi atla: saniye atlamasını bekle */
        while (cmos_second() == s && guard++ < 200000000UL)
            __asm__ volatile("pause");
        unsigned char s1 = cmos_second();
        unsigned long n = 0;
        /* tam bir saniyelik sürede kaç PAUSE sığdığını ölç */
        while (n < 4000000000UL) {
            if (cmos_second() != s1) break;
            for (volatile int i = 0; i < 100000; i++) __asm__ volatile("pause");
            n += 100000;
        }
        if (n < 1000000UL) n = 1000000UL;
        nops_per_ms = n / 1000ul;
    }

    volatile unsigned long total = (unsigned long)ms * nops_per_ms;
    while (total--) __asm__ volatile("pause");
}

/* Timer bazlı bekleme */
void io_wait(void) {
    if (g_st) {
        g_st->BootServices->Stall(1000); /* 1ms bekle */
    }
}

/* Milisaniye cinsinden UEFI Stall beklemesi (arka plan işleri için) */
void uefi_stall_ms(unsigned int ms) {
    if (g_st && ms > 0) {
        g_st->BootServices->Stall(ms * 1000);
    }
}

void putchar(char c) { (void)c; }
void puts(const char* s) { (void)s; }
void printf(const char* fmt, ...) { (void)fmt; }
