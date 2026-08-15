/*
 * SHELL.H - cofeuOS Shell API
 */

#ifndef _SHELL_H
#define _SHELL_H

#include "types.h"
#include "fs.h"
#include "string.h"
#include "web.h"

#define MAX_PATH_LEN 256

typedef struct {
    int x, y, w, h;
    int cx, cy;
    int active;
    char cmd_buf[512];
    int cmd_pos;
    int needs_prompt;
} shell_split_t;

#define MAX_SPLITS 2
extern shell_split_t splits[MAX_SPLITS];
extern int active_split;
extern int num_splits;

#define cursor_x (splits[active_split].cx)
#define cursor_y (splits[active_split].cy)

typedef struct {
    char user[32];
    char host[32];
    char partition[32];
    char cwd[MAX_PATH_LEN];
} shell_control;

/* ─── Masaüstü Pencere Yönetimi (cofeuDE GUI) ───────────────── */
#define WINDOW_TYPE_TERMINAL 1
#define WINDOW_TYPE_FILES    2
#define WINDOW_TYPE_NOTES    3
#define WINDOW_TYPE_INFO     4
#define WINDOW_TYPE_CALC     5
#define WINDOW_TYPE_BROWSER  6

#define MAX_GUI_WINDOWS 8
#define GUI_TERM_ROWS   20
#define GUI_TERM_COLS   80

typedef struct {
    int id;
    int active;
    int minimized;
    int x, y, w, h;
    char title[32];
    int type;

    /* Terminal verileri */
    char term_screen[GUI_TERM_ROWS][GUI_TERM_COLS + 1];
    u8   term_colors[GUI_TERM_ROWS][GUI_TERM_COLS + 1];
    int  term_cx, term_cy;
    char input_buf[128];
    int  input_pos;

    /* Tarayıcı verileri */
    char url_buf[128];
    int  url_pos;
    char page_buf[4096];
    int  page_len;
    int  page_scroll;
    char page_base[160];      /* geçerli sayfanın tam URL'si */
    web_document_t *doc;      /* DOM ağacı */
    web_css_rule_t *css;      /* ayrıştırılmış stiller */
    web_box_t      *layout;   /* yerleşim kutusu ağacı */
    int             page_h;   /* yerleşim toplam yüksekliği */

    /* Hesap makinesi verileri */
    char calc_disp[32];
    int  calc_acc;
    char calc_op;
    int  calc_entered;
} desktop_window_t;

extern shell_control g_shell;
extern int shell_execute(const char* cmd);

#endif

