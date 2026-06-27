/*
 * IO.C - UEFI modunda port I/O devre dışı
 */
#include "../include/io.h"

void outb(u16 port, u8 data)  { (void)port; (void)data; }
void outw(u16 port, u16 data) { (void)port; (void)data; }
void outl(u16 port, u32 data) { (void)port; (void)data; }
u8  inb(u16 port)  { (void)port; return 0; }
u16 inw(u16 port)  { (void)port; return 0; }
u32 inl(u16 port)  { (void)port; return 0; }
void io_wait(void) {}
void putchar(char c) { (void)c; }
void puts(const char* s) { (void)s; }
void printf(const char* fmt, ...) { (void)fmt; }
