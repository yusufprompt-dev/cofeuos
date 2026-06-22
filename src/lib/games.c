/*
 * GAMES.C - cofeuOS Oyun Modülleri
 * Snake, Guess, Tic-Tac-Toe, Breakout
 */

#include "../include/games.h"
#include "../include/video.h"
#include "../include/keyboard.h"
#include "../include/string.h"
#include "../include/types.h"

/* ============================================================
 * YARDIMCI FONKSİYONLAR
 * ============================================================ */

static u32 rng_seed = 12345;

static u32 rng_next(void) {
    rng_seed = rng_seed * 1664525u + 1013904223u;
    return rng_seed;
}

/* Sayıyı ekrana çiz (video_print kullanarak) */
static void draw_int(int val, int x, int y, u8 color) {
    char buf[12];
    int pos = 0;
    if (val < 0) { buf[pos++] = '-'; val = -val; }
    if (val == 0) { buf[pos++] = '0'; }
    else {
        char tmp[10]; int tlen = 0;
        while (val > 0) { tmp[tlen++] = '0' + val % 10; val /= 10; }
        for (int i = tlen - 1; i >= 0; i--) buf[pos++] = tmp[i];
    }
    buf[pos] = '\0';
    video_print(buf, x, y, color);
}

/* Küçük ekran ortası hesaplama */
static int center_x(int w) { return (SCREEN_WIDTH  - w) / 2; }
static int center_y(int h) { return (SCREEN_HEIGHT - h) / 2; }

/* Çerçeveli pencere çiz */
static void draw_box(int x, int y, int w, int h, u8 border, u8 bg) {
    video_fill_rect(x, y, w, h, bg);
    video_draw_rect(x, y, w, h, border);
}

/* Oyun bitişi: bir tuşa basınca devam et */
static void wait_any_key(void) {
    while (!try_read_key()) {}
    /* key buffer'ı boşalt */
    kbd_delay(500000);
}

/* ============================================================
 * 1. SNAKE — Yılan Oyunu
 * Grid: 22x16 hücre, hücre boyutu 12px
 * Ekran: 320x200
 * ============================================================ */

#define SN_COLS   22
#define SN_ROWS   16
#define SN_CELL   12   /* piksel */
#define SN_OX     ((SCREEN_WIDTH  - SN_COLS * SN_CELL) / 2)
#define SN_OY     20   /* üstte bilgi alanı için */
#define SN_MAXLEN (SN_COLS * SN_ROWS)
#define SN_SPEED  200000u  /* gecikme döngüsü */

static int sn_x[SN_MAXLEN], sn_y[SN_MAXLEN];
static int sn_len, sn_dx, sn_dy;
static int sn_fx, sn_fy;   /* yem */
static int sn_score;

static void sn_place_food(void) {
    int ok;
    do {
        ok = 1;
        sn_fx = (int)(rng_next() % (u32)SN_COLS);
        sn_fy = (int)(rng_next() % (u32)SN_ROWS);
        for (int i = 0; i < sn_len; i++)
            if (sn_x[i] == sn_fx && sn_y[i] == sn_fy) { ok = 0; break; }
    } while (!ok);
}

static void sn_draw_cell(int cx, int cy, u8 color) {
    int px = SN_OX + cx * SN_CELL;
    int py = SN_OY + cy * SN_CELL;
    video_fill_rect(px + 1, py + 1, SN_CELL - 2, SN_CELL - 2, color);
}

static void sn_draw_all(void) {
    /* Zemin */
    video_fill_rect(SN_OX, SN_OY, SN_COLS * SN_CELL, SN_ROWS * SN_CELL, 0);
    video_draw_rect(SN_OX - 1, SN_OY - 1, SN_COLS * SN_CELL + 2, SN_ROWS * SN_CELL + 2, 15);

    /* Yem */
    sn_draw_cell(sn_fx, sn_fy, 12); /* kırmızımsı */

    /* Yılan */
    for (int i = 0; i < sn_len; i++)
        sn_draw_cell(sn_x[i], sn_y[i], i == 0 ? 10 : 2); /* baş=parlak yeşil, gövde=koyu yeşil */

    /* Skor */
    video_fill_rect(0, 0, SCREEN_WIDTH, SN_OY - 2, 1);
    video_print("SNAKE  Score:", 4, 4, 14);
    draw_int(sn_score, 110, 4, 15);
    video_print("WASD=Yon  ESC=Cik", 160, 4, 7);
}

