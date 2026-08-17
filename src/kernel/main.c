/*
 * MAIN.C - cofeuOS Kernel Entry Point
 * GOP (UEFI) versiyonu
 */

#include "../include/types.h"
#include "../include/io.h"
#include "../include/memory.h"
#include "../include/fs.h"
#include "../include/shell.h"
#include "../include/video.h"
#include "../include/string.h"
#include "../include/keyboard.h"
#include "../include/session.h"
#include "../include/network.h"
#include "../include/time.h"
#include <efi.h>

shell_control    g_shell;
fs_control_block g_fs;
memory_arena     g_mem_arena;

/* ─── İmleç yardımcıları ─────────────────────────────────────── */

static void next_line(void) {
    cursor_x = splits[active_split].x + 5;
    cursor_y += font_height;
    if (cursor_y > splits[active_split].y + splits[active_split].h - font_height) {
        video_scroll_rect(
            splits[active_split].x,
            splits[active_split].y,
            splits[active_split].w,
            splits[active_split].h
        );
        cursor_y = splits[active_split].y + splits[active_split].h - font_height;
    }
}

static void erase_at(int x, int y) {
    if (x >= splits[active_split].x && y >= splits[active_split].y) {
        video_clear_rect(x, y, font_width, font_height);
    }
}

/* ─── Port 0xE9 debug (QEMU -debugcon stdio) ────────────────── */
static void dbg_putc(char c) {
    __asm__ volatile("outb %0, %1" :: "a"((unsigned char)c), "Nd"((unsigned short)0xe9));
}
static void dbg_print(const char *s) {
    while (*s) dbg_putc(*s++);
}

/* ─── Premium Login (UI/UX) ──────────────────────────────────── */

/* Lüks karanlık mod paleti (0x00RRGGBB) */
#define LOGIN_BG_TOP      0x00070B14u
#define LOGIN_BG_BOTTOM   0x00111A2Eu
#define LOGIN_CARD_BG     0x000C1320u
#define LOGIN_CARD_EDGE   0x001D2B43u
#define LOGIN_BORDER      0x00203450u
#define LOGIN_BORDER_HI   0x0038537Bu
#define LOGIN_ACCENT      0x0042E0C2u
#define LOGIN_ACCENT_LT   0x0078F4D8u
#define LOGIN_ACCENT_DIM  0x00299982u
#define LOGIN_FIELD_BG    0x000E1626u
#define LOGIN_FIELD_BG_F  0x00101E33u
#define LOGIN_TEXT        0x00E8EFF9u
#define LOGIN_MUTED       0x00899CBBu
#define LOGIN_LABEL       0x00A9BAD4u
#define LOGIN_ERROR       0x00FF5A6Au
#define LOGIN_OK          0x0038D98Bu
#define LOGIN_BTN_TEXT    0x0006131Cu

#define LOGIN_FIELD_H     36
#define LOGIN_FIELD_PAD   14

typedef struct {
    int cx, cy, cw, ch;      /* kart */
    int field_x, field_w;    /* giriş alanı */
    int user_y, pass_y;      /* giriş alanı üst kenarı */
    int btn_y;               /* oturum aç butonu üst kenarı */
} login_layout_t;

static login_layout_t g_ll;

/* Kayıtlı oturum bilgisi (UI ipucu + otomatik doldurma için) */
static int  g_has_session = 0;
static char g_session_user[SESSION_USER_MAX] = "";

/* Odakta yumuşak nabız gibi atan aksan kenar rengi */
static u32 login_focus_border(int frame) {
    int phase = frame % 40;
    int t = phase < 20 ? phase : 40 - phase;
    return video_blend(LOGIN_ACCENT_DIM, LOGIN_ACCENT_LT, t, 20);
}

