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

/* ─── 32-bit renk yardımcıları (premium UI) ──────────────────── */
void video_fill_rect32(int x, int y, int w, int h, u32 color32) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if ((u32)x >= gop_width || (u32)y >= gop_height) return;
    if ((u32)(x + w) > gop_width)  w = (int)gop_width - x;
    if ((u32)(y + h) > gop_height) h = (int)gop_height - y;
    for (int dy = 0; dy < h; dy++) {
        u32 *row = g_fb + (u32)(y + dy) * g_pitch + (u32)x;
        for (int dx = 0; dx < w; dx++) row[dx] = color32;
    }
}

void video_clear_rect32(int x, int y, int w, int h, u32 color32) {
    video_fill_rect32(x, y, w, h, color32);
}

void video_draw_rect32(int x, int y, int w, int h, u32 color32) {
    video_fill_rect32(x, y, w, 1, color32);
    video_fill_rect32(x, y + h - 1, w, 1, color32);
    video_fill_rect32(x, y, 1, h, color32);
    video_fill_rect32(x + w - 1, y, 1, h, color32);
}

void video_draw_char32(char c, int x, int y, u32 color32) {
    if (!font) return;

    u8 *glyph = (u8*)font_psf + font->headersize +
                (unsigned char)c * font->charsize;
    int bpr = ((int)font->width + 7) / 8;  /* bytes per row */

    for (int row = 0; row < (int)font->height; row++) {
        for (int col = 0; col < (int)font->width; col++) {
            int bi = row * bpr + col / 8;
            if ((u32)bi < font->charsize &&
                glyph[bi] & (1 << (7 - col % 8))) {
                video_put_pixel(x + col, y + row, color32);
            }
        }
    }
}

void video_print32(const char *str, int x, int y, u32 color32) {
    int cx = x;
    while (*str) {
        if (*str == '\n') {
            y += font_height;
            cx = x;
        } else {
            video_draw_char32(*str, cx, y, color32);
            cx += font_width;
        }
        str++;
        if (cx > (int)gop_width - font_width) {
            cx = x;
            y += font_height;
        }
    }
}

void video_draw_char_scaled(char c, int x, int y, int scale, u32 color32) {
    if (!font || scale < 1) return;

    u8 *glyph = (u8*)font_psf + font->headersize +
                (unsigned char)c * font->charsize;
    int bpr = ((int)font->width + 7) / 8;

    for (int row = 0; row < (int)font->height; row++) {
        for (int col = 0; col < (int)font->width; col++) {
            int bi = row * bpr + col / 8;
            if ((u32)bi < font->charsize &&
                glyph[bi] & (1 << (7 - col % 8))) {
                video_fill_rect32(x + col * scale, y + row * scale,
                                  scale, scale, color32);
            }
        }
    }
}

void video_print_scaled(const char *str, int x, int y, int scale, u32 color32) {
    int cx = x;
    while (*str) {
        video_draw_char_scaled(*str, cx, y, scale, color32);
        cx += font_width * scale;
        str++;
    }
}

void video_fill_gradient_v(int x, int y, int w, int h, u32 top, u32 bottom) {
    if (h <= 1) {
        video_fill_rect32(x, y, w, h, top);
        return;
    }
    for (int dy = 0; dy < h; dy++) {
        u32 c = video_blend(top, bottom, dy, h - 1);
        video_fill_rect32(x, y + dy, w, 1, c);
    }
}

/* c1 -> c2 arasında t/tmax oranında yumuşak renk geçişi */
u32 video_blend(u32 c1, u32 c2, int t, int tmax) {
    if (tmax <= 0) return c1;
    if (t <= 0) return c1;
    if (t >= tmax) return c2;

    u32 r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    u32 r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;

    u32 r = r1 + ((r2 - r1) * t) / tmax;
    u32 g = g1 + ((g2 - g1) * t) / tmax;
    u32 b = b1 + ((b2 - b1) * t) / tmax;

    return (r << 16) | (g << 8) | b;
}

/* ─── Yumuşak köşeli (rounded) dikdörtgenler ─────────────────── */