void game_snake(void) {
    video_clear(0);

    /* Başlangıç */
    sn_len   = 3;
    sn_dx    = 1; sn_dy = 0;
    sn_score = 0;
    sn_x[0] = SN_COLS / 2;     sn_y[0] = SN_ROWS / 2;
    sn_x[1] = SN_COLS / 2 - 1; sn_y[1] = SN_ROWS / 2;
    sn_x[2] = SN_COLS / 2 - 2; sn_y[2] = SN_ROWS / 2;
    sn_place_food();
    sn_draw_all();

    while (1) {
        /* Tuş kontrolü (bloklamayan) */
        char k = try_read_key();
        if (k == 'w' || k == 'W') { if (sn_dy != 1)  { sn_dx = 0;  sn_dy = -1; } }
        if (k == 's' || k == 'S') { if (sn_dy != -1) { sn_dx = 0;  sn_dy =  1; } }
        if (k == 'a' || k == 'A') { if (sn_dx != 1)  { sn_dx = -1; sn_dy =  0; } }
        if (k == 'd' || k == 'D') { if (sn_dx != -1) { sn_dx =  1; sn_dy =  0; } }
        if (k == 27) break; /* ESC çıkış */

        /* Gecikme */
        kbd_delay(SN_SPEED);

        /* Yeni baş */
        int nx = sn_x[0] + sn_dx;
        int ny = sn_y[0] + sn_dy;

        /* Duvar çarpışması */
        if (nx < 0 || nx >= SN_COLS || ny < 0 || ny >= SN_ROWS) goto game_over;

        /* Kendine çarpışma */
        for (int i = 0; i < sn_len - 1; i++)
            if (sn_x[i] == nx && sn_y[i] == ny) goto game_over;

        /* Kuyruğu kaydır */
        for (int i = sn_len - 1; i > 0; i--) {
            sn_x[i] = sn_x[i - 1];
            sn_y[i] = sn_y[i - 1];
        }
        sn_x[0] = nx; sn_y[0] = ny;

        /* Yem yendi mi? */
        if (nx == sn_fx && ny == sn_fy) {
            sn_score += 10;
            if (sn_len < SN_MAXLEN) sn_len++;
            sn_place_food();
        }

        sn_draw_all();
        continue;

    game_over:
        video_fill_rect(center_x(160), center_y(40), 160, 40, 1);
        video_draw_rect(center_x(160), center_y(40), 160, 40, 15);
        video_print("OYUN BITTI!", center_x(160) + 20, center_y(40) + 6, 12);
        video_print("Skor:", center_x(160) + 20, center_y(40) + 20, 15);
        draw_int(sn_score, center_x(160) + 65, center_y(40) + 20, 10);
        video_print("Tus=Cik", center_x(160) + 20, center_y(40) + 30, 7);
        wait_any_key();
        break;
    }

    video_clear(0);
}

/* ============================================================
 * 2. GUESS — Sayı Tahmin Oyunu
 * ============================================================ */

extern int cursor_x, cursor_y;

static void g_print(const char* s, u8 col) {
    video_print(s, cursor_x, cursor_y, col);
    cursor_x += video_text_width(s);
}

static void g_newline(void) {
    cursor_x = 10;
    cursor_y += line_height + 2;
}