/* Tek bir giriş alanını çizer (animasyon karelerinde yeniden çizim için) */
static void login_draw_field(int frame, int focus, int field_no,
                             const char *text, int len, int masked) {
    int fy = (field_no == 0) ? g_ll.user_y : g_ll.pass_y;
    int fx = g_ll.field_x;
    int fw = g_ll.field_w;
    int focused = (focus == field_no);

    /* alan arka planı */
    video_fill_rect32(fx, fy, fw, LOGIN_FIELD_H,
                      focused ? LOGIN_FIELD_BG_F : LOGIN_FIELD_BG);

    /* odakta yumuşak dış parlama (glow) */
    if (focused) {
        u32 glow = video_blend(LOGIN_CARD_BG, login_focus_border(frame), 22, 100);
        video_draw_rect32(fx - 2, fy - 2, fw + 4, LOGIN_FIELD_H + 4, glow);
    }
    video_draw_rect32(fx, fy, fw, LOGIN_FIELD_H,
                      focused ? login_focus_border(frame) : LOGIN_BORDER);

    /* metin (parola alanı maskelenir) */
    int tx = fx + LOGIN_FIELD_PAD;
    int ty = fy + ((int)LOGIN_FIELD_H - font_height) / 2;
    for (int i = 0; i < len; i++) {
        if (tx > fx + fw - LOGIN_FIELD_PAD - font_width) break;
        video_draw_char32(masked ? '*' : text[i], tx, ty, LOGIN_TEXT);
        tx += font_width;
    }

    /* yanıp sönen caret */
    if (focused && (frame % 10) < 6)
        video_fill_rect32(tx, fy + 8, 2, LOGIN_FIELD_H - 16, LOGIN_ACCENT_LT);
}

