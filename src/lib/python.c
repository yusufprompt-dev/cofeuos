/*
 * PYTHON.C - cofeuOS Mini Python REPL
 *
 * Desteklenen özellikler:
 *   x = 42              # Tamsayı atama
 *   x = 2 + 3 * 4      # Aritmetik (+, -, *, /, %)
 *   print(x)            # Değişken yazdır
 *   print("metin")      # String yazdır
 *   if x > 5: print(x)  # Inline if (tek satır gövde)
 *   while x > 0: x = x - 1  # Inline while
 *   for i in range(n): print(i)  # for range
 *   exit()              # REPL'den çık
 */

#include "../include/python.h"
#include "../include/video.h"
#include "../include/keyboard.h"
#include "../include/string.h"
#include "../include/types.h"

/* ============================================================
 * Değişken deposu (en fazla 16 tam sayı değişken)
 * ============================================================ */
#define PY_MAX_VARS 16
#define PY_NAMELEN  16

typedef struct {
    char  name[PY_NAMELEN];
    int   value;
    u8    used;
} py_var;



static py_var py_vars[PY_MAX_VARS];

static py_var* py_find_var(const char* name) {
    for (int i = 0; i < PY_MAX_VARS; i++)
        if (py_vars[i].used && strncmp(py_vars[i].name, name, PY_NAMELEN) == 0)
            return &py_vars[i];
    return NULL;
}

static py_var* py_set_var(const char* name, int val) {
    py_var* v = py_find_var(name);
    if (!v) {
        /* Yeni değişken */
        for (int i = 0; i < PY_MAX_VARS; i++) {
            if (!py_vars[i].used) {
                v = &py_vars[i];
                strncpy(v->name, name, PY_NAMELEN - 1);
                v->name[PY_NAMELEN - 1] = '\0';
                v->used = 1;
                break;
            }
        }
    }
    if (v) v->value = val;
    return v;
}

/* ============================================================
 * Lekser / İfade Ayrıştırıcı
 * ============================================================ */

/* Boşlukları atla */
static const char* py_skip(const char* p) {
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* Alfanumerik token oku */
static int py_read_ident(const char* p, char* out, int maxlen) {
    int n = 0;
    while ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
           (*p >= '0' && *p <= '9') ||  *p == '_') {
        if (n < maxlen - 1) out[n++] = *p;
        p++;
    }
    out[n] = '\0';
    return n;
}

/* Tamsayı oku */
static int py_read_int(const char* p, int* out) {
    int sign = 1, val = 0, n = 0;
    if (*p == '-') { sign = -1; p++; n++; }
    while (*p >= '0' && *p <= '9') { val = val * 10 + (*p - '0'); p++; n++; }
    *out = val * sign;
    return n;
}

/* İleri bildirim */
static int py_eval_expr(const char* p, int* val, const char** end);

/* Birincil değer: sayı, değişken, parantez */
static int py_eval_primary(const char* p, int* val, const char** end) {
    p = py_skip(p);
    if (*p == '(') {
        int ok = py_eval_expr(p + 1, val, &p);
        p = py_skip(p);
        if (*p == ')') p++;
        if (end) *end = p;
        return ok;
    }
    if (*p == '-' || (*p >= '0' && *p <= '9')) {
        int n = py_read_int(p, val);
        if (end) *end = p + n;
        return n > 0 ? 1 : 0;
    }
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || *p == '_') {
        char name[PY_NAMELEN];
        int  n = py_read_ident(p, name, PY_NAMELEN);
        py_var* v = py_find_var(name);
        *val = v ? v->value : 0;
        if (end) *end = p + n;
        return 1;
    }
    if (end) *end = p;
    *val = 0;
    return 0;
}

/* Çarpma/bölme/mod */
static int py_eval_term(const char* p, int* val, const char** end) {
    int ok = py_eval_primary(p, val, &p);
    while (ok) {
        p = py_skip(p);
        if (*p != '*' && *p != '/' && *p != '%') break;
        char op = *p++;
        int rhs;
        ok = py_eval_primary(p, &rhs, &p);
        if (!ok) break;
        if (op == '*') *val *= rhs;
        else if (op == '/') { if (rhs) *val /= rhs; }
        else if (op == '%') { if (rhs) *val %= rhs; }
    }
    if (end) *end = p;
    return 1;
}

/* Toplama/çıkarma */
static int py_eval_add(const char* p, int* val, const char** end) {
    int ok = py_eval_term(p, val, &p);
    while (ok) {
        p = py_skip(p);
        if (*p != '+' && *p != '-') break;
        char op = *p++;
        int rhs;
        ok = py_eval_term(p, &rhs, &p);
        if (!ok) break;
        if (op == '+') *val += rhs;
        else           *val -= rhs;
    }
    if (end) *end = p;
    return 1;
}