static int g_getint(void) {
    char buf[8];
    int pos = 0;
    int bx  = cursor_x;

    while (1) {
        char c = read_key();
        if (c == '\n') { buf[pos] = '\0'; break; }
        if (c == '\b' && pos > 0) {
            pos--;
            cursor_x -= font_width;
            if (cursor_x < bx) cursor_x = bx;
            video_fill_rect(cursor_x, cursor_y, font_width, font_height, 0);
            continue;
        }
        if (c >= '0' && c <= '9' && pos < 5) {
            buf[pos++] = c;
            video_draw_char(c, cursor_x, cursor_y, 15);
            cursor_x += font_width;
        }
    }
    if (pos == 0) return -1;
    int v = 0;
    for (int i = 0; i < pos; i++) v = v * 10 + (buf[i] - '0');
    return v;
}

void game_guess(void) {
    video_clear(0);

    /* Başlık */
    int bw = 240, bh = 160;
    int bx = center_x(bw), by = center_y(bh);
    draw_box(bx, by, bw, bh, 14, 0);
    video_print("SAYI TAHMIN", bx + 60, by + 6, 14);
    video_draw_rect(bx, by, bw, 16, 14);

    cursor_x = bx + 10;
    cursor_y = by + 22;

    g_print("1-100 arasinda bir sayi tuttum.", 15); g_newline();
    g_print("7 hakkin var. Basla!", 11); g_newline(); g_newline();

    int secret = (int)(rng_next() % 100u) + 1;
    int tries  = 0;
    int won    = 0;

    while (tries < 7) {
        g_print("Tahmin (", 7);
        draw_int(7 - tries, cursor_x, cursor_y, 10);
        cursor_x += font_width * 2;
        g_print(" hak): ", 7);

        int guess = g_getint();
        g_newline();
        tries++;

        if (guess < 1 || guess > 100) {
            g_print("1-100 arasi gir!", 12); g_newline();
            tries--;
            continue;
        }

        if (guess == secret) {
            won = 1;
            break;
        } else if (guess < secret) {
            g_print("Daha BUYUK!", 13);
        } else {
            g_print("Daha KUCUK!", 13);
        }
        g_newline();

        if (cursor_y > by + bh - 20) {
            /* Scroll içeriği */
            cursor_y -= line_height + 2;
        }
    }

    g_newline();
    if (won) {
        g_print("TEBRIKLER! ", 10);
        draw_int(tries, cursor_x, cursor_y, 10);
        cursor_x += font_width * 2;
        g_print(". denemede buldun!", 10);
    } else {
        g_print("Kaybettin! Sayi: ", 12);
        draw_int(secret, cursor_x, cursor_y, 15);
    }
    g_newline(); g_newline();
    g_print("Herhangi bir tusa bas...", 7);

    wait_any_key();
    video_clear(0);
}

/* ============================================================
 * 3. TIC-TAC-TOE — XOX
 * ============================================================ */

#define TTT_EMPTY 0
#define TTT_X     1
#define TTT_O     2

#define TTT_CX    ((SCREEN_WIDTH  - 90) / 2)
#define TTT_CY    ((SCREEN_HEIGHT - 90) / 2)
#define TTT_CS    30   /* hücre boyutu */

static u8 ttt_board[9];
static int ttt_sel;

