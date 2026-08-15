/*
 * ============================================================================
 * IO.H - Giriş/Çıkış İşlemleri
 * ============================================================================
 */

#ifndef _IO_H
#define _IO_H

#include "types.h"

/* IO başlatma (UEFI SystemTable + RuntimeServices) */
void io_init(void *st, void *rs);

/* UEFI tablo erişimcileri (kalıcı oturum vb. katmanlar için) */
void *io_get_system_table(void);
void *io_get_runtime_services(void);

/* Donanım I/O port işlemleri */
void outb(u16 port, u8 data);
void outw(u16 port, u16 data);
void outl(u16 port, u32 data);

u8 inb(u16 port);
u16 inw(u16 port);
u32 inl(u16 port);

/* Gecikme fonksiyonları */
void io_wait(void);
void uefi_stall_ms(unsigned int ms);

/* 8254 PIT tabanlı bekleme (UEFI BS->Stall'a bağımlı değildir) */
void pit_delay_ms(unsigned int ms);

/* UEFI sistem kontrolleri */
void uefi_reset_system(void);
void uefi_shutdown(void);

/* Basit yazdırma fonksiyonları */
void printf(const char* format, ...);
void putchar(char c);
void puts(const char* s);

/* QEMU debugcon (port 0xE9) hata ayıklama çıktısı */
void dbg_write(const char *s);

#endif /* _IO_H */
