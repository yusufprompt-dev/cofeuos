/*
 * VIDEO.H - VESA/VGA Video Fonksiyonları
 */
#ifndef _VIDEO_H
#define _VIDEO_H

#include "types.h"
#include "terminus_font.h"

#define VIDEO_MEMORY 0xA0000

typedef struct __attribute__((packed)) {
    u16 attributes;
    u8 window_a;
    u8 window_b;
    u16 granularity;
    u16 window_size;
    u16 segment_a;
    u16 segment_b;
    u32 win_func_ptr;
    u16 pitch;
    u16 width;
    u16 height;
    u8 w_char;
    u8 y_char;
    u8 planes;
    u8 bpp;
    u8 banks;
    u8 memory_model;
    u8 bank_size;
    u8 image_pages;
    u8 reserved0;
    u8 red_mask;
    u8 red_position;
    u8 green_mask;
    u8 green_position;
    u8 blue_mask;
    u8 blue_position;
    u8 reserved_mask;
    u8 reserved_position;
    u8 direct_color_attributes;
    u32 framebuffer;
    u32 off_screen_mem_off;
    u16 off_screen_mem_size;
    u8 reserved1[206];
} vbe_mode_info_t;

extern int screen_width;
extern int screen_height;
extern int screen_bpp;

extern int font_width;
extern int font_height;
extern int line_height;

#define SCREEN_WIDTH screen_width
#define SCREEN_HEIGHT screen_height
#define CHAR_WIDTH font_width
#define CHAR_HEIGHT font_height
#define LINE_HEIGHT line_height

void video_init(vbe_mode_info_t* vbe_info);
void video_clear(u8 color);
void video_put_pixel(int x, int y, u8 color);
void video_fill_rect(int x, int y, int width, int height, u8 color);
void video_draw_rect(int x, int y, int width, int height, u8 color);
void video_draw_char(char c, int x, int y, u8 color);
void video_clear_rect(int x, int y, int width, int height);
int video_text_width(const char* str);
void video_print(const char* str, int x, int y, u8 color);
void video_scroll(void);
void video_scroll_rect(int x, int y, int w, int h);

#endif