static void ttt_draw(void) {
    video_clear(0);
    video_print("TIC-TAC-TOE", center_x(88), 4, 14);
    video_print("Sen=X  CPU=O  1-9=Sec  ESC=Cik", 10, SCREEN_HEIGHT - 12, 7);

    int ox = TTT_CX, oy = TTT_CY;

    /* Izgara çizgileri */
    for (int i = 1; i < 3; i++) {
        video_fill_rect(ox + i * TTT_CS - 1, oy,     2, 3 * TTT_CS, 15);
        video_fill_rect(ox,     oy + i * TTT_CS - 1, 3 * TTT_CS, 2, 15);
    }

    /* Hücreler */
    for (int i = 0; i < 9; i++) {
        int cx = ox + (i % 3) * TTT_CS;
        int cy = oy + (i / 3) * TTT_CS;
        int mid_x = cx + TTT_CS / 2;
        int mid_y = cy + TTT_CS / 2;

        /* Seçili hücre vurgusu */
        if (i == ttt_sel)
            video_fill_rect(cx + 2, cy + 2, TTT_CS - 4, TTT_CS - 4, 1);

        if (ttt_board[i] == TTT_X) {
            /* X çiz */
            for (int d = 0; d < 10; d++) {
                video_put_pixel(cx + 4 + d,      cy + 4 + d,      10);
                video_put_pixel(cx + 4 + d,      cy + 4 + d + 1,  10);
                video_put_pixel(cx + TTT_CS-5-d, cy + 4 + d,      10);
                video_put_pixel(cx + TTT_CS-5-d, cy + 4 + d + 1,  10);
            }
        } else if (ttt_board[i] == TTT_O) {
            /* O çiz (daire yaklaşımı) */
            for (int a = 0; a < 32; a++) {
                /* basit sekizgen */
                int r = 10;
                static const int dx8[8] = {0, 7, 10, 7, 0, -7,-10,-7};
                static const int dy8[8] = {-10,-7, 0,  7,10,  7,  0,-7};
                video_fill_rect(mid_x + dx8[a % 8] - 1, mid_y + dy8[a % 8] - 1, 2, 2, 12);
                (void)r;
            }
        }
    }
}

static int ttt_check_winner(void) {
    static const int lines[8][3] = {
        {0,1,2},{3,4,5},{6,7,8},
        {0,3,6},{1,4,7},{2,5,8},
        {0,4,8},{2,4,6}
    };
    for (int l = 0; l < 8; l++) {
        int a = ttt_board[lines[l][0]];
        if (a && a == ttt_board[lines[l][1]] && a == ttt_board[lines[l][2]])
            return a;
    }
    /* Beraberlik kontrolü */
    int full = 1;
    for (int i = 0; i < 9; i++) if (!ttt_board[i]) { full = 0; break; }
    return full ? -1 : 0;
}

static void ttt_cpu_move(void) {
    static const int lines[8][3] = {
        {0,1,2},{3,4,5},{6,7,8},
        {0,3,6},{1,4,7},{2,5,8},
        {0,4,8},{2,4,6}
    };
    /* Önce kazanmayı dene, sonra bloke et, sonra merkez, sonra rastgele */
    for (int who = TTT_O; who >= TTT_X; who--) {
        for (int l = 0; l < 8; l++) {
            int empty = -1, cnt = 0;
            for (int i = 0; i < 3; i++) {
                int cell = lines[l][i];
                if (ttt_board[cell] == (u8)who) cnt++;
                else if (!ttt_board[cell]) empty = cell;
            }
            if (cnt == 2 && empty >= 0) {
                ttt_board[empty] = TTT_O;
                return;
            }
        }
    }
    if (!ttt_board[4]) { ttt_board[4] = TTT_O; return; }
    /* Rastgele boş hücre */
    for (int i = 0; i < 9; i++) {
        int idx = (int)(rng_next() % 9u);
        if (!ttt_board[idx]) { ttt_board[idx] = TTT_O; return; }
    }
    for (int i = 0; i < 9; i++) if (!ttt_board[i]) { ttt_board[i] = TTT_O; return; }
}