/* Karşılaştırma: ==, !=, <, >, <=, >= */
static int py_eval_cmp(const char* p, int* val, const char** end) {
    int ok = py_eval_add(p, val, &p);
    p = py_skip(p);
    if (*p == '=' && *(p+1) == '=') {
        int rhs; py_eval_add(p + 2, &rhs, &p);
        *val = (*val == rhs) ? 1 : 0;
    } else if (*p == '!' && *(p+1) == '=') {
        int rhs; py_eval_add(p + 2, &rhs, &p);
        *val = (*val != rhs) ? 1 : 0;
    } else if (*p == '<' && *(p+1) == '=') {
        int rhs; py_eval_add(p + 2, &rhs, &p);
        *val = (*val <= rhs) ? 1 : 0;
    } else if (*p == '>' && *(p+1) == '=') {
        int rhs; py_eval_add(p + 2, &rhs, &p);
        *val = (*val >= rhs) ? 1 : 0;
    } else if (*p == '<' && *(p+1) != '<') {
        int rhs; py_eval_add(p + 1, &rhs, &p);
        *val = (*val < rhs) ? 1 : 0;
    } else if (*p == '>' && *(p+1) != '>') {
        int rhs; py_eval_add(p + 1, &rhs, &p);
        *val = (*val > rhs) ? 1 : 0;
    }
    if (end) *end = p;
    return ok;
}

static int py_eval_expr(const char* p, int* val, const char** end) {
    return py_eval_cmp(p, val, end);
}

/* ============================================================
 * Ekran / I/O
 * ============================================================ */

#include "../include/shell.h"

static void py_print_str(const char* s, u8 col) {
    video_print(s, cursor_x, cursor_y, col);
    cursor_x += video_text_width(s);
}

static void py_newline(void) {
    cursor_x = 4;
    cursor_y += font_height + 2;
    if (cursor_y > SCREEN_HEIGHT - font_height - 2) {
        video_scroll();
        cursor_y = SCREEN_HEIGHT - font_height - 2;
    }
}

static void py_print_int(int v) {
    char buf[14];
    int pos = 0;
    if (v < 0) { buf[pos++] = '-'; v = -v; }
    if (v == 0) { buf[pos++] = '0'; }
    else {
        char tmp[12]; int tlen = 0;
        while (v > 0) { tmp[tlen++] = '0' + v % 10; v /= 10; }
        for (int i = tlen - 1; i >= 0; i--) buf[pos++] = tmp[i];
    }
    buf[pos] = '\0';
    py_print_str(buf, 10);
}

/* REPL için tek satır okuma */
static int py_readline(char* buf, int maxlen) {
    int pos = 0;
    int bx  = cursor_x;

    while (1) {
        char c = read_key();
        if (c == '\n') { buf[pos] = '\0'; py_newline(); return pos; }
        if (c == '\b') {
            if (pos > 0) {
                pos--;
                cursor_x -= font_width;
                if (cursor_x < bx) cursor_x = bx;
                video_fill_rect(cursor_x, cursor_y, font_width, font_height, 0);
            }
            continue;
        }
        if (c >= ' ' && pos < maxlen - 1) {
            buf[pos++] = c;
            video_draw_char(c, cursor_x, cursor_y, 15);
            cursor_x += font_width;
            if (cursor_x >= SCREEN_WIDTH - font_width) {
                cursor_x = bx;
                cursor_y += font_height + 2;
            }
        }
    }
}

/* ============================================================
 * İfade Yürütücü (statement executor)
 * ============================================================ */

/* Tek bir ifadeyi yürüt, hata mesajı için msg veya NULL döndür */
static const char* py_exec(const char* line);

/* print(...) işle */
static void py_do_print(const char* args) {
    args = py_skip(args);
    if (*args == '"' || *args == '\'') {
        /* String literal */
        char quote = *args++;
        while (*args && *args != quote) {
            if (*args == '\\' && *(args+1) == 'n') {
                py_newline();
                args += 2;
            } else {
                video_draw_char(*args, cursor_x, cursor_y, 15);
                cursor_x += font_width;
                args++;
            }
        }
    } else {
        /* İfade */
        int val;
        py_eval_expr(args, &val, NULL);
        py_print_int(val);
    }
}

