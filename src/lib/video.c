/*
 * VIDEO.C - UEFI GOP Linear Framebuffer Driver
 */

#include "../include/video.h"
#include "../include/types.h"
#include "../include/terminus_font.h"

/* ─── Frame buffer state ─────────────────────────────────────── */
static u32 *g_fb    = (void*)0;
static u32  g_pitch = 0;   /* pixels per scan line */

u32 gop_width  = 0;
u32 gop_height = 0;

/* ─── Font ───────────────────────────────────────────────────── */
static psf2_t *font = (void*)0;

int font_width  = 8;
int font_height = 16;

/* ─── 16-renk EGA paleti (32-bit BGRX) ──────────────────────── */
static const u32 palette[16] = {
    0x00000000, /* 0  siyah       */
    0x000000AA, /* 1  koyu mavi   */
    0x0000AA00, /* 2  koyu yeşil  */
    0x0000AAAA, /* 3  koyu camgöbeği */
    0x00AA0000, /* 4  koyu kırmızı */
    0x00AA00AA, /* 5  mor         */
    0x00AA5500, /* 6  kahverengi  */
    0x00AAAAAA, /* 7  açık gri    */
    0x00555555, /* 8  koyu gri    */
    0x005555FF, /* 9  mavi        */
    0x0055FF55, /* 10 yeşil       */
    0x0055FFFF, /* 11 camgöbeği   */
    0x00FF5555, /* 12 kırmızı     */
    0x00FF55FF, /* 13 pembe       */
    0x00FFFF55, /* 14 sarı        */
    0x00FFFFFF, /* 15 beyaz       */
};

/* ─── Init ───────────────────────────────────────────────────── */
void video_init(gop_info_t *info) {
    font = (psf2_t*)font_psf;
    g_fb       = info->framebuffer;
    gop_width  = info->width;
    gop_height = info->height;
    g_pitch    = info->pitch;

    if (font) {
        font_width  = (int)font->width;
        font_height = (int)font->height;
    }
}

/* ─── Pixel ──────────────────────────────────────────────────── */
void video_put_pixel(int x, int y, u32 color32) {
    if (x < 0 || y < 0 || (u32)x >= gop_width || (u32)y >= gop_height)
        return;
    g_fb[(u32)y * g_pitch + (u32)x] = color32;
}

static inline void put(int x, int y, u8 color_idx) {
    video_put_pixel(x, y, palette[color_idx & 0xF]);
}

/* ─── Temizleme ──────────────────────────────────────────────── */
void video_clear(u8 color) {
    u32 c = palette[color & 0xF];
    /* Satır satır doldur (daha hızlı) */
    for (u32 y = 0; y < gop_height; y++) {
        u32 *row = g_fb + y * g_pitch;
        u32 x = 0;
        /* 4'lü doldurma */
        for (; x + 3 < gop_width; x += 4) {
            row[x] = c;
            row[x + 1] = c;
            row[x + 2] = c;
            row[x + 3] = c;
        }
        for (; x < gop_width; x++) {
            row[x] = c;
        }
    }
}

void video_clear_rect(int x, int y, int w, int h) {
    video_fill_rect(x, y, w, h, 0);
}

/* ─── Dikdörtgen ─────────────────────────────────────────────── */
void video_fill_rect(int x, int y, int w, int h, u8 color) {
    u32 c = palette[color & 0xF];
    for (int dy = 0; dy < h; dy++) {
        u32 *row = g_fb + (u32)(y + dy) * g_pitch + (u32)x;
        int dx = 0;
        /* 4'lü doldurma */
        for (; dx + 3 < w; dx += 4) {
            row[dx] = c;
            row[dx + 1] = c;
            row[dx + 2] = c;
            row[dx + 3] = c;
        }
        for (; dx < w; dx++) {
            row[dx] = c;
        }
    }
}

void video_draw_rect(int x, int y, int w, int h, u8 color) {
    for (int dx = 0; dx < w; dx++) {
        put(x + dx, y,         color);
        put(x + dx, y + h - 1, color);
    }
    for (int dy = 0; dy < h; dy++) {
        put(x,         y + dy, color);
        put(x + w - 1, y + dy, color);
    }
}

/* ─── Karakter ───────────────────────────────────────────────── */
void video_draw_char(char c, int x, int y, u8 color) {
    if (!font) return;

    u8 *glyph = (u8*)font_psf + font->headersize +
                (unsigned char)c * font->charsize;
    int bpr = ((int)font->width + 7) / 8;  /* bytes per row */

    for (int row = 0; row < (int)font->height; row++) {
        for (int col = 0; col < (int)font->width; col++) {
            int bi = row * bpr + col / 8;
            if ((u32)bi < font->charsize &&
                glyph[bi] & (1 << (7 - col % 8))) {
                put(x + col, y + row, color);
            }
        }
    }
}

/* ─── Metin ──────────────────────────────────────────────────── */
int video_text_width(const char *str) {
    int n = 0;
    while (*str++) n++;
    return n * font_width;
}

void video_print(const char *str, int x, int y, u8 color) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            y += font_height;
            cx = x;
        } else {
            video_draw_char(*str, cx, y, color);
            cx += font_width;
        }
        str++;
        if (cx > (int)gop_width - font_width) {
            cx = x;
            y += font_height;
        }
    }
}

/* ─── Scroll ─────────────────────────────────────────────────── */
void video_scroll_rect(int rx, int ry, int rw, int rh) {
    /* Satırları bir font_height yukarı taşı */
    for (int row = ry + font_height; row < ry + rh; row++) {
        u32 *dst = g_fb + (u32)(row - font_height) * g_pitch + (u32)rx;
        u32 *src = g_fb + (u32)row               * g_pitch + (u32)rx;
        for (int col = 0; col < rw; col++)
            dst[col] = src[col];
    }
    /* Son satır bloğunu temizle */
    for (int row = ry + rh - font_height; row < ry + rh; row++) {
        u32 *dst = g_fb + (u32)row * g_pitch + (u32)rx;
        for (int col = 0; col < rw; col++)
            dst[col] = 0;
    }
}

void video_scroll(void) {
    video_scroll_rect(0, 0, (int)gop_width, (int)gop_height);
}