void game_tictactoe(void) {
    memset(ttt_board, 0, sizeof(ttt_board));
    ttt_sel = 4;
    ttt_draw();

    while (1) {
        char k = read_key();
        if (k == 27) break;

        /* 1-9 tuşları ile hücre seçimi ve hamle */
        if (k >= '1' && k <= '9') {
            int cell = k - '1';
            if (!ttt_board[cell]) {
                ttt_board[cell] = TTT_X;
                ttt_sel = cell;
                ttt_draw();

                int w = ttt_check_winner();
                if (w) goto show_result;

                /* CPU hamlesi */
                ttt_cpu_move();
                ttt_draw();

                w = ttt_check_winner();
                if (w) goto show_result;
            }
        }
        /* WASD ile seçim gezinme */
        if (k == 'a' && ttt_sel % 3 > 0) ttt_sel--;
        if (k == 'd' && ttt_sel % 3 < 2) ttt_sel++;
        if (k == 'w' && ttt_sel / 3 > 0) ttt_sel -= 3;
        if (k == 's' && ttt_sel / 3 < 2) ttt_sel += 3;
        /* Enter: mevcut seçim */
        if (k == '\n' && !ttt_board[ttt_sel]) {
            ttt_board[ttt_sel] = TTT_X;
            ttt_draw();
            int w = ttt_check_winner();
            if (w) goto show_result;
            ttt_cpu_move();
            ttt_draw();
            w = ttt_check_winner();
            if (w) goto show_result;
        }
        ttt_draw();
        continue;

    show_result:;
        int result = ttt_check_winner();
        int rx = center_x(160), ry = center_y(30);
        draw_box(rx, ry, 160, 30, 15, 0);
        if (result == TTT_X)       video_print("KAZANDIN! Tebrikler!", rx + 8, ry + 10, 10);
        else if (result == TTT_O)  video_print("CPU Kazandi. Tekrar?", rx + 8, ry + 10, 12);
        else                       video_print("Beraberlik!", rx + 30, ry + 10, 14);
        wait_any_key();
        break;
    }

    video_clear(0);
}

/* ============================================================
 * 4. BREAKOUT — Top Tuğla Kıran Oyun
 * ============================================================ */

#define BRK_BW   10    /* Tuğla genişliği */
#define BRK_BH    6    /* Tuğla yüksekliği */
#define BRK_COLS 26    /* Tuğla sütunları */
#define BRK_ROWS  5    /* Tuğla satırları */
#define BRK_PAD_W 40   /* Platform genişliği */
#define BRK_PAD_H  5   /* Platform yüksekliği */
#define BRK_BALL   4   /* Top yarıçapı */
#define BRK_OX    10   /* Izgara x başlangıç */
#define BRK_OY    18   /* Izgara y başlangıç */
#define BRK_SPEED 30000u

static u8  brk_bricks[BRK_ROWS][BRK_COLS];
static int brk_pad_x;
static int brk_ball_x, brk_ball_y;
static int brk_ball_dx, brk_ball_dy;
static int brk_score, brk_lives;

static void brk_draw(void) {
    video_clear(0);

    /* Bilgi çubuğu */
    video_fill_rect(0, 0, SCREEN_WIDTH, 12, 1);
    video_print("BREAKOUT  Skor:", 4, 2, 14);
    draw_int(brk_score, 90, 2, 15);
    video_print("Can:", 160, 2, 14);
    draw_int(brk_lives, 190, 2, 12);
    video_print("A/D=Hareket  ESC=Cik", SCREEN_WIDTH - 132, 2, 7);

    /* Tuğlalar */
    static const u8 brick_colors[BRK_ROWS] = {12, 4, 6, 10, 3};
    for (int r = 0; r < BRK_ROWS; r++) {
        for (int c = 0; c < BRK_COLS; c++) {
            if (brk_bricks[r][c]) {
                video_fill_rect(BRK_OX + c * (BRK_BW + 1),
                                BRK_OY + r * (BRK_BH + 1),
                                BRK_BW, BRK_BH,
                                brick_colors[r]);
            }
        }
    }

    /* Platform */
    video_fill_rect(brk_pad_x, SCREEN_HEIGHT - 20,
                    BRK_PAD_W, BRK_PAD_H, 9);

    /* Top */
    video_fill_rect(brk_ball_x - BRK_BALL, brk_ball_y - BRK_BALL,
                    BRK_BALL * 2, BRK_BALL * 2, 15);
}