/* Login ekranının tamamını çizer */
static void login_draw(int frame, int focus, const char *user, int ulen,
                       const char *pass, int plen, const char *err) {
    int W = (int)gop_width;
    int H = (int)gop_height;

    /* arka plan: dikey gradyan + üst kenarda soft aksan çizgisi */
    video_fill_gradient_v(0, 0, W, H, LOGIN_BG_TOP, LOGIN_BG_BOTTOM);
    video_fill_rect32(0, 0, W, 2, video_blend(LOGIN_BG_TOP, LOGIN_ACCENT, 35, 100));
    video_fill_rect32(0, H - 1, W, 1, video_blend(LOGIN_BG_BOTTOM, LOGIN_BORDER_HI, 40, 100));

    /* logo ölçeği ekran boyutuna göre */
    int scale = (W >= 1600) ? 3 : 2;
    int logo_w = 7 * font_width * scale;
    int logo_h = font_height * scale;
    int logo_y = 36;

    /* kart geometrisi (içerikten hesaplanır, dikeyde ortalanır) */
    int cw = 460;
    int subtitle_y = logo_y + logo_h + 10;
    int div_y      = subtitle_y + 28;
    int status_y   = div_y + 12;
    int ulabel_y   = status_y + 26;
    int ufield_y   = ulabel_y + 22;
    int plabel_y   = ufield_y + LOGIN_FIELD_H + 22;
    int pfield_y   = plabel_y + 22;
    int err_y      = pfield_y + LOGIN_FIELD_H + 14;
    int btn_y      = err_y + 36;
    int ch         = btn_y + 56;

    int cx = (W - cw) / 2;
    int cy = (H - ch) / 2 - 6;

    g_ll.cx = cx; g_ll.cy = cy; g_ll.cw = cw; g_ll.ch = ch;
    g_ll.field_x = cx + 28;
    g_ll.field_w = cw - 56;
    g_ll.user_y  = ufield_y;
    g_ll.pass_y  = pfield_y;
    g_ll.btn_y   = btn_y;

    /* kart gövdesi + çerçeve */
    video_fill_rect32(cx, cy, cw, ch, LOGIN_CARD_BG);
    video_draw_rect32(cx, cy, cw, ch, LOGIN_BORDER);
    for (int x = 0; x < cw; x++)
        video_fill_rect32(cx + x, cy + ch - 1, 1, 1,
                          video_blend(LOGIN_BORDER, LOGIN_ACCENT, x * 100 / cw, 100));
    for (int x = 0; x < cw; x++)
        video_fill_rect32(cx + x, cy, 1, 1,
                          video_blend(LOGIN_CARD_EDGE, LOGIN_ACCENT_DIM, x * 100 / cw, 100));

    /* logo arkasında konsantrik soft parlama (aura) */
    {
        int lx = cx + (cw - logo_w) / 2;
        int au_w = logo_w + 48;
        for (int i = 0; i < 6; i++) {
            u32 c = video_blend(LOGIN_CARD_BG, LOGIN_ACCENT, 14 + i * 4, 100);
            video_draw_rect32(lx - 24 + i * 4, logo_y - 8 + i * 2,
                              au_w - i * 8, logo_h + 16 - i * 4, c);
        }
        /* marka: soldan sağa aksan → parlak camgöbeği geçişli */
        for (int i = 0; i < 7; i++)
            video_draw_char_scaled("cofeuOS"[i],
                                   lx + i * font_width * scale, logo_y, scale,
                                   video_blend(LOGIN_ACCENT, LOGIN_ACCENT_LT, i * 17, 100));
    }

    /* alt başlık */
    {
        const char *sub = "Secure Session Login";
        video_print32(sub, cx + (cw - video_text_width(sub)) / 2,
                      subtitle_y, LOGIN_MUTED);
    }

    /* ayraç: yatay gradyan ince çizgi */
    for (int x = 24; x < cw - 24; x++)
        video_fill_rect32(cx + x, div_y, 1, 1,
                          video_blend(LOGIN_BORDER_HI, LOGIN_ACCENT,
                                      (x - 24) * 100 / (cw - 48), 100));

    /* oturum durumu ipucu */
    if (g_has_session) {
        const char *s1 = "Kayitli oturum: ";
        int w1 = video_text_width(s1);
        int w2 = video_text_width(g_session_user);
        int x0 = cx + (cw - (w1 + w2)) / 2;
        video_print32(s1, x0, status_y, LOGIN_MUTED);
        video_print32(g_session_user, x0 + w1, status_y, LOGIN_ACCENT);
    } else {
        const char *s2 = "Yeni oturum — bilgileriniz guvenle saklanir";
        video_print32(s2, cx + (cw - video_text_width(s2)) / 2, status_y, LOGIN_MUTED);
    }

    /* etiketler */
    video_print32("KULLANICI ADI", g_ll.field_x, ulabel_y,
                  focus == 0 ? LOGIN_ACCENT : LOGIN_LABEL);
    video_print32("PAROLA", g_ll.field_x, plabel_y,
                  focus == 1 ? LOGIN_ACCENT : LOGIN_LABEL);

    /* alanlar */
    login_draw_field(frame, focus, 0, user, ulen, 0);
    login_draw_field(frame, focus, 1, pass, plen, 1);

    /* hata mesajı */
    if (err[0] != '\0')
        video_print32(err, g_ll.field_x, err_y, LOGIN_ERROR);

    /* oturum aç butonu */
    {
        int bw = cw - 56;
        int bh = 40;
        video_fill_gradient_v(cx + 28, btn_y, bw, bh, LOGIN_ACCENT, LOGIN_ACCENT_DIM);
        video_draw_rect32(cx + 28, btn_y, bw, bh,
                          video_blend(LOGIN_ACCENT_DIM, LOGIN_ACCENT_LT, 40, 100));
        if (focus == 1)
            video_draw_rect32(cx + 26, btn_y - 2, bw + 4, bh + 4,
                              video_blend(LOGIN_CARD_BG, login_focus_border(frame), 28, 100));
        const char *bt = "Oturum Ac   [Enter]";
        video_print32(bt, cx + 28 + (bw - video_text_width(bt)) / 2,
                      btn_y + (bh - font_height) / 2, LOGIN_BTN_TEXT);
    }

    /* alt bilgi */
    {
        const char *ft = "cofeuOS v3.0  |  Guvenli Oturum Depolama";
        video_print32(ft, cx + (cw - video_text_width(ft)) / 2, ch - 24, LOGIN_MUTED);
    }
}