/* Tek satır ifade yürütücüsü */
static const char* py_exec(const char* line) {
    line = py_skip(line);
    if (!*line || *line == '#') return NULL; /* boş / yorum */

    /* exit() */
    if (strncmp(line, "exit()", 6) == 0) return "EXIT";

    /* print(...) */
    if (strncmp(line, "print(", 6) == 0) {
        const char* args = line + 6;
        /* Kapanış parantezini bul */
        char argbuf[128];
        int alen = 0;
        while (*args && *args != ')' && alen < 127) argbuf[alen++] = *args++;
        argbuf[alen] = '\0';
        py_do_print(argbuf);
        py_newline();
        return NULL;
    }

    /* if koşul: gövde */
    if (strncmp(line, "if ", 3) == 0) {
        const char* cond = line + 3;
        const char* colon = NULL;
        /* ':' bul */
        for (const char* p = cond; *p; p++) {
            if (*p == ':') { colon = p; break; }
        }
        if (!colon) return "SyntaxError: if satiri ':' gerektiriyor";
        int val;
        py_eval_expr(cond, &val, NULL);
        if (val) {
            const char* body = py_skip(colon + 1);
            if (*body) py_exec(body);
        }
        return NULL;
    }

    /* while koşul: gövde */
    if (strncmp(line, "while ", 6) == 0) {
        const char* cond = line + 6;
        const char* colon = NULL;
        for (const char* p = cond; *p; p++) {
            if (*p == ':') { colon = p; break; }
        }
        if (!colon) return "SyntaxError: while satiri ':' gerektiriyor";
        char cond_buf[64];
        int clen = (int)(colon - cond);
        if (clen > 63) clen = 63;
        strncpy(cond_buf, cond, (size_t)clen);
        cond_buf[clen] = '\0';
        const char* body = py_skip(colon + 1);
        int limit = 10000; /* sonsuz döngü koruması */
        while (limit-- > 0) {
            int val;
            py_eval_expr(cond_buf, &val, NULL);
            if (!val) break;
            py_exec(body);
            /* ESC ile çıkış */
            char k = try_read_key();
            if (k == 27) { py_print_str("[while sonlandi]", 12); py_newline(); break; }
        }
        if (limit <= 0) { py_print_str("[while limit: 10000]", 12); py_newline(); }
        return NULL;
    }

    /* for i in range(n): gövde */
    if (strncmp(line, "for ", 4) == 0) {
        /* "for VAR in range(N): BODY" */
        const char* p = line + 4;
        char varname[PY_NAMELEN];
        int n = py_read_ident(p, varname, PY_NAMELEN);
        p = py_skip(p + n);
        if (strncmp(p, "in range(", 9) != 0)
            return "SyntaxError: for <var> in range(<n>): destekleniyor";
        p += 9;
        int range_n = 0;
        py_eval_expr(p, &range_n, &p);
        /* ): bul */
        while (*p && *p != ':') p++;
        if (*p != ':') return "SyntaxError: ':' bekleniyor";
        const char* body = py_skip(p + 1);
        int limit = (range_n > 10000) ? 10000 : range_n;
        for (int i = 0; i < limit; i++) {
            py_set_var(varname, i);
            py_exec(body);
            char k = try_read_key();
            if (k == 27) { py_print_str("[for sonlandi]", 12); py_newline(); break; }
        }
        return NULL;
    }

    /* Değişken ataması: VAR = İFADE veya VAR += İFADE vs */
    {
        char varname[PY_NAMELEN];
        int  n = py_read_ident(line, varname, PY_NAMELEN);
        if (n > 0) {
            const char* after = py_skip(line + n);
            char op = 0;
            if ((*after == '+' || *after == '-' || *after == '*' || *after == '/' || *after == '%') && *(after + 1) == '=') {
                op = *after;
                after += 2;
            } else if (*after == '=') {
                after += 1;
            }
            
            if (op || *(py_skip(line + n)) == '=') {
                int val = 0;
                py_eval_expr(after, &val, NULL);
                if (op) {
                    py_var* v = py_find_var(varname);
                    int current = v ? v->value : 0;
                    if (op == '+') val = current + val;
                    else if (op == '-') val = current - val;
                    else if (op == '*') val = current * val;
                    else if (op == '/') { if (val != 0) val = current / val; else val = 0; }
                    else if (op == '%') { if (val != 0) val = current % val; else val = 0; }
                }
                py_set_var(varname, val);
                return NULL;
            }
        }
    }

    return "SyntaxError: bilinmeyen ifade";
}

/* ============================================================
 * Ana REPL Döngüsü
 * ============================================================ */

void python_repl(void) {
    /* Değişkenleri temizle */
    memset(py_vars, 0, sizeof(py_vars));

    video_clear(0);

    /* Başlık çubuğu */
    video_fill_rect(0, 0, SCREEN_WIDTH, 14, 1);
    video_print("cofeuPython 1.0  (Ctrl+C / exit() = Cik)", 4, 2, 14);

    cursor_x = 4;
    cursor_y = 18;

    py_print_str("cofeuOS Python 1.0 - Hazir!", 10);  py_newline();
    py_print_str("Ornek: x = 5  print(x)  if x>3: print(\"evet\")", 7); py_newline();
    py_newline();

    char line[128];

    while (1) {
        /* Prompt */
        py_print_str(">>> ", 11);

        /* Satır oku */
        py_readline(line, sizeof(line));

        if (line[0] == '\0') continue;

        /* Çalıştır */
        const char* err = py_exec(line);
        if (err) {
            if (strncmp(err, "EXIT", 4) == 0) break;
            py_print_str(err, 12);
            py_newline();
        }
    }

    video_clear(0);
}
