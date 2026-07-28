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

/* UEFI ile sistem reinisi */
void uefi_reset_system(void) {
    if (g_rs) {
        g_rs->ResetSystem(EfiResetWarm, EFI_SUCCESS, 0, NULL);
    }
    while (1) __asm__ volatile("hlt");
}

/* UEFI ile sistem kapatma */
void uefi_shutdown(void) {
    if (g_rs) {
        g_rs->ResetSystem(EfiResetShutdown, EFI_SUCCESS, 0, NULL);
    }
    while (1) __asm__ volatile("hlt");
}

/* Port I/O - UEFI modunda gerçek port erişimi mümkün değil,
   sadece reboot/halt için UEFI servisleri kullanılır */
void outb(u16 port, u8 data)  { (void)port; (void)data; }
void outw(u16 port, u16 data) { (void)port; (void)data; }
void outl(u16 port, u32 data) { (void)port; (void)data; }
u8  inb(u16 port)  { (void)port; return 0; }
u16 inw(u16 port)  { (void)port; return 0; }
u32 inl(u16 port)  { (void)port; return 0; }

/* Timer bazlı bekleme */
void io_wait(void) {
    if (g_st) {
        g_st->BootServices->Stall(1000); /* 1ms bekle */
    }
}

void putchar(char c) { (void)c; }
void puts(const char* s) { (void)s; }
void printf(const char* fmt, ...) { (void)fmt; }