/* Tuş bekleme döngüsü: bekleme sırasında odak parlaması + caret animasyonu */
static char login_wait_key(int *frame, int focus, const char *user, int ulen,
                           const char *pass, int plen) {
    while (1) {
        key_event_t ev = try_read_key_event();
        if (ev.key || ev.scan_code)
            return (ev.scan_code == 0x01 || ev.scan_code == 0x03)
                       ? (char)ev.scan_code : ev.key;

        network_monitor_tick();
        (*frame)++;
        if ((*frame) % 2 == 0) {
            login_draw_field(*frame, focus, 0, user, ulen, 0);
            login_draw_field(*frame, focus, 1, pass, plen, 1);
        }
        for (volatile int i = 0; i < 200000; i++) __asm__ volatile("nop");
    }
}

/* Başarılı girişte buton üzerinde kayan ışık süpürmesi */
static void login_success_anim(void) {
    for (int i = 8; i <= g_ll.cw; i += 8) {
        video_fill_rect32(g_ll.cx, g_ll.btn_y, i, 40, LOGIN_ACCENT_LT);
        for (volatile int j = 0; j < 600000; j++) __asm__ volatile("nop");
    }
    for (volatile int j = 0; j < 1500000; j++) __asm__ volatile("nop");
    video_clear(0);
}

/* ─── Kullanici Girisi (Kalıcı Hafızalı) ──────────────────────── */
static void show_login(void) {
    char username[SESSION_USER_MAX];
    char password[SESSION_PASS_MAX];
    int ulen = 0, plen = 0;

    /* Uygulama başlarken ilk iş: OS üzerindeki kalıcı yapılandırmayı oku,
       giriş alanlarını otomatik doldur (autofill). */
    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));

    {
        login_session sess;
        if (session_load(&sess) > 0 && sess.valid) {
            g_has_session = 1;
            strncpy(username, sess.username, sizeof(username) - 1);
            strncpy(password, sess.password, sizeof(password) - 1);
            strncpy(g_session_user, sess.username, sizeof(g_session_user) - 1);
            ulen = (int)strlen(username);
            plen = (int)strlen(password);
            dbg_print("[KRN] kayitli oturum yuklendi (autofill)\n");
        } else {
            g_has_session = 0;
            g_session_user[0] = '\0';
        }
    }

    /* Parolası bozuksa parola alanından başla */
    int focus = (g_has_session && plen == 0) ? 1 : 0;
    int frame = 0;
    char err[96] = "";

    while (1) {
        login_draw(frame, focus, username, ulen, password, plen, err);
        err[0] = '\0';

        char ch = login_wait_key(&frame, focus, username, ulen, password, plen);

        /* Sekme / yön tuşları → alanlar arası geçiş */
        if (ch == '\t' || ch == 0x01 || ch == 0x03) {
            focus = 1 - focus;
            continue;
        }

        /* Geri sil */
        if (ch == '\b') {
            if (focus == 0 && ulen > 0) { username[--ulen] = '\0'; }
            if (focus == 1 && plen > 0) { password[--plen] = '\0'; }
            continue;
        }

        /* Enter */
        if (ch == '\n') {
            if (focus == 0) {
                if (ulen == 0) { strcpy(err, "Kullanici adi gerekli"); continue; }
                focus = 1;   /* önce parola alanına geç */
                continue;
            }
            if (plen == 0) { strcpy(err, "Parola gerekli"); continue; }

            /* Oturumu güvenli biçimde kalıcı depolamaya yaz */
            session_save(username, password);
            strcpy(g_shell.user, username);
            login_success_anim();
            cursor_x = 5;
            cursor_y = 5;
            return;
        }

        /* Normal karakter */
        if (ch >= ' ' && ch < 0x7F) {
            if (focus == 0 && ulen < SESSION_USER_MAX - 1)
                username[ulen++] = ch;
            else if (focus == 1 && plen < SESSION_PASS_MAX - 1)
                password[plen++] = ch;
            username[ulen] = '\0';
            password[plen] = '\0';
        }
    }
}