void game_breakout(void) {
    /* Başlangıç */
    for (int r = 0; r < BRK_ROWS; r++)
        for (int c = 0; c < BRK_COLS; c++)
            brk_bricks[r][c] = 1;

    brk_pad_x   = (SCREEN_WIDTH - BRK_PAD_W) / 2;
    brk_ball_x  = SCREEN_WIDTH / 2;
    brk_ball_y  = SCREEN_HEIGHT - 40;
    brk_ball_dx = 2;
    brk_ball_dy = -2;
    brk_score   = 0;
    brk_lives   = 3;

    brk_draw();

    while (brk_lives > 0) {
        char k = try_read_key();
        if (k == 27) break;
        if ((k == 'a' || k == 'A') && brk_pad_x > 2)                         brk_pad_x -= 4;
        if ((k == 'd' || k == 'D') && brk_pad_x < SCREEN_WIDTH - BRK_PAD_W - 2) brk_pad_x += 4;

        kbd_delay(BRK_SPEED);

        /* Top hareketi */
        brk_ball_x += brk_ball_dx;
        brk_ball_y += brk_ball_dy;

        /* Duvar yansıması */
        if (brk_ball_x <= BRK_BALL || brk_ball_x >= SCREEN_WIDTH - BRK_BALL)
            brk_ball_dx = -brk_ball_dx;
        if (brk_ball_y <= 14)
            brk_ball_dy = -brk_ball_dy;

        /* Platform çarpışması */
        if (brk_ball_y + BRK_BALL >= SCREEN_HEIGHT - 20 &&
            brk_ball_y + BRK_BALL <= SCREEN_HEIGHT - 14 &&
            brk_ball_x >= brk_pad_x && brk_ball_x <= brk_pad_x + BRK_PAD_W) {
            brk_ball_dy = -2;
            /* Açı: platforma göre */
            int hit_pos = brk_ball_x - brk_pad_x;
            if (hit_pos < BRK_PAD_W / 3)      brk_ball_dx = -2;
            else if (hit_pos > 2*BRK_PAD_W/3) brk_ball_dx =  2;
        }

        /* Top düştü */
        if (brk_ball_y > SCREEN_HEIGHT) {
            brk_lives--;
            if (brk_lives <= 0) break;
            brk_ball_x  = SCREEN_WIDTH / 2;
            brk_ball_y  = SCREEN_HEIGHT - 40;
            brk_ball_dx = 2;
            brk_ball_dy = -2;
        }

        /* Tuğla çarpışması */
        for (int r = 0; r < BRK_ROWS; r++) {
            for (int c = 0; c < BRK_COLS; c++) {
                if (!brk_bricks[r][c]) continue;
                int bx = BRK_OX + c * (BRK_BW + 1);
                int by = BRK_OY + r * (BRK_BH + 1);
                if (brk_ball_x + BRK_BALL >= bx &&
                    brk_ball_x - BRK_BALL <= bx + BRK_BW &&
                    brk_ball_y + BRK_BALL >= by &&
                    brk_ball_y - BRK_BALL <= by + BRK_BH) {
                    brk_bricks[r][c] = 0;
                    brk_score += 5;
                    brk_ball_dy = -brk_ball_dy;
                }
            }
        }

        /* Tüm tuğlalar kırıldı mı? */
        int total = 0;
        for (int r = 0; r < BRK_ROWS; r++)
            for (int c = 0; c < BRK_COLS; c++)
                total += brk_bricks[r][c];
        if (total == 0) {
            brk_draw();
            int rx = center_x(160), ry = center_y(30);
            draw_box(rx, ry, 160, 30, 14, 0);
            video_print("TEBRIKLER! Kazandin!", rx + 10, ry + 10, 10);
            wait_any_key();
            video_clear(0);
            return;
        }

        brk_draw();
    }

    /* Oyun bitti */
    brk_draw();
    int rx = center_x(160), ry = center_y(36);
    draw_box(rx, ry, 160, 36, 12, 0);
    video_print("OYUN BITTI!", rx + 30, ry + 6, 12);
    video_print("Skor:", rx + 10, ry + 20, 15);
    draw_int(brk_score, rx + 55, ry + 20, 10);
    wait_any_key();
    video_clear(0);
}
