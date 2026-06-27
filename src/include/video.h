/*
 * VIDEO.H - UEFI GOP Video Fonksiyonları
 */
#ifndef _VIDEO_H
#define _VIDEO_H

#include "types.h"
#include "terminus_font.h"

/* ─── GOP bilgi yapısı (efi_main.c'den kernel'a geçirilir) ─── */
typedef struct {
    u32 *framebuffer;
    u32  width;
    u32  height;
    u32  pitch;       /* pixels per scan line */
} gop_info_t;

/* ─── Global ekran boyutları ──────────────────────────────────── */
extern u32 gop_width;
extern u32 gop_height;

#define SCREEN_WIDTH  ((int)gop_width)
#define SCREEN_HEIGHT ((int)gop_height)

/* ─── Font boyutları ──────────────────────────────────────────── */
extern int font_width;
extern int font_height;

#define CHAR_WIDTH  font_width
#define CHAR_HEIGHT font_height
#define LINE_HEIGHT font_height

/* ─── Fonksiyonlar ────────────────────────────────────────────── */
void video_init(gop_info_t *info);
void video_clear(u8 color);
void video_put_pixel(int x, int y, u32 color32);
void video_fill_rect(int x, int y, int width, int height, u8 color);
void video_draw_rect(int x, int y, int width, int height, u8 color);
void video_draw_char(char c, int x, int y, u8 color);
void video_clear_rect(int x, int y, int width, int height);
int  video_text_width(const char *str);
void video_print(const char *str, int x, int y, u8 color);
void video_scroll(void);
void video_scroll_rect(int x, int y, int w, int h);

#endif