/* (dx,dy) pikseli r yarıçaplı yuvarlatılmış w×h dikdörtgen içinde mi? */
static int round_inside(int dx, int dy, int w, int h, int r) {
    if (dx < 0 || dy < 0 || dx >= w || dy >= h) return 0;
    if (r < 1 || dx >= r || dx < w - r) {
        if (r < 1 || dy >= r || dy < h - r)
            return 1;               /* çekirdek bölge */
    }
    /* köşe testi */
    int cx, cy;
    if (dx < r) cx = r - dx; else cx = dx - (w - r - 1);
    if (dy < r) cy = r - dy; else cy = dy - (h - r - 1);
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    return (cx * cx + cy * cy) <= (r * r);
}

void video_fill_round_rect32(int x, int y, int w, int h, int r, u32 color32) {
    if (r < 1) { video_fill_rect32(x, y, w, h, color32); return; }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r < 1) r = 1;
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            if (round_inside(dx, dy, w, h, r))
                video_put_pixel(x + dx, y + dy, color32);
        }
    }
}

void video_draw_round_rect32(int x, int y, int w, int h, int r, u32 color32) {
    if (r < 1) { video_draw_rect32(x, y, w, h, color32); return; }
    if (r > w / 2) r = w / 2;
    if (r > h / 2) r = h / 2;
    if (r < 1) r = 1;
    int ir = (r - 1 < 1) ? 1 : r - 1;
    if (w - 2 < 1 || h - 2 < 1) { video_draw_rect32(x, y, w, h, color32); return; }
    for (int dy = 0; dy < h; dy++) {
        for (int dx = 0; dx < w; dx++) {
            int border = round_inside(dx, dy, w, h, r) &&
                        !round_inside(dx - 1, dy - 1, w - 2, h - 2, ir);
            if (border) video_put_pixel(x + dx, y + dy, color32);
        }
    }
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

/* ─── Fare İmleci (Mouse Cursor) ──────────────────────────────── */
static u32 cursor_bg_buffer[16][16];
static int saved_cursor_x = -1;
static int saved_cursor_y = -1;

static const u8 cursor_bitmap[12][12] = {
    {2,0,0,0,0,0,0,0,0,0,0,0},
    {2,2,0,0,0,0,0,0,0,0,0,0},
    {2,1,2,0,0,0,0,0,0,0,0,0},
    {2,1,1,2,0,0,0,0,0,0,0,0},
    {2,1,1,1,2,0,0,0,0,0,0,0},
    {2,1,1,1,1,2,0,0,0,0,0,0},
    {2,1,1,1,1,1,2,0,0,0,0,0},
    {2,1,1,1,1,1,1,2,0,0,0,0},
    {2,1,1,1,2,2,2,2,2,0,0,0},
    {2,1,2,1,2,0,0,0,0,0,0,0},
    {2,2,0,2,1,2,0,0,0,0,0,0},
    {0,0,0,0,2,2,0,0,0,0,0,0}
};

u32 video_get_pixel(int x, int y) {
    if (x < 0 || y < 0 || (u32)x >= gop_width || (u32)y >= gop_height)
        return 0;
    return g_fb[(u32)y * g_pitch + (u32)x];
}

void video_restore_cursor(int cx, int cy) {
    (void)cx; (void)cy;
    if (saved_cursor_x < 0 || saved_cursor_y < 0) return;
    for (int r = 0; r < 12; r++) {
        for (int c = 0; c < 12; c++) {
            int px = saved_cursor_x + c;
            int py = saved_cursor_y + r;
            if (px >= 0 && py >= 0 && (u32)px < gop_width && (u32)py < gop_height) {
                video_put_pixel(px, py, cursor_bg_buffer[r][c]);
            }
        }
    }
    saved_cursor_x = -1;
    saved_cursor_y = -1;
}

void video_draw_cursor(int cx, int cy) {
    video_restore_cursor(saved_cursor_x, saved_cursor_y);
    saved_cursor_x = cx;
    saved_cursor_y = cy;

    /* Arka planı kaydet */
    for (int r = 0; r < 12; r++) {
        for (int c = 0; c < 12; c++) {
            cursor_bg_buffer[r][c] = video_get_pixel(cx + c, cy + r);
        }
    }

    /* İmleci çiz */
    for (int r = 0; r < 12; r++) {
        for (int c = 0; c < 12; c++) {
            u8 type = cursor_bitmap[r][c];
            if (type == 1) {
                video_put_pixel(cx + c, cy + r, 0x00FFFFFF); /* Beyaz */
            } else if (type == 2) {
                video_put_pixel(cx + c, cy + r, 0x00000000); /* Siyah kenarlık */
            }
        }
    }
}