/* ─── Shell prompt ───────────────────────────────────────────── */

static void print_shell_prompt(void) {
    int x = splits[active_split].x + 5;

    video_print(g_shell.user,      x, cursor_y, 10); x += video_text_width(g_shell.user);
    video_print("@",               x, cursor_y, 10); x += font_width;
    video_print(g_shell.host,      x, cursor_y, 14); x += video_text_width(g_shell.host);
    video_print(" [",              x, cursor_y, 11); x += video_text_width(" [");
    video_print(g_shell.partition, x, cursor_y, 11); x += video_text_width(g_shell.partition);
    video_print("] ",              x, cursor_y, 11); x += video_text_width("] ");
    video_print(g_shell.cwd,       x, cursor_y, 12); x += video_text_width(g_shell.cwd);
    video_print(" # ",             x, cursor_y, 15); x += video_text_width(" # ");

    cursor_x = x;
}

/* ─── Komut girişi ───────────────────────────────────────────── */

int main_get_input(char *buffer, int max_len) {
    int pos = splits[active_split].cmd_pos;

    tty_mode_set(1);  /* TTY moduna gir: ağ izlemesini devre dışı bırak */

    while (1) {
        char ch = try_read_key();
        if (ch == 0) {
            /* Klavye beklemesi: arka planda ağ sağlığını denetle ve
               kopan bağlantıyı kullanıcıya hissettirmeden onar. */
            network_monitor_tick();
            pit_delay_ms(10);
            continue;
        }

        /* Ctrl+P → split aç / değiştir */
        if (ch == 16) {
            splits[active_split].cmd_pos = pos;
            if (num_splits == 1) {
                splits[0].h = (int)(gop_height / 2);

                splits[1].x  = 0;
                splits[1].y  = (int)(gop_height / 2);
                splits[1].w  = (int)gop_width;
                splits[1].h  = (int)(gop_height / 2);
                splits[1].cx = 5;
                splits[1].cy = splits[1].y + 5;
                splits[1].active      = 0;
                splits[1].cmd_pos     = 0;
                splits[1].needs_prompt = 1;
                splits[1].cmd_buf[0]  = '\0';
                num_splits = 2;

                video_fill_rect(0, splits[1].y - 2, (int)gop_width, 2, 8);
                active_split = 1;
            } else {
                active_split = (active_split + 1) % 2;
            }
            tty_mode_set(0);  /* TTY modundan cik: ag izlemesini etkinlestir */
            return -2;
        }

        /* Ctrl+X → split kapat */
        if (ch == 24) {
            if (num_splits == 2) {
                num_splits   = 1;
                active_split = 0;
                splits[0].h  = (int)gop_height;
                video_clear(0);
                splits[0].cx         = 5;
                splits[0].cy         = 5;
                splits[0].cmd_pos    = 0;
                splits[0].cmd_buf[0] = '\0';
                splits[0].needs_prompt = 1;
            }
            tty_mode_set(0);  /* TTY modundan cik: ag izlemesini etkinlestir */
            return -2;
        }

        /* Enter */
        if (ch == '\n') {
            buffer[pos] = '\0';
            splits[active_split].cmd_pos = 0;
            next_line();
            tty_mode_set(0);  /* TTY modundan cik: ag izlemesini etkinlestir */
            return pos;
        }

        /* Backspace */
        if (ch == '\b') {
            if (pos > 0) {
                pos--;
                if (cursor_x > splits[active_split].x + 5)
                    cursor_x -= font_width;
                else
                    cursor_x = splits[active_split].x + 5;
                erase_at(cursor_x, cursor_y);
            }
            continue;
        }

        /* Normal karakter */
        if (ch >= ' ' && pos < max_len - 1) {
            if (cursor_x >= splits[active_split].x +
                            splits[active_split].w - font_width) {
                next_line();
            }
            buffer[pos++] = ch;
            video_draw_char(ch, cursor_x, cursor_y, 15);
            cursor_x += font_width;
        }
    }
}

