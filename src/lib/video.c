/*
 * VIDEO.C - VESA Linear Framebuffer Video Driver
 */

#include "../include/video.h"
#include "../include/io.h"
#include "../include/types.h"
#include "../include/terminus_font.h"

static u8* video_mem = (u8*)VIDEO_MEMORY;
static psf2_t* font = (psf2_t*)font_psf;
static int screen_pitch = 320;
static u8 red_mask = 0;
static u8 red_position = 16;
static u8 green_mask = 0;
static u8 green_position = 8;
static u8 blue_mask = 0;
static u8 blue_position = 0;

int screen_width = 320;
int screen_height = 200;
int screen_bpp = 8;
int font_width = 8;
int font_height = 8;
int line_height = 8;

static const u32 vga_palette[16] = {
    0x000000, 0x0000aa, 0x00aa00, 0x00aaaa,
    0xaa0000, 0xaa00aa, 0xaa5500, 0xaaaaaa,
    0x555555, 0x5555ff, 0x55ff55, 0x55ffff,
    0xff5555, 0xff55ff, 0xffff55, 0xffffff
};

static u32 color_to_rgb(u8 color) {
    return vga_palette[color & 0x0f];
}

static u32 pack_channel(u8 value, u8 mask_size, u8 position) {
    if (!mask_size) {
        return 0;
    }

    u32 max_value = (1u << mask_size) - 1;
    return ((value * max_value + 127) / 255) << position;
}

static u32 color_to_native(u8 color) {
    u32 rgb = color_to_rgb(color);
    u8 r = (rgb >> 16) & 0xff;
    u8 g = (rgb >> 8) & 0xff;
    u8 b = rgb & 0xff;

    return pack_channel(r, red_mask, red_position) |
           pack_channel(g, green_mask, green_position) |
           pack_channel(b, blue_mask, blue_position);
}

void video_init(vbe_mode_info_t* vbe_info) {
    if (font) {
        font_width = font->width;
        font_height = font->height;
        line_height = font->height;
    }

    if (vbe_info && vbe_info->framebuffer && vbe_info->width && vbe_info->height) {
        video_mem = (u8*)vbe_info->framebuffer;
        screen_width = vbe_info->width;
        screen_height = vbe_info->height;
        screen_pitch = vbe_info->pitch;
        screen_bpp = vbe_info->bpp;
        red_mask = vbe_info->red_mask;
        red_position = vbe_info->red_position;
        green_mask = vbe_info->green_mask;
        green_position = vbe_info->green_position;
        blue_mask = vbe_info->blue_mask;
        blue_position = vbe_info->blue_position;

        if (!red_mask && screen_bpp >= 24) {
            red_mask = 8;
            red_position = 16;
            green_mask = 8;
            green_position = 8;
            blue_mask = 8;
            blue_position = 0;
        } else if (!red_mask && screen_bpp == 16) {
            red_mask = 5;
            red_position = 11;
            green_mask = 6;
            green_position = 5;
            blue_mask = 5;
            blue_position = 0;
        } else if (!red_mask && screen_bpp == 15) {
            red_mask = 5;
            red_position = 10;
            green_mask = 5;
            green_position = 5;
            blue_mask = 5;
            blue_position = 0;
        }
    }

    video_clear(0);
}

void video_clear(u8 color) {
    for (int y = 0; y < screen_height; y++) {
        for (int x = 0; x < screen_width; x++) {
            video_put_pixel(x, y, color);
        }
    }
}

void video_put_pixel(int x, int y, u8 color) {
    if (x < 0 || x >= screen_width || y < 0 || y >= screen_height) {
        return;
    }

    u8* pixel = video_mem + y * screen_pitch + x * ((screen_bpp + 7) / 8);
    u32 native = color_to_native(color);

    if (screen_bpp == 32) {
        *(u32*)pixel = native;
    } else if (screen_bpp == 24) {
        pixel[0] = native & 0xff;
        pixel[1] = (native >> 8) & 0xff;
        pixel[2] = (native >> 16) & 0xff;
    } else if (screen_bpp == 15 || screen_bpp == 16) {
        *(u16*)pixel = native & 0xffff;
    } else {
        video_mem[y * screen_width + x] = color;
    }
}

void video_fill_rect(int x, int y, int width, int height, u8 color) {
    for (int dy = 0; dy < height; dy++) {
        for (int dx = 0; dx < width; dx++) {
            video_put_pixel(x + dx, y + dy, color);
        }
    }
}

void video_draw_rect(int x, int y, int width, int height, u8 color) {
    for (int dx = 0; dx < width; dx++) {
        video_put_pixel(x + dx, y, color);
        video_put_pixel(x + dx, y + height - 1, color);
    }

    for (int dy = 0; dy < height; dy++) {
        video_put_pixel(x, y + dy, color);
        video_put_pixel(x + width - 1, y + dy, color);
    }
}

void video_draw_char(char c, int x, int y, u8 color) {
    if (!font) return;

    u8* glyph = (u8*)font_psf + font->headersize + (unsigned int)c * font->charsize;
    int bytes_per_row = (font->width + 7) / 8;

    for (u32 row = 0; row < font->height; row++) {
        for (u32 col = 0; col < font->width; col++) {
            int byte_index = row * bytes_per_row + (col / 8);
            if ((u32)byte_index < font->charsize && glyph[byte_index] & (1 << (7 - (col % 8)))) {
                video_put_pixel(x + col, y + row, color);
            }
        }
    }
}

void video_clear_rect(int x, int y, int width, int height) {
    video_fill_rect(x, y, width, height, 0);
}

int video_text_width(const char* str) {
    int width = 0;
    while (*str++) width += font_width;
    return width;
}

void video_print(const char* str, int x, int y, u8 color) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            y += LINE_HEIGHT;
            cx = x;
        } else {
            video_draw_char(*str, cx, y, color);
            cx += font_width;
        }
        str++;
        if (cx > screen_width - font_width) {
            cx = x;
            y += LINE_HEIGHT;
        }
    }
}

void video_scroll_rect(int rx, int ry, int rw, int rh) {
    int bytes_per_pixel = (screen_bpp + 7) / 8;
    
    if (screen_bpp == 8) {
        for (int y = ry + LINE_HEIGHT; y < ry + rh; y++) {
            for (int x = rx; x < rx + rw; x++) {
                video_mem[(y - LINE_HEIGHT) * screen_width + x] = video_mem[y * screen_width + x];
            }
        }
    } else {
        for (int y = ry + LINE_HEIGHT; y < ry + rh; y++) {
            u8* dst = video_mem + (y - LINE_HEIGHT) * screen_pitch;
            u8* src = video_mem + y * screen_pitch;
            for (int x = rx * bytes_per_pixel; x < (rx + rw) * bytes_per_pixel; x++) {
                dst[x] = src[x];
            }
        }
    }

    /* Clear last char row */
    for (int y = ry + rh - LINE_HEIGHT; y < ry + rh; y++) {
        for (int x = rx; x < rx + rw; x++) {
            video_put_pixel(x, y, 0);
        }
    }
}

void video_scroll(void) {
    video_scroll_rect(0, 0, screen_width, screen_height);
}