/* ─── Kernel entry (GOP) ─────────────────────────────────────── */

void kernel_main(gop_info_t *gop_info, void *mem_base) {
    dbg_print("[KRN] kernel_main start\n");
    dbg_print("[KRN] mem_base=");
    for (int i = 7; i >= 0; i--) {
        unsigned char c = (unsigned char)((unsigned long long)mem_base >> (8 * i));
        const char *hex = "0123456789abcdef";
        dbg_putc(hex[c >> 4]); dbg_putc(hex[c & 0xf]);
    }
    dbg_print("\n");

    /* EFI watchdog (varsayilan 5 dk) sistem calisirken reset atmasin */
    {
        EFI_SYSTEM_TABLE *st = (EFI_SYSTEM_TABLE*)io_get_system_table();
        if (st && st->BootServices)
            st->BootServices->SetWatchdogTimer(0, 0, 0, NULL);
    }

    /* UEFI RTC (gercek zamanli saat) baslat */
    {
        EFI_SYSTEM_TABLE *st = (EFI_SYSTEM_TABLE*)io_get_system_table();
        if (st && st->RuntimeServices)
            time_uefi_init(st->RuntimeServices);
    }
    dbg_print("[KRN] time_uefi_init done\n");

    /* Video başlat — GOP frame buffer */
    video_init(gop_info);
    dbg_print("[KRN] video_init done\n");

    video_clear(0);
    dbg_print("[KRN] video_clear done\n");

    /* Bellek arenası — UEFI tarafından güvenli olarak tahsis edilmiş */
    mem_init(&g_mem_arena, mem_base, 16 * 1024 * 1024);
    dbg_print("[KRN] mem_init done\n");

    /* Dosya sistemi */
    fs_init(&g_fs);
    dbg_print("[KRN] fs_init done\n");

    /* Split başlangıç ayarları */
    splits[0].x  = 0;
    splits[0].y  = 0;
    splits[0].w  = (int)gop_width;
    splits[0].h  = (int)gop_height;
    splits[0].cx = 5;
    splits[0].cy = 5;
    splits[0].active       = 1;
    splits[0].needs_prompt = 1;
    splits[0].cmd_pos      = 0;
    splits[0].cmd_buf[0]   = '\0';
    num_splits   = 1;
    active_split = 0;

    /* Acilis mesaji — cofeu fontu ile net ve okunaklı hizalanmis baslik */
    video_print("cofeuOS",  5,                   5, 10); /* yesil   */
    video_print(" v3.0",    5 + 7 * font_width,  5, 14); /* sari    */
    video_print(" cofeu",   5 + 12 * font_width, 5, 11); /* camgobegi */
    video_print(" kernel",  5 + 17 * font_width, 5, 15); /* beyaz   */
    video_print("==============================", 5, 5 + font_height + 2, 7);

    /* Shell varsayılanları */
    strcpy(g_shell.host,      "cofeu");
    strcpy(g_shell.partition, "/dev/sda1");
    /* /dev/sda1 bir aygıt dosyasıdır; çalışma dizini olamaz. */
    strcpy(g_shell.cwd,       "/");

    cursor_x = 5;
    cursor_y = 5 + font_height + 2 + font_height + 8;

    /* Login */
    show_login();

    /* Ana döngü */
    while (1) {
        if (splits[active_split].needs_prompt) {
            next_line();
            print_shell_prompt();
            splits[active_split].needs_prompt = 0;
        }

        int len = main_get_input(
            splits[active_split].cmd_buf,
            sizeof(splits[0].cmd_buf)
        );

        if (len == -2) continue;

        if (len > 0)
            shell_execute(splits[active_split].cmd_buf);

        splits[active_split].cmd_pos    = 0;
        splits[active_split].needs_prompt = 1;
    }
}
