/*
 * JS.C - CofeuTarayici JavaScript Motoru (Faz 1: saf JS, DOM yok)
 *
 * Hicbir libc bagimliligi yoktur; bellek web_malloc/web_free (kernel
 * arenasi) uzerindendir ve sayfa degisiminde js_reset ile toplu
 * serbest birakilir. Aritmetik cift duyarlikli (double) SSE'dir.
 */
#include "../include/js.h"
#include "../include/web.h"
#include "../include/string.h"

/* Freestanding EFI: kayan nokta kullanimi CRT'nin _fltused sembolune
   baglidir; SSE aritmetigi icin deger onemsiz, sadece tanimli olmali. */
int _fltused = 0;

/* ─── Guc: mimari sinirlar ──────────────────────────────── */
#define JS_MAX_PARSE_DEPTH 400
#define JS_MAX_EVAL_DEPTH  600
#define JS_MAX_ARGS        64
#define JS_MAX_FRAC_DIGITS 6
#define JS_SB_INIT         64

#define JS_ISNAN(x) ((x) != (x))
#define JS_ISINF(x) ((x) == 1.0/0.0 || (x) == -1.0/0.0)
static double js_nan(void)   { return 0.0/0.0; }
static double js_inf(void)   { return 1.0/0.0; }

/* ─── Arena bellek katmani ──────────────────────────────── */
typedef struct js_block { void *ptr; struct js_block *next; } js_block_t;
static js_block_t *g_blocks;

static void *js_malloc(int n) {
    void *p = web_malloc((unsigned int)(n > 0 ? n : 1));
    if (!p) return NULL;
    js_block_t *b = (js_block_t*)web_malloc(sizeof(js_block_t));
    if (b) {
        b->ptr = p;
        b->next = g_blocks;
        g_blocks = b;
    }
    return p;
}

static char *js_strdup(const char *s) {
    int n = (int)strlen(s);
    char *p = (char*)js_malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static char *js_strndup(const char *s, int n) {
    char *p = (char*)js_malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

/* ─── Dinamik string olusturucu ─────────────────────────── */
typedef struct {
    char *buf;
    int len, cap;
} js_sb_t;

static void sb_init(js_sb_t *sb) {
    sb->buf = NULL;
    sb->len = 0;
    sb->cap = 0;
}

static void sb_grow(js_sb_t *sb, int need) {
    if (sb->len + need + 1 <= sb->cap) return;
    int ncap = sb->cap > 0 ? sb->cap : JS_SB_INIT;
    while (ncap < sb->len + need + 1) ncap *= 2;
    char *nb = (char*)js_malloc(ncap);
    if (!nb) return;
    if (sb->buf && sb->len > 0) memcpy(nb, sb->buf, sb->len);
    sb->buf = nb;
    sb->cap = ncap;
}

static void sb_putc(js_sb_t *sb, char c) {
    sb_grow(sb, 1);
    if (sb->buf) { sb->buf[sb->len++] = c; sb->buf[sb->len] = '\0'; }
}

static void sb_puts(js_sb_t *sb, const char *s) {
    if (!s) return;
    int n = (int)strlen(s);
    sb_grow(sb, n);
    if (!sb->buf) return;
    memcpy(sb->buf + sb->len, s, n);
    sb->len += n;
    sb->buf[sb->len] = '\0';
}

/* ─── Sayi → string / string → sayi ─────────────────────── */

static void sb_put_ll(js_sb_t *sb, long long v) {
    if (v == 0) { sb_putc(sb, '0'); return; }
    if (v < 0) { sb_putc(sb, '-'); if (v == (-9223372036854775807LL - 1)) { sb_put_ll(sb, 922337203685477580LL); sb_putc(sb, '8'); return; } v = -v; }
    char tmp[24];
    int n = 0;
    while (v > 0) { tmp[n++] = (char)('0' + v % 10); v /= 10; }
    while (n > 0) sb_putc(sb, tmp[--n]);
}

/* Buyuk ondalikli tamsayi kismi (long long'a sigmaz) */
static void sb_put_dint(js_sb_t *sb, double d) {
    if (d < 1.0) { sb_putc(sb, '0'); return; }
    /* long long'a girene kadar kucult, basamak sayisini tut */
    int e10 = 0;
    while (d >= 1e16 && e10 < 300) { d /= 10.0; e10++; }
    long long ip = (long long)d;
    char dig[32];
    int nd = 0;
    if (ip == 0) dig[nd++] = '0';
    while (ip > 0) { dig[nd++] = (char)('0' + ip % 10); ip /= 10; }
    for (int i = nd - 1; i >= 0; i--) sb_putc(sb, dig[i]);
    for (int i = 0; i < e10; i++) sb_putc(sb, '0');
}

static char *js_num_to_str(double d) {
    if (JS_ISNAN(d)) return js_strdup("NaN");
    if (JS_ISINF(d)) return js_strdup(d > 0 ? "Infinity" : "-Infinity");
    js_sb_t sb;
    sb_init(&sb);
    int neg = (d < 0.0) || (d == 0.0 && (1.0 / d) < 0.0);
    if (neg) d = -d;
    if (d < 9e15) {
        long long ip = (long long)d;
        double frac = d - (double)ip;
        sb_put_ll(&sb, ip);
        if (frac > 0.0) {
            sb_putc(&sb, '.');
            int digits[16];
            int n = 0;
            while (frac > 0.0 && n < JS_MAX_FRAC_DIGITS) {
                frac *= 10.0;
                int digit = (int)frac;
                digits[n++] = digit;
                frac -= (double)digit;
                if (frac < 1e-7 && frac > -1e-7) break;
            }
            while (n > 0 && digits[n - 1] == 0) n--;
            if (n == 0) { sb.len--; sb.buf[sb.len] = '\0'; }
            else for (int i = 0; i < n; i++) sb_putc(&sb, (char)('0' + digits[i]));
        }
    } else {
        sb_put_dint(&sb, d);
    }
    if (neg) {
        /* basina eksi ekle */
        int l = sb.len;
        char *nb = (char*)js_malloc(l + 2);
        if (!nb) return js_strdup("?");
        nb[0] = '-';
        memcpy(nb + 1, sb.buf, l);
        nb[l + 1] = '\0';
        return nb;
    }
    return sb.buf ? sb.buf : js_strdup("");
}

static double js_str_to_num(const char *s) {
    if (!s) return js_nan();
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    int sign = 1;
    if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
    double v = 0.0;
    int digits = 0;
    while (*s >= '0' && *s <= '9') { v = v * 10.0 + (*s - '0'); digits++; s++; }
    if (*s == '.') {
        s++;
        double f = 0.1;
        while (*s >= '0' && *s <= '9') {
            v += (*s - '0') * f;
            f *= 0.1;
            digits++;
            s++;
        }
    }
    if ((*s == 'e' || *s == 'E') && digits > 0) {
        s++;
        int esign = 1;
        if (*s == '+' || *s == '-') { if (*s == '-') esign = -1; s++; }
        int e = 0;
        while (*s >= '0' && *s <= '9') { e = e * 10 + (*s - '0'); s++; }
        double m = 1.0;
        for (int i = 0; i < e; i++) m *= 10.0;
        v = esign > 0 ? v * m : v / m;
    }
    if (digits == 0) return js_nan();
    return sign * v;
}

/* ─── Deger sistemi ─────────────────────────────────────── */

typedef enum {
    JV_UNDEF, JV_NULL, JV_NUM, JV_STR, JV_BOOL,
    JV_FUNC, JV_NATIVE, JV_OBJECT
} jv_type_t;

typedef struct js_value js_value_t;
typedef struct js_env js_env_t;
typedef struct js_ast js_ast_t;
typedef struct js_object js_object_t;
typedef struct js_prop js_prop_t;

typedef struct js_func {
    char *name;
    js_ast_t *body;
    char **params;
    int nparams;
    js_env_t *closure;
} js_func_t;

typedef js_value_t (*js_native_fn)(js_value_t *args, int nargs, void *ud);

struct js_value {
    jv_type_t type;
    double num;
    char *str;
    int  boolean;
    union {
        js_func_t *fn;
        struct { js_native_fn fn; void *ud; } nf;
        js_object_t *obj;
    } as;
};

struct js_object {
    int is_array;
    int is_host;    /* 0=yok, 1=element, 2=style, 3=location, 4=document */
    void *host;     /* web_node_t* veya NULL */
    js_prop_t *props;
    js_prop_t *tail;
};

struct js_prop {
    char *key;
    js_value_t value;
    struct js_prop *next;
};

typedef struct js_binding {
    char *name;
    js_value_t value;
    struct js_binding *next;
} js_binding_t;

struct js_env {
    js_binding_t *head;
    js_env_t *parent;
};

/* ─── Deger yapicilar ───────────────────────────────────── */
static js_value_t v_undef(void) { js_value_t v; memset(&v, 0, sizeof(v)); v.type = JV_UNDEF; return v; }
static js_value_t v_null(void)  { js_value_t v; memset(&v, 0, sizeof(v)); v.type = JV_NULL; return v; }
static js_value_t v_num(double d){ js_value_t v; memset(&v, 0, sizeof(v)); v.type = JV_NUM; v.num = d; return v; }
static js_value_t v_bool(int b) { js_value_t v; memset(&v, 0, sizeof(v)); v.type = JV_BOOL; v.boolean = b ? 1 : 0; return v; }
static js_value_t v_str(char *s) { js_value_t v; memset(&v, 0, sizeof(v)); v.type = JV_STR; v.str = s; return v; }
static js_value_t v_native(js_native_fn fn, void *ud) { js_value_t v; memset(&v, 0, sizeof(v)); v.type = JV_NATIVE; v.as.nf.fn = fn; v.as.nf.ud = ud; return v; }
static js_value_t v_object(js_object_t *o) { js_value_t v; memset(&v, 0, sizeof(v)); v.type = JV_OBJECT; v.as.obj = o; return v; }

static js_object_t *obj_new(int is_array) {
    js_object_t *o = (js_object_t*)js_malloc(sizeof(js_object_t));
    if (!o) return NULL;
    memset(o, 0, sizeof(js_object_t));
    o->is_array = is_array;
    return o;
}

static js_prop_t *obj_find(js_object_t *o, const char *key) {
    js_prop_t *p;
    if (!o || !key) return NULL;
    for (p = o->props; p; p = p->next)
        if (strcmp(p->key, key) == 0) return p;
    return NULL;
}

static void obj_set(js_object_t *o, const char *key, js_value_t v) {
    if (!o || !key) return;
    js_prop_t *p = obj_find(o, key);
    if (p) { p->value = v; return; }
    p = (js_prop_t*)js_malloc(sizeof(js_prop_t));
    if (!p) return;
    p->key = js_strdup(key);
    p->value = v;
    p->next = NULL;
    if (o->tail) { o->tail->next = p; o->tail = p; }
    else { o->props = p; o->tail = p; }
}

static js_value_t obj_get(js_object_t *o, const char *key) {
    js_prop_t *p = obj_find(o, key);
    if (p) return p->value;
    return v_undef();
}

static int is_truthy(js_value_t v) {
    switch (v.type) {
        case JV_UNDEF: case JV_NULL: return 0;
        case JV_NUM:   return v.num != 0.0 && !JS_ISNAN(v.num);
        case JV_STR:   return v.str && v.str[0] != '\0';
        case JV_BOOL:  return v.boolean;
        default:       return 1;
    }
}

static double to_number(js_value_t v) {
    switch (v.type) {
        case JV_NUM:   return v.num;
        case JV_BOOL:  return v.boolean ? 1.0 : 0.0;
        case JV_NULL:  return 0.0;
        case JV_UNDEF: return js_nan();
        case JV_STR:   return js_str_to_num(v.str);
        default:       return js_nan();
    }
}

static void value_to_sb(js_sb_t *sb, js_value_t v);

static char *to_string(js_value_t v) {
    js_sb_t sb;
    sb_init(&sb);
    value_to_sb(&sb, v);
    return sb.buf ? sb.buf : js_strdup("");
}

/* fmod (libm yok) */
static double fmod_js(double a, double b) {
    if (b == 0.0) return js_nan();
    long long iq = (long long)(a / b);
    double r = a - (double)iq * b;
    if (r != 0.0 && ((a < 0.0) != (b < 0.0))) r += b;
    return r;
}

/* Gevsek == (tip donusumlu) */
static int js_loose_eq(js_value_t l, js_value_t r) {
    if (l.type == r.type) {
        switch (l.type) {
            case JV_NUM:    return l.num == r.num;
            case JV_STR:    return strcmp(l.str ? l.str : "", r.str ? r.str : "") == 0;
            case JV_BOOL:   return l.boolean == r.boolean;
            case JV_NULL:
            case JV_UNDEF:  return 1;
            case JV_OBJECT: return l.as.obj == r.as.obj;
            default:        return 0;
        }
    }
    if ((l.type == JV_NULL && r.type == JV_UNDEF) || (l.type == JV_UNDEF && r.type == JV_NULL)) return 1;
    if (l.type == JV_NULL || l.type == JV_UNDEF || r.type == JV_NULL || r.type == JV_UNDEF) return 0;
    if (l.type == JV_STR && r.type == JV_NUM) return js_str_to_num(l.str) == r.num;
    if (l.type == JV_NUM && r.type == JV_STR) return l.num == js_str_to_num(r.str);
    if (l.type == JV_BOOL) return js_loose_eq(v_num(l.boolean ? 1.0 : 0.0), r);
    if (r.type == JV_BOOL) return js_loose_eq(l, v_num(r.boolean ? 1.0 : 0.0));
    return 0;
}

static void int_to_key(int n, char *buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = '\0'; return; }
    char tmp[12];
    int m = 0;
    while (n > 0) { tmp[m++] = (char)('0' + n % 10); n /= 10; }
    int k = 0;
    while (m > 0) buf[k++] = tmp[--m];
    buf[k] = '\0';
}

static void value_to_sb(js_sb_t *sb, js_value_t v) {
    switch (v.type) {
        case JV_NUM:   sb_puts(sb, js_num_to_str(v.num)); break;
        case JV_STR:   sb_puts(sb, v.str); break;
        case JV_BOOL:  sb_puts(sb, v.boolean ? "true" : "false"); break;
        case JV_NULL:  sb_puts(sb, "null"); break;
        case JV_UNDEF: sb_puts(sb, "undefined"); break;
        case JV_FUNC:  sb_puts(sb, v.as.fn->name ? v.as.fn->name : "function"); break;
        case JV_NATIVE: sb_puts(sb, "function"); break;
        case JV_OBJECT: {
            js_object_t *o = v.as.obj;
            if (!o) { sb_puts(sb, "[object Object]"); break; }
            if (o->is_array) {
                sb_putc(sb, '[');
                js_prop_t *p;
                int first = 1;
                for (p = o->props; p; p = p->next) {
                    if (strcmp(p->key, "length") == 0 || strcmp(p->key, "push") == 0) continue;
                    if (!first) sb_putc(sb, ',');
                    first = 0;
                    value_to_sb(sb, p->value);
                }
                sb_putc(sb, ']');
            } else {
                js_prop_t *p;
                int first = 1;
                sb_putc(sb, '{');
                for (p = o->props; p; p = p->next) {
                    if (!first) sb_putc(sb, ',');
                    first = 0;
                    sb_puts(sb, p->key);
                    sb_putc(sb, ':');
                    value_to_sb(sb, p->value);
                }
                sb_putc(sb, '}');
            }
            break;
        }
    }
}

/* ─── Ortamlar ──────────────────────────────────────────── */
static js_env_t *env_new(js_env_t *parent) {
    js_env_t *e = (js_env_t*)js_malloc(sizeof(js_env_t));
    if (!e) return NULL;
    memset(e, 0, sizeof(js_env_t));
    e->parent = parent;
    return e;
}

static void env_set(js_env_t *e, const char *name, js_value_t v) {
    js_binding_t *b;
    for (b = e->head; b; b = b->next)
        if (strcmp(b->name, name) == 0) { b->value = v; return; }
    b = (js_binding_t*)js_malloc(sizeof(js_binding_t));
    if (!b) return;
    b->name = js_strdup(name);
    b->value = v;
    b->next = e->head;
    e->head = b;
}

static int env_get(js_env_t *e, const char *name, js_value_t *out) {
    while (e) {
        js_binding_t *b;
        for (b = e->head; b; b = b->next)
            if (strcmp(b->name, name) == 0) { *out = b->value; return 1; }
        e = e->parent;
    }
    return 0;
}

/* ─── Tokenizer ─────────────────────────────────────────── */

typedef enum {
    TK_EOF, TK_NUM, TK_STR, TK_IDENT,
    TK_LP, TK_RP, TK_LB, TK_RB, TK_LC, TK_RC,
    TK_SEMI, TK_COMMA, TK_DOT, TK_COLON, TK_QUESTION,
    TK_ASSIGN, TK_EQ, TK_NE, TK_LT, TK_LE, TK_GT, TK_GE,
    TK_AND, TK_OR, TK_NOT,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_PLUSPLUS, TK_MINUSMINUS,
    TK_PLUS_EQ, TK_MINUS_EQ, TK_STAR_EQ, TK_SLASH_EQ
} tk_type_t;

typedef struct {
    int type;
    double num;
    char *str;
    int line;
} token_t;

typedef struct {
    const char *src;
    int pos, len, line;
} lexer_t;

static int lx_peek(lexer_t *lx) { return lx->pos < lx->len ? (unsigned char)lx->src[lx->pos] : 0; }
static int lx_peek2(lexer_t *lx) { return lx->pos + 1 < lx->len ? (unsigned char)lx->src[lx->pos + 1] : 0; }

static void lx_adv(lexer_t *lx) {
    if (lx->pos < lx->len) {
        if (lx->src[lx->pos] == '\n') lx->line++;
        lx->pos++;
    }
}

static void lex_skip(lexer_t *lx) {
    for (;;) {
        char c = (char)lx_peek(lx);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { lx_adv(lx); continue; }
        if (c == '/' && lx_peek2(lx) == '/') {
            while (lx_peek(lx) && lx_peek(lx) != '\n') lx_adv(lx);
            continue;
        }
        if (c == '/' && lx_peek2(lx) == '*') {
            lx_adv(lx); lx_adv(lx);
            while (lx_peek(lx)) {
                if (lx_peek(lx) == '*' && lx_peek2(lx) == '/') { lx_adv(lx); lx_adv(lx); break; }
                lx_adv(lx);
            }
            continue;
        }
        break;
    }
}

static int is_ident_start(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}
static int is_ident_char(int c) {
    return is_ident_start(c) || (c >= '0' && c <= '9');
}
static int is_digit(int c) { return c >= '0' && c <= '9'; }

static token_t lex_next(lexer_t *lx) {
    token_t t;
    memset(&t, 0, sizeof(t));
    lex_skip(lx);
    t.line = lx->line;
    int c = lx_peek(lx);

    if (c == 0) { t.type = TK_EOF; return t; }

    if (is_digit(c) || (c == '.' && is_digit(lx_peek2(lx)))) {
        int start = lx->pos;
        while (is_digit(lx_peek(lx))) lx_adv(lx);
        if (lx_peek(lx) == '.') { lx_adv(lx); while (is_digit(lx_peek(lx))) lx_adv(lx); }
        if ((lx_peek(lx) == 'e' || lx_peek(lx) == 'E')) {
            int save = lx->pos;
            lx_adv(lx);
            if (lx_peek(lx) == '+' || lx_peek(lx) == '-') lx_adv(lx);
            if (is_digit(lx_peek(lx))) { while (is_digit(lx_peek(lx))) lx_adv(lx); }
            else lx->pos = save;
        }
        char *tmp = js_strndup(lx->src + start, lx->pos - start);
        t.type = TK_NUM;
        t.num = js_str_to_num(tmp);
        return t;
    }

    if (is_ident_start(c)) {
        int start = lx->pos;
        while (is_ident_char(lx_peek(lx))) lx_adv(lx);
        t.type = TK_IDENT;
        t.str = js_strndup(lx->src + start, lx->pos - start);
        return t;
    }

    if (c == '\'' || c == '"') {
        int quote = c;
        lx_adv(lx);
        js_sb_t sb;
        sb_init(&sb);
        while (lx_peek(lx) && lx_peek(lx) != quote) {
            int ch = lx_peek(lx);
            if (ch == '\\') {
                lx_adv(lx);
                int e = lx_peek(lx);
                switch (e) {
                    case 'n': sb_putc(&sb, '\n'); break;
                    case 't': sb_putc(&sb, '\t'); break;
                    case 'r': sb_putc(&sb, '\r'); break;
                    case '0': sb_putc(&sb, '\0'); break;
                    case '\\': sb_putc(&sb, '\\'); break;
                    case '\'': sb_putc(&sb, '\''); break;
                    case '"': sb_putc(&sb, '"'); break;
                    default: sb_putc(&sb, (char)e); break;
                }
                lx_adv(lx);
            } else {
                sb_putc(&sb, (char)ch);
                lx_adv(lx);
            }
        }
        if (lx_peek(lx) == quote) lx_adv(lx);
        t.type = TK_STR;
        t.str = sb.buf ? sb.buf : js_strdup("");
        return t;
    }

    lx_adv(lx);
    switch (c) {
        case '(': t.type = TK_LP; break;
        case ')': t.type = TK_RP; break;
        case '[': t.type = TK_LB; break;
        case ']': t.type = TK_RB; break;
        case '{': t.type = TK_LC; break;
        case '}': t.type = TK_RC; break;
        case ';': t.type = TK_SEMI; break;
        case ',': t.type = TK_COMMA; break;
        case ':': t.type = TK_COLON; break;
        case '?': t.type = TK_QUESTION; break;
        case '.': t.type = TK_DOT; break;
        case '+':
            if (lx_peek(lx) == '+') { lx_adv(lx); t.type = TK_PLUSPLUS; }
            else if (lx_peek(lx) == '=') { lx_adv(lx); t.type = TK_PLUS_EQ; }
            else t.type = TK_PLUS;
            break;
        case '-':
            if (lx_peek(lx) == '-') { lx_adv(lx); t.type = TK_MINUSMINUS; }
            else if (lx_peek(lx) == '=') { lx_adv(lx); t.type = TK_MINUS_EQ; }
            else t.type = TK_MINUS;
            break;
        case '*':
            if (lx_peek(lx) == '=') { lx_adv(lx); t.type = TK_STAR_EQ; }
            else t.type = TK_STAR;
            break;
        case '/':
            if (lx_peek(lx) == '=') { lx_adv(lx); t.type = TK_SLASH_EQ; }
            else t.type = TK_SLASH;
            break;
        case '%': t.type = TK_PERCENT; break;
        case '=':
            if (lx_peek(lx) == '=') { lx_adv(lx); t.type = TK_EQ; }
            else t.type = TK_ASSIGN;
            break;
        case '!':
            if (lx_peek(lx) == '=') { lx_adv(lx); t.type = TK_NE; }
            else t.type = TK_NOT;
            break;
        case '<':
            if (lx_peek(lx) == '=') { lx_adv(lx); t.type = TK_LE; }
            else t.type = TK_LT;
            break;
        case '>':
            if (lx_peek(lx) == '=') { lx_adv(lx); t.type = TK_GE; }
            else t.type = TK_GT;
            break;
        case '&':
            if (lx_peek(lx) == '&') { lx_adv(lx); t.type = TK_AND; }
            else t.type = TK_EOF;
            break;
        case '|':
            if (lx_peek(lx) == '|') { lx_adv(lx); t.type = TK_OR; }
            else t.type = TK_EOF;
            break;
        default:
            t.type = TK_EOF;
            break;
    }
    return t;
}

/* ─── AST ───────────────────────────────────────────────── */

typedef enum {
    A_NUM, A_STR, A_IDENT, A_BOOL, A_NULL, A_UNDEF, A_THIS,
    A_UNARY, A_BINARY, A_LOGICAL, A_ASSIGN, A_TERNARY, A_POSTFIX,
    A_CALL, A_MEMBER, A_INDEX, A_ARRAY, A_OBJECT, A_PAIR,
    A_VAR, A_IF, A_WHILE, A_FOR, A_BLOCK, A_FUNC, A_RETURN,
    A_BREAK, A_CONTINUE, A_SEQ, A_EMPTY
} ast_t;

struct js_ast {
    ast_t type;
    int line;
    double num;
    char *str;
    int ival;
    struct js_ast *a, *b, *c;
    struct js_ast *init, *cond, *step;
    struct js_ast *body;
    char **params;
    int nparams;
};

static js_ast_t *ast_new(ast_t type, int line) {
    js_ast_t *n = (js_ast_t*)js_malloc(sizeof(js_ast_t));
    if (!n) return NULL;
    memset(n, 0, sizeof(js_ast_t));
    n->type = type;
    n->line = line;
    return n;
}

static js_ast_t *ast_seq(js_ast_t *a, js_ast_t *b) {
    js_ast_t *n = ast_new(A_SEQ, a ? a->line : 0);
    if (!n) return b;
    n->a = a;
    n->b = b;
    return n;
}

/* ─── Parser ────────────────────────────────────────────── */

typedef struct {
    lexer_t lx;
    token_t cur;
    int depth;
    int error;
    char err[128];
} parser_t;

static int kw_is(token_t *t, const char *kw) {
    return t->type == TK_IDENT && strcmp(t->str, kw) == 0;
}

static void p_error(parser_t *p, const char *msg) {
    if (p->error) return;
    p->error = 1;
    int l = p->cur.line;
    int n = 0;
    char tmp[140];
    tmp[n++] = 's';
    tmp[n++] = 'a';
    tmp[n++] = 't';
    tmp[n++] = 'i';
    tmp[n++] = 'r';
    tmp[n++] = ' ';
    /* satir numarasi */
    char num[12];
    int nn = 0;
    int v = l;
    if (v == 0) num[nn++] = '0';
    while (v > 0) { num[nn++] = (char)('0' + v % 10); v /= 10; }
    while (nn > 0) tmp[n++] = num[--nn];
    tmp[n++] = ':';
    tmp[n++] = ' ';
    const char *m = msg;
    while (*m && n < 130) tmp[n++] = *m++;
    tmp[n] = '\0';
    memcpy(p->err, tmp, n + 1);
}

static void p_advance(parser_t *p) {
    p->cur = lex_next(&p->lx);
}

static int p_peek(parser_t *p) { return p->cur.type; }

static int p_check(parser_t *p, int type) {
    if (p_peek(p) == type) { p_advance(p); return 1; }
    return 0;
}

static int p_expect(parser_t *p, int type, const char *what) {
    if (p_peek(p) == type) { p_advance(p); return 1; }
    p_error(p, what);
    return 0;
}

static js_ast_t *parse_expr(parser_t *p);
static js_ast_t *parse_stmt(parser_t *p);
static js_ast_t *parse_block(parser_t *p);
static js_ast_t *parse_function(parser_t *p);

static js_ast_t *parse_primary(parser_t *p) {
    int line = p->cur.line;
    if (p_peek(p) == TK_NUM) {
        js_ast_t *n = ast_new(A_NUM, line);
        if (n) n->num = p->cur.num;
        p_advance(p);
        return n;
    }
    if (p_peek(p) == TK_STR) {
        js_ast_t *n = ast_new(A_STR, line);
        if (n) n->str = p->cur.str;
        p_advance(p);
        return n;
    }
    if (p_peek(p) == TK_IDENT) {
        const char *kw = p->cur.str;
        /* function () {} da bir ifadedir; onclick/setTimeout bunu kullanır. */
        if (strcmp(kw, "function") == 0) return parse_function(p);
        if (strcmp(kw, "true") == 0)  { js_ast_t *n = ast_new(A_BOOL, line); if (n) n->ival = 1; p_advance(p); return n; }
        if (strcmp(kw, "false") == 0) { js_ast_t *n = ast_new(A_BOOL, line); if (n) n->ival = 0; p_advance(p); return n; }
        if (strcmp(kw, "null") == 0)  { js_ast_t *n = ast_new(A_NULL, line); p_advance(p); return n; }
        if (strcmp(kw, "undefined") == 0) { js_ast_t *n = ast_new(A_UNDEF, line); p_advance(p); return n; }
        if (strcmp(kw, "this") == 0)  { js_ast_t *n = ast_new(A_THIS, line); p_advance(p); return n; }
        js_ast_t *n = ast_new(A_IDENT, line);
        if (n) n->str = p->cur.str;
        p_advance(p);
        return n;
    }
    if (p_check(p, TK_LP)) {
        js_ast_t *e = parse_expr(p);
        p_expect(p, TK_RP, "beklenen )");
        return e;
    }
    if (p_peek(p) == TK_LB) {
        p_advance(p);
        js_ast_t *list = NULL;
        if (p_peek(p) != TK_RB) {
            js_ast_t *first = parse_expr(p);
            list = first;
            while (p_check(p, TK_COMMA)) {
                if (p_peek(p) == TK_RB) break;
                list = ast_seq(list, parse_expr(p));
            }
        }
        p_expect(p, TK_RB, "beklenen ]");
        js_ast_t *n = ast_new(A_ARRAY, line);
        if (n) n->a = list;
        return n;
    }
    if (p_peek(p) == TK_LC) {
        p_advance(p);
        js_ast_t *list = NULL;
        if (p_peek(p) != TK_RC) {
            for (;;) {
                js_ast_t *pair = ast_new(A_PAIR, p->cur.line);
                if (!pair) break;
                if (p_peek(p) == TK_STR) { pair->str = p->cur.str; p_advance(p); }
                else if (p_peek(p) == TK_IDENT) { pair->str = p->cur.str; p_advance(p); }
                else if (p_peek(p) == TK_NUM) {
                    js_ast_t *kn = ast_new(A_NUM, p->cur.line);
                    if (kn) { pair->str = js_num_to_str(p->cur.num); }
                    p_advance(p);
                } else {
                    p_error(p, "beklenen nesne anahtari");
                    break;
                }
                p_expect(p, TK_COLON, "beklenen :");
                pair->a = parse_expr(p);
                list = list ? ast_seq(list, pair) : pair;
                if (!p_check(p, TK_COMMA)) break;
                if (p_peek(p) == TK_RC) break;
            }
        }
        p_expect(p, TK_RC, "beklenen }");
        js_ast_t *n = ast_new(A_OBJECT, line);
        if (n) n->a = list;
        return n;
    }
    p_error(p, "beklenen ifade");
    return NULL;
}

static js_ast_t *parse_args(parser_t *p) {
    js_ast_t *list = NULL;
    if (p_peek(p) == TK_RP) return NULL;
    js_ast_t *first = parse_expr(p);
    list = first;
    while (p_check(p, TK_COMMA)) {
        if (p_peek(p) == TK_RP) break;
        list = ast_seq(list, parse_expr(p));
    }
    return list;
}

static js_ast_t *parse_postfix(parser_t *p) {
    js_ast_t *e = parse_primary(p);
    if (!e) return NULL;
    for (;;) {
        if (p_peek(p) == TK_DOT) {
            p_advance(p);
            if (p_peek(p) != TK_IDENT) { p_error(p, "beklenen ozellik adi"); break; }
            js_ast_t *m = ast_new(A_MEMBER, p->cur.line);
            if (m) { m->a = e; m->str = p->cur.str; }
            p_advance(p);
            e = m;
        } else if (p_peek(p) == TK_LB) {
            p_advance(p);
            js_ast_t *idx = parse_expr(p);
            p_expect(p, TK_RB, "beklenen ]");
            js_ast_t *m = ast_new(A_INDEX, p->cur.line);
            if (m) { m->a = e; m->b = idx; }
            e = m;
        } else if (p_peek(p) == TK_LP) {
            p_advance(p);
            js_ast_t *args = parse_args(p);
            p_expect(p, TK_RP, "beklenen )");
            js_ast_t *c = ast_new(A_CALL, p->cur.line);
            if (c) { c->a = e; c->b = args; }
            e = c;
        } else if (p_peek(p) == TK_PLUSPLUS || p_peek(p) == TK_MINUSMINUS) {
            js_ast_t *pf = ast_new(A_POSTFIX, p->cur.line);
            if (pf) { pf->a = e; pf->ival = p_peek(p) == TK_PLUSPLUS ? 1 : -1; }
            p_advance(p);
            e = pf;
        } else {
            break;
        }
    }
    return e;
}

static js_ast_t *parse_unary(parser_t *p) {
    int line = p->cur.line;
    if (p_peek(p) == TK_NOT || p_peek(p) == TK_MINUS || p_peek(p) == TK_PLUS ||
        p_peek(p) == TK_PLUSPLUS || p_peek(p) == TK_MINUSMINUS) {
        int op = p_peek(p);
        p_advance(p);
        js_ast_t *operand = parse_unary(p);
        js_ast_t *n = ast_new(A_UNARY, line);
        if (n) { n->ival = op; n->a = operand; }
        return n;
    }
    return parse_postfix(p);
}

static int bin_prec(int type) {
    switch (type) {
        case TK_OR: return 1;
        case TK_AND: return 2;
        case TK_EQ: case TK_NE: return 3;
        case TK_LT: case TK_LE: case TK_GT: case TK_GE: return 4;
        case TK_PLUS: case TK_MINUS: return 5;
        case TK_STAR: case TK_SLASH: case TK_PERCENT: return 6;
        default: return 0;
    }
}

static int is_assign_op(int type) {
    return type == TK_ASSIGN || type == TK_PLUS_EQ || type == TK_MINUS_EQ ||
           type == TK_STAR_EQ || type == TK_SLASH_EQ;
}

static js_ast_t *parse_binary(parser_t *p, int min_prec) {
    js_ast_t *left = parse_unary(p);
    if (!left) return NULL;
    for (;;) {
        int op = p_peek(p);
        int prec = bin_prec(op);
        if (prec == 0 || prec < min_prec) break;
        p_advance(p);
        js_ast_t *right = parse_binary(p, prec + 1);
        js_ast_t *n = ast_new((op == TK_AND || op == TK_OR) ? A_LOGICAL : A_BINARY, p->cur.line);
        if (n) { n->a = left; n->b = right; n->ival = op; }
        left = n;
    }
    return left;
}

static js_ast_t *parse_ternary(parser_t *p) {
    js_ast_t *cond = parse_binary(p, 1);
    if (!cond) return NULL;
    if (p_peek(p) == TK_QUESTION) {
        int line = p->cur.line;
        p_advance(p);
        js_ast_t *then_e = parse_ternary(p);
        p_expect(p, TK_COLON, "beklenen :");
        js_ast_t *else_e = parse_ternary(p);
        js_ast_t *n = ast_new(A_TERNARY, line);
        if (n) { n->a = cond; n->b = then_e; n->c = else_e; }
        return n;
    }
    return cond;
}

static js_ast_t *parse_assignment(parser_t *p) {
    js_ast_t *left = parse_ternary(p);
    if (!left) return NULL;
    if (is_assign_op(p_peek(p))) {
        int op = p_peek(p);
        p_advance(p);
        js_ast_t *right = parse_assignment(p);
        js_ast_t *n = ast_new(A_ASSIGN, p->cur.line);
        if (n) { n->a = left; n->b = right; n->ival = op; }
        return n;
    }
    return left;
}

static js_ast_t *parse_expr(parser_t *p) {
    return parse_assignment(p);
}

/* Fonksiyon bildirimi: function name(p1,p2){ body } */
static js_ast_t *parse_function(parser_t *p) {
    int line = p->cur.line;
    p_advance(p);   /* 'function' */
    js_ast_t *n = ast_new(A_FUNC, line);
    if (!n) return NULL;
    if (p_peek(p) == TK_IDENT) { n->str = p->cur.str; p_advance(p); }
    p_expect(p, TK_LP, "beklenen (");
    int maxp = 0;
    char **params = NULL;
    if (p_peek(p) != TK_RP) {
        for (;;) {
            if (p_peek(p) != TK_IDENT) { p_error(p, "beklenen parametre adi"); break; }
            char **np = (char**)js_malloc((maxp + 1) * sizeof(char*));
            if (np) {
                if (params) memcpy(np, params, maxp * sizeof(char*));
                np[maxp] = p->cur.str;
            }
            params = np;
            maxp++;
            p_advance(p);
            if (!p_check(p, TK_COMMA)) break;
            if (p_peek(p) == TK_RP) break;
        }
    }
    p_expect(p, TK_RP, "beklenen )");
    n->params = params;
    n->nparams = maxp;
    js_ast_t *body = parse_block(p);
    n->body = body;
    return n;
}

static js_ast_t *parse_block(parser_t *p) {
    if (!p_expect(p, TK_LC, "beklenen {")) return NULL;
    js_ast_t *list = NULL;
    while (p_peek(p) != TK_RC && p_peek(p) != TK_EOF) {
        js_ast_t *s = parse_stmt(p);
        if (!s) break;
        if (s->type != A_EMPTY) list = list ? ast_seq(list, s) : s;
    }
    p_expect(p, TK_RC, "beklenen }");
    return list;
}

static js_ast_t *parse_var(parser_t *p) {
    p_advance(p);   /* var/let/const */
    js_ast_t *list = NULL;
    for (;;) {
        js_ast_t *v = ast_new(A_VAR, p->cur.line);
        if (!v) break;
        if (p_peek(p) != TK_IDENT) { p_error(p, "beklenen degisken adi"); break; }
        v->str = p->cur.str;
        p_advance(p);
        if (p_peek(p) == TK_ASSIGN) {
            p_advance(p);
            v->a = parse_assignment(p);
        }
        list = list ? ast_seq(list, v) : v;
        if (!p_check(p, TK_COMMA)) break;
    }
    return list;
}

static js_ast_t *parse_stmt(parser_t *p) {
    int line = p->cur.line;
    if (p_peek(p) == TK_SEMI) { p_advance(p); return ast_new(A_EMPTY, line); }
    if (p_peek(p) == TK_LC) return parse_block(p);

    if (p_peek(p) == TK_IDENT) {
        const char *kw = p->cur.str;
        if (strcmp(kw, "var") == 0 || strcmp(kw, "let") == 0 || strcmp(kw, "const") == 0)
            return parse_var(p);
        if (strcmp(kw, "function") == 0)
            return parse_function(p);
        if (strcmp(kw, "if") == 0) {
            p_advance(p);
            p_expect(p, TK_LP, "beklenen (");
            js_ast_t *cond = parse_expr(p);
            p_expect(p, TK_RP, "beklenen )");
            js_ast_t *then_s = parse_stmt(p);
            js_ast_t *else_s = NULL;
            if (p_peek(p) == TK_IDENT && strcmp(p->cur.str, "else") == 0) {
                p_advance(p);
                else_s = parse_stmt(p);
            }
            js_ast_t *n = ast_new(A_IF, line);
            if (n) { n->a = cond; n->b = then_s; n->c = else_s; }
            return n;
        }
        if (strcmp(kw, "while") == 0) {
            p_advance(p);
            p_expect(p, TK_LP, "beklenen (");
            js_ast_t *cond = parse_expr(p);
            p_expect(p, TK_RP, "beklenen )");
            js_ast_t *body = parse_stmt(p);
            js_ast_t *n = ast_new(A_WHILE, line);
            if (n) { n->a = cond; n->b = body; }
            return n;
        }
        if (strcmp(kw, "for") == 0) {
            p_advance(p);
            p_expect(p, TK_LP, "beklenen (");
            js_ast_t *init = NULL, *cond = NULL, *step = NULL;
            if (p_peek(p) != TK_SEMI) {
                if (p_peek(p) == TK_IDENT && (kw_is(&p->cur, "var") || kw_is(&p->cur, "let") || kw_is(&p->cur, "const")))
                    init = parse_var(p);
                else
                    init = parse_expr(p);
            }
            p_expect(p, TK_SEMI, "beklenen ;");
            if (p_peek(p) != TK_SEMI) cond = parse_expr(p);
            p_expect(p, TK_SEMI, "beklenen ;");
            if (p_peek(p) != TK_RP) step = parse_expr(p);
            p_expect(p, TK_RP, "beklenen )");
            js_ast_t *body = parse_stmt(p);
            js_ast_t *n = ast_new(A_FOR, line);
            if (n) { n->init = init; n->cond = cond; n->step = step; n->body = body; }
            return n;
        }
        if (strcmp(kw, "return") == 0) {
            p_advance(p);
            js_ast_t *n = ast_new(A_RETURN, line);
            if (n && p_peek(p) != TK_SEMI && p_peek(p) != TK_RC && p_peek(p) != TK_EOF)
                n->a = parse_expr(p);
            p_check(p, TK_SEMI);
            return n;
        }
        if (strcmp(kw, "break") == 0) {
            p_advance(p);
            p_check(p, TK_SEMI);
            return ast_new(A_BREAK, line);
        }
        if (strcmp(kw, "continue") == 0) {
            p_advance(p);
            p_check(p, TK_SEMI);
            return ast_new(A_CONTINUE, line);
        }
    }

    js_ast_t *e = parse_expr(p);
    if (p_peek(p) == TK_SEMI) p_advance(p);
    return e;
}

static js_ast_t *parse_program(parser_t *p) {
    js_ast_t *list = NULL;
    while (p_peek(p) != TK_EOF) {
        js_ast_t *s = parse_stmt(p);
        if (!s) break;
        if (s->type != A_EMPTY) list = list ? ast_seq(list, s) : s;
    }
    return list ? list : ast_new(A_EMPTY, p->cur.line);
}

/* ─── Yorumlayici ───────────────────────────────────────── */

static js_env_t *g_globals;
static int g_ctrl;              /* 0 yok, 1 return, 2 break, 3 continue */
static js_value_t g_retval;
static js_log_fn   g_log_cb;
static js_alert_fn g_alert_cb;
static js_prompt_fn g_prompt_cb;
static js_confirm_fn g_confirm_cb;
static int g_eval_depth;

/* ─── DOM baglantisi (Faz 2) ────────────────────────────── */
static web_document_t *g_doc;
static web_css_rule_t *g_css;
static char g_cur_url[1024];
static char g_pending_nav[1024];

typedef struct js_evt {
    web_node_t *node;
    js_value_t fn;
    char *type;              /* NULL = onclick, aksi halde addEventListener tipi */
    struct js_evt *next;
} js_evt_t;
static js_evt_t *g_evts;

typedef struct js_timer {
    unsigned int due_ms;
    js_value_t fn;
    struct js_timer *next;
} js_timer_t;
static js_timer_t *g_timers;
static unsigned int g_now_ms;

static void js_log(const char *msg) {
    if (g_log_cb) g_log_cb(msg);
}

static js_value_t eval(js_ast_t *n, js_env_t *env);
static js_value_t js_native_array_push(js_value_t *args, int nargs, void *ud);
static js_value_t js_native_add_event_listener(js_value_t *args, int nargs, void *ud);
static js_value_t js_native_remove_event_listener(js_value_t *args, int nargs, void *ud);
static js_value_t js_native_set_timeout(js_value_t *args, int nargs, void *ud);
static js_value_t js_native_clear_timeout(js_value_t *args, int nargs, void *ud);
static int dom_get_prop(js_object_t *o, const char *key, js_value_t *out);
static int dom_set_prop(js_object_t *o, const char *key, js_value_t v);

static js_value_t get_member(js_value_t base, const char *key) {
    if (base.type == JV_OBJECT && base.as.obj) {
        js_object_t *o = base.as.obj;
        if (o->is_host) {
            js_value_t out;
            if (dom_get_prop(o, key, &out)) return out;
        }
        return obj_get(o, key);
    }
    return v_undef();
}

static void set_member(js_value_t *base, const char *key, js_value_t v) {
    if (base->type == JV_OBJECT && base->as.obj) {
        js_object_t *o = base->as.obj;
        if (o->is_host) {
            if (dom_set_prop(o, key, v)) return;
        }
        obj_set(o, key, v);
        return;
    }
    /* degistirilemez */
}

/* Atama hedefinin gecerli degerini okur */
static js_value_t assign_read(js_ast_t *target, js_env_t *env) {
    if (target->type == A_IDENT) {
        js_value_t v;
        if (env_get(env, target->str, &v)) return v;
        return v_undef();
    }
    if (target->type == A_MEMBER) {
        js_value_t base = eval(target->a, env);
        return get_member(base, target->str);
    }
    if (target->type == A_INDEX) {
        js_value_t base = eval(target->a, env);
        js_value_t idx = eval(target->b, env);
        char *key = to_string(idx);
        return get_member(base, key);
    }
    return v_undef();
}

static void assign_write(js_ast_t *target, js_env_t *env, js_value_t v) {
    if (target->type == A_IDENT) {
        js_value_t old;
        if (!env_get(env, target->str, &old)) {
            if (g_globals) env_set(g_globals, target->str, v);
            return;
        }
        /* kapsamdaki baglamayi guncelle */
        js_env_t *e = env;
        while (e) {
            js_binding_t *b;
            for (b = e->head; b; b = b->next)
                if (strcmp(b->name, target->str) == 0) { b->value = v; return; }
            e = e->parent;
        }
        env_set(env, target->str, v);
        return;
    }
    if (target->type == A_MEMBER) {
        js_value_t base = eval(target->a, env);
        set_member(&base, target->str, v);
        return;
    }
    if (target->type == A_INDEX) {
        js_value_t base = eval(target->a, env);
        js_value_t idx = eval(target->b, env);
        char *key = to_string(idx);
        set_member(&base, key, v);
        return;
    }
}

static js_value_t eval(js_ast_t *n, js_env_t *env) {
    if (!n) return v_undef();
    if (g_eval_depth > JS_MAX_EVAL_DEPTH) {
        g_eval_depth = 0;
        js_log("JS hatasi: yineleme siniri asildi");
        g_ctrl = 1;
        g_retval = v_undef();
        return v_undef();
    }
    g_eval_depth++;

    js_value_t result = v_undef();

    switch (n->type) {
        case A_NUM:   result = v_num(n->num); break;
        case A_STR:   result = v_str(n->str); break;
        case A_BOOL:  result = v_bool(n->ival); break;
        case A_NULL:  result = v_null(); break;
        case A_UNDEF: result = v_undef(); break;
        case A_THIS:  result = v_undef(); break;
        case A_IDENT: {
            js_value_t v;
            if (env_get(env, n->str, &v)) result = v;
            else {
                result = v_undef();
            }
            break;
        }
        case A_EMPTY: result = v_undef(); break;
        case A_SEQ: {
            eval(n->a, env);
            if (g_ctrl) break;
            result = eval(n->b, env);
            break;
        }
        case A_VAR: {
            js_value_t v = n->a ? eval(n->a, env) : v_undef();
            env_set(env, n->str, v);
            result = v;
            break;
        }
        case A_UNARY: {
            js_value_t opv = eval(n->a, env);
            switch (n->ival) {
                case TK_NOT: result = v_bool(!is_truthy(opv)); break;
                case TK_PLUS: result = v_num(to_number(opv)); break;
                case TK_MINUS: result = v_num(-to_number(opv)); break;
                case TK_PLUSPLUS: case TK_MINUSMINUS: {
                    double delta = n->ival == TK_PLUSPLUS ? 1.0 : -1.0;
                    js_value_t cur = assign_read(n->a, env);
                    js_value_t nv = v_num(to_number(cur) + delta);
                    assign_write(n->a, env, nv);
                    result = nv;
                    break;
                }
                default: break;
            }
            break;
        }
        case A_POSTFIX: {
            js_value_t cur = assign_read(n->a, env);
            double delta = n->ival > 0 ? 1.0 : -1.0;
            js_value_t nv = v_num(to_number(cur) + delta);
            assign_write(n->a, env, nv);
            result = cur;
            break;
        }
        case A_BINARY: {
            js_value_t l = eval(n->a, env);
            js_value_t r = eval(n->b, env);
            switch (n->ival) {
                case TK_PLUS: {
                    if (l.type == JV_STR || r.type == JV_STR) {
                        js_sb_t sb;
                        sb_init(&sb);
                        value_to_sb(&sb, l);
                        value_to_sb(&sb, r);
                        result = v_str(sb.buf ? sb.buf : js_strdup(""));
                    } else {
                        result = v_num(to_number(l) + to_number(r));
                    }
                    break;
                }
                case TK_MINUS: result = v_num(to_number(l) - to_number(r)); break;
                case TK_STAR:  result = v_num(to_number(l) * to_number(r)); break;
                case TK_SLASH: {
                    double rn = to_number(r);
                    if (rn == 0.0) result = v_num(rn == 0.0 && 1.0 / rn < 0 ? -js_inf() : js_inf());
                    else result = v_num(to_number(l) / rn);
                    break;
                }
                case TK_PERCENT: {
                    double rn = to_number(r);
                    if (rn == 0.0) result = v_num(js_nan());
                    else result = v_num(fmod_js(to_number(l), rn));
                    break;
                }
                case TK_EQ: result = v_bool(js_loose_eq(l, r)); break;
                case TK_NE: result = v_bool(!js_loose_eq(l, r)); break;
                case TK_LT: result = v_bool(to_number(l) < to_number(r)); break;
                case TK_LE: result = v_bool(to_number(l) <= to_number(r)); break;
                case TK_GT: result = v_bool(to_number(l) > to_number(r)); break;
                case TK_GE: result = v_bool(to_number(l) >= to_number(r)); break;
                default: break;
            }
            break;
        }
        case A_LOGICAL: {
            js_value_t l = eval(n->a, env);
            if (n->ival == TK_AND) {
                result = is_truthy(l) ? eval(n->b, env) : l;
            } else {
                result = is_truthy(l) ? l : eval(n->b, env);
            }
            break;
        }
        case A_TERNARY: {
            js_value_t c = eval(n->a, env);
            result = is_truthy(c) ? eval(n->b, env) : eval(n->c, env);
            break;
        }
        case A_ASSIGN: {
            js_value_t rhs = eval(n->b, env);
            if (n->ival == TK_ASSIGN) {
                assign_write(n->a, env, rhs);
                result = rhs;
            } else {
                js_value_t cur = assign_read(n->a, env);
                js_value_t nv = v_undef();
                if (n->ival == TK_PLUS_EQ) {
                    if (cur.type == JV_STR || rhs.type == JV_STR) {
                        js_sb_t sb; sb_init(&sb);
                        value_to_sb(&sb, cur);
                        value_to_sb(&sb, rhs);
                        nv = v_str(sb.buf ? sb.buf : js_strdup(""));
                    } else nv = v_num(to_number(cur) + to_number(rhs));
                } else if (n->ival == TK_MINUS_EQ) nv = v_num(to_number(cur) - to_number(rhs));
                else if (n->ival == TK_STAR_EQ)  nv = v_num(to_number(cur) * to_number(rhs));
                else if (n->ival == TK_SLASH_EQ) nv = v_num(to_number(cur) / to_number(rhs));
                assign_write(n->a, env, nv);
                result = nv;
            }
            break;
        }
        case A_MEMBER: {
            js_value_t base = eval(n->a, env);
            result = get_member(base, n->str);
            break;
        }
        case A_INDEX: {
            js_value_t base = eval(n->a, env);
            js_value_t idx = eval(n->b, env);
            char *key = to_string(idx);
            result = get_member(base, key);
            break;
        }
        case A_ARRAY: {
            js_object_t *o = obj_new(1);
            if (!o) break;
            int len = 0;
            js_ast_t *it = n->a;
            while (it && it->type == A_SEQ) {
                js_value_t v = eval(it->a, env);
                char key[12];
                int_to_key(len, key);
                obj_set(o, key, v);
                len++;
                it = it->b;
            }
            if (it && it->type != A_SEQ) {
                js_value_t v = eval(it, env);
                char key[12];
                int_to_key(len, key);
                obj_set(o, key, v);
                len++;
            }
            obj_set(o, "length", v_num((double)len));
            obj_set(o, "push", v_native(js_native_array_push, o));
            result = v_object(o);
            break;
        }
        case A_OBJECT: {
            js_object_t *o = obj_new(0);
            if (!o) break;
            js_ast_t *it = n->a;
            while (it) {
                js_ast_t *pair = it->type == A_SEQ ? it->a : it;
                if (pair && pair->type == A_PAIR) {
                    js_value_t v = eval(pair->a, env);
                    obj_set(o, pair->str, v);
                }
                it = (it->type == A_SEQ) ? it->b : NULL;
            }
            result = v_object(o);
            break;
        }
        case A_CALL: {
            js_value_t callee = eval(n->a, env);
            js_value_t args[JS_MAX_ARGS];
            int nargs = 0;
            js_ast_t *it = n->b;
            while (it && it->type == A_SEQ && nargs < JS_MAX_ARGS) {
                args[nargs++] = eval(it->a, env);
                it = it->b;
            }
            if (it && it->type != A_SEQ && nargs < JS_MAX_ARGS)
                args[nargs++] = eval(it, env);

            if (callee.type == JV_NATIVE) {
                result = callee.as.nf.fn(args, nargs, callee.as.nf.ud);
            } else if (callee.type == JV_FUNC) {
                js_func_t *f = callee.as.fn;
                js_env_t *nenv = env_new(f->closure);
                if (!nenv) break;
                for (int i = 0; i < f->nparams; i++) {
                    js_value_t av = i < nargs ? args[i] : v_undef();
                    env_set(nenv, f->params[i], av);
                }
                int old_ctrl = g_ctrl;
                js_value_t old_ret = g_retval;
                g_ctrl = 0;
                eval(f->body, nenv);
                result = (g_ctrl == 1) ? g_retval : v_undef();
                g_ctrl = old_ctrl;
                g_retval = old_ret;
            } else {
                js_log("JS hatasi: deger fonksiyon degil");
                result = v_undef();
            }
            break;
        }
        case A_FUNC: {
            js_func_t *f = (js_func_t*)js_malloc(sizeof(js_func_t));
            if (f) {
                memset(f, 0, sizeof(js_func_t));
                f->name = n->str;
                f->body = n->body;
                f->params = n->params;
                f->nparams = n->nparams;
                f->closure = env;
            }
            js_value_t fv;
            memset(&fv, 0, sizeof(fv));
            fv.type = JV_FUNC;
            fv.as.fn = f;
            if (f && n->str) env_set(env, n->str, fv);
            result = fv;
            break;
        }
        case A_IF: {
            js_value_t c = eval(n->a, env);
            if (is_truthy(c)) eval(n->b, env);
            else if (n->c) eval(n->c, env);
            break;
        }
        case A_WHILE: {
            while (is_truthy(eval(n->a, env))) {
                eval(n->b, env);
                if (g_ctrl == 2) { g_ctrl = 0; break; }
                if (g_ctrl == 3) { g_ctrl = 0; continue; }
                if (g_ctrl == 1) break;
            }
            break;
        }
        case A_FOR: {
            if (n->init) eval(n->init, env);
            for (;;) {
                if (n->cond && !is_truthy(eval(n->cond, env))) break;
                eval(n->body, env);
                if (g_ctrl == 2) { g_ctrl = 0; break; }
                if (g_ctrl == 3) { g_ctrl = 0; }
                if (g_ctrl == 1) break;
                if (n->step) eval(n->step, env);
            }
            break;
        }
        case A_BLOCK: {
            eval(n->body, env);
            break;
        }
        case A_RETURN:
            g_ctrl = 1;
            g_retval = n->a ? eval(n->a, env) : v_undef();
            break;
        case A_BREAK:
            g_ctrl = 2;
            break;
        case A_CONTINUE:
            g_ctrl = 3;
            break;
        default:
            break;
    }

    g_eval_depth--;
    return result;
}

static js_value_t js_native_array_push(js_value_t *args, int nargs, void *ud);

static js_value_t js_native_add_event_listener(js_value_t *args, int nargs, void *ud) {
    web_node_t *node = (web_node_t*)ud;
    if (!node || nargs < 2 || args[0].type != JV_STR ||
        (args[1].type != JV_FUNC && args[1].type != JV_NATIVE)) return v_undef();
    js_evt_t *e = (js_evt_t*)js_malloc(sizeof(js_evt_t));
    if (!e) return v_undef();
    memset(e, 0, sizeof(*e));
    e->node = node;
    e->type = js_strdup(args[0].str);
    e->fn = args[1];
    e->next = g_evts;
    g_evts = e;
    return v_undef();
}

static js_value_t js_native_set_timeout(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    if (nargs < 1 || (args[0].type != JV_FUNC && args[0].type != JV_NATIVE)) return v_num(0);
    unsigned int delay = nargs > 1 && to_number(args[1]) > 0.0
                       ? (unsigned int)to_number(args[1]) : 0;
    js_timer_t *t = (js_timer_t*)js_malloc(sizeof(js_timer_t));
    if (!t) return v_num(0);
    t->due_ms = g_now_ms + delay;
    t->fn = args[0];
    t->next = g_timers;
    g_timers = t;
    return v_num((double)t->due_ms);
}

static js_value_t js_native_remove_event_listener(js_value_t *args, int nargs, void *ud) {
    web_node_t *node = (web_node_t*)ud;
    if (!node || nargs < 2 || args[0].type != JV_STR ||
        (args[1].type != JV_FUNC && args[1].type != JV_NATIVE)) return v_undef();
    const char *type = args[0].str;
    js_value_t fn = args[1];
    js_evt_t **p = &g_evts;
    while (*p) {
        js_evt_t *e = *p;
        if (e->node == node && e->type && strcmp_ci(e->type, type) == 0 && e->fn.type == fn.type) {
            if (e->fn.type == JV_FUNC) {
                if (e->fn.as.fn == fn.as.fn) {
                    *p = e->next;
                    web_free(e->type);
                    web_free(e);
                    return v_undef();
                }
            } else if (e->fn.type == JV_NATIVE) {
                if (e->fn.as.nf.fn == fn.as.nf.fn && e->fn.as.nf.ud == fn.as.nf.ud) {
                    *p = e->next;
                    web_free(e->type);
                    web_free(e);
                    return v_undef();
                }
            }
        }
        p = &e->next;
    }
    return v_undef();
}

static js_value_t js_native_clear_timeout(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    if (nargs < 1 || args[0].type != JV_NUM) return v_undef();
    unsigned int id = (unsigned int)args[0].num;
    js_timer_t **p = &g_timers;
    while (*p) {
        js_timer_t *t = *p;
        if ((unsigned int)t->due_ms == id) {
            *p = t->next;
            web_free(t);
            return v_undef();
        }
        p = &t->next;
    }
    return v_undef();
}

/* ─── Yerlesikler ───────────────────────────────────────── */
static js_value_t js_native_alert(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    char *msg = nargs > 0 ? to_string(args[0]) : js_strdup("undefined");
    if (g_alert_cb) g_alert_cb(msg);
    return v_undef();
}

static js_value_t js_native_prompt(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    char *msg = nargs > 0 ? to_string(args[0]) : js_strdup("");
    if (g_prompt_cb) {
        char buf[256];
        buf[0] = '\0';
        int ok = g_prompt_cb(msg, buf, (int)sizeof(buf));
        if (ok) return v_str(js_strdup(buf));
    }
    return v_null();
}

static js_value_t js_native_confirm(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    char *msg = nargs > 0 ? to_string(args[0]) : js_strdup("");
    if (g_confirm_cb) return v_bool(g_confirm_cb(msg));
    return v_bool(0);
}

static js_value_t js_console_log(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    js_sb_t sb;
    sb_init(&sb);
    for (int i = 0; i < nargs; i++) {
        if (i > 0) sb_putc(&sb, ' ');
        value_to_sb(&sb, args[i]);
    }
    js_log(sb.buf ? sb.buf : "");
    return v_undef();
}

static js_value_t js_console_error(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    js_sb_t sb;
    sb_init(&sb);
    for (int i = 0; i < nargs; i++) {
        if (i > 0) sb_putc(&sb, ' ');
        value_to_sb(&sb, args[i]);
    }
    js_log(sb.buf ? sb.buf : "");
    return v_undef();
}

static js_value_t js_native_parse_int(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    char *s = nargs > 0 ? to_string(args[0]) : js_strdup("NaN");
    int radix = nargs > 1 ? (int)to_number(args[1]) : 10;
    if (radix == 0) radix = 10;
    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
    int sign = 1;
    if (*s == '+' || *s == '-') { if (*s == '-') sign = -1; s++; }
    if (radix == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    double acc = 0.0;
    int any = 0;
    while (*s) {
        int d;
        if (*s >= '0' && *s <= '9') d = *s - '0';
        else if (*s >= 'a' && *s <= 'z') d = *s - 'a' + 10;
        else if (*s >= 'A' && *s <= 'Z') d = *s - 'A' + 10;
        else break;
        if (d >= radix) break;
        acc = acc * radix + d;
        any = 1;
        s++;
    }
    if (!any) return v_num(js_nan());
    return v_num(sign * acc);
}

static js_value_t js_native_parse_float(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    double d = nargs > 0 ? js_str_to_num(to_string(args[0])) : js_nan();
    return v_num(d);
}

static js_value_t js_native_isnan(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    return v_bool(JS_ISNAN(to_number(nargs > 0 ? args[0] : v_undef())));
}

static js_value_t js_native_number(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    if (nargs == 0) return v_num(0.0);
    return v_num(to_number(args[0]));
}

static js_value_t js_native_string(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    if (nargs == 0) return v_str(js_strdup(""));
    return v_str(to_string(args[0]));
}

static js_value_t js_native_boolean(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    return v_bool(is_truthy(nargs > 0 ? args[0] : v_undef()));
}

static double js_floor(double x) {
    if (x > -9e15 && x < 9e15) {
        long long i = (long long)x;
        if ((double)i > x) i--;
        return (double)i;
    }
    return x;
}
static double js_ceil(double x) {
    if (x > -9e15 && x < 9e15) {
        long long i = (long long)x;
        if ((double)i < x) i++;
        return (double)i;
    }
    return x;
}
static double js_sqrt(double x) {
    if (x < 0.0) return js_nan();
    if (x == 0.0) return 0.0;
    double g = x;
    for (int i = 0; i < 40; i++) g = (g + x / g) * 0.5;
    return g;
}
static double js_pow(double b, double e) {
    if (e == 0.0) return 1.0;
    long long ie = (long long)e;
    if ((double)ie == e) {
        double r = 1.0;
        long long n = ie < 0 ? -ie : ie;
        for (long long i = 0; i < n; i++) r *= b;
        return ie < 0 ? 1.0 / r : r;
    }
    return js_nan();
}

static js_value_t js_math_min(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    if (nargs == 0) return v_num(js_inf());
    double m = to_number(args[0]);
    for (int i = 1; i < nargs; i++) { double d = to_number(args[i]); if (d < m) m = d; }
    return v_num(m);
}
static js_value_t js_math_max(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    if (nargs == 0) return v_num(-js_inf());
    double m = to_number(args[0]);
    for (int i = 1; i < nargs; i++) { double d = to_number(args[i]); if (d > m) m = d; }
    return v_num(m);
}
static js_value_t js_math_floor(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    return v_num(js_floor(to_number(nargs > 0 ? args[0] : v_undef())));
}
static js_value_t js_math_ceil(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    return v_num(js_ceil(to_number(nargs > 0 ? args[0] : v_undef())));
}
static js_value_t js_math_round(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    return v_num(js_floor(to_number(nargs > 0 ? args[0] : v_undef()) + 0.5));
}
static js_value_t js_math_pow(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    double b = to_number(nargs > 0 ? args[0] : v_undef());
    double e = to_number(nargs > 1 ? args[1] : v_undef());
    return v_num(js_pow(b, e));
}
static js_value_t js_math_sqrt(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    return v_num(js_sqrt(to_number(nargs > 0 ? args[0] : v_undef())));
}
static js_value_t js_math_abs(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    double d = to_number(nargs > 0 ? args[0] : v_undef());
    return v_num(d < 0.0 ? -d : d);
}
static js_value_t js_math_random(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    (void)args; (void)nargs;
    static unsigned long long s = 0x123456789abcdef0ULL;
    s = s * 6364136223846793005ULL + 1442695040888963407ULL;
    double r = (double)(s >> 11) / (double)(1ULL << 53);
    return v_num(r);
}

static js_value_t js_native_array_push(js_value_t *args, int nargs, void *ud) {
    js_object_t *o = (js_object_t*)ud;
    if (!o || nargs == 0) return v_undef();
    js_value_t len = obj_get(o, "length");
    int n = (int)to_number(len);
    char key[12];
    int_to_key(n, key);
    obj_set(o, key, args[0]);
    obj_set(o, "length", v_num((double)(n + 1)));
    return v_num((double)(n + 1));
}

/* ════════════ DOM saricilari (Faz 2) ════════════
   document / window / element / style / location. Host objeler web.h'deki
   web_node_t agacini sarmalar; bellek web_malloc ailesinden gelir ve sayfa
   bosalirken (browser_free_page) serbest kalir. */

#define JS_HOST_ELEMENT   1
#define JS_HOST_STYLE     2
#define JS_HOST_LOCATION  3
#define JS_HOST_DOCUMENT  4

static js_value_t dom_element(web_node_t *n) {
    js_object_t *o = obj_new(0);
    if (!o) return v_undef();
    o->is_host = JS_HOST_ELEMENT;
    o->host = n;
    return v_object(o);
}

static char *js_web_strdup(const char *s) {
    int n = (int)strlen(s);
    char *p = (char*)web_malloc((unsigned int)(n + 1));
    if (!p) return NULL;
    memcpy(p, s, n + 1);
    return p;
}

static void node_detach(web_node_t *n) {
    if (!n || !n->parent) return;
    if (n->parent->first_child == n) n->parent->first_child = n->next_sibling;
    if (n->parent->last_child == n) n->parent->last_child = n->prev_sibling;
    if (n->prev_sibling) n->prev_sibling->next_sibling = n->next_sibling;
    if (n->next_sibling) n->next_sibling->prev_sibling = n->prev_sibling;
    n->parent = NULL;
    n->prev_sibling = NULL;
    n->next_sibling = NULL;
}

static void node_append(web_node_t *parent, web_node_t *n) {
    if (!parent || !n) return;
    node_detach(n);
    n->parent = parent;
    n->prev_sibling = parent->last_child;
    n->next_sibling = NULL;
    if (parent->last_child) parent->last_child->next_sibling = n;
    else parent->first_child = n;
    parent->last_child = n;
}

static web_attr_t *js_node_find_attr(web_node_t *n, const char *name) {
    for (web_attr_t *a = n->attrs; a; a = a->next)
        if (strcmp_ci(a->name, name) == 0) return a;
    return NULL;
}

static void js_node_set_attr(web_node_t *n, const char *name, const char *value) {
    if (!n) return;
    char *val = js_web_strdup(value ? value : "");
    web_attr_t *a = js_node_find_attr(n, name);
    if (a) {
        web_free(a->value);
        a->value = val;
    } else {
        web_attr_t *na = (web_attr_t*)web_malloc(sizeof(web_attr_t));
        if (!na) { web_free(val); return; }
        na->name = js_web_strdup(name);
        na->value = val;
        na->next = n->attrs;
        n->attrs = na;
    }
    if (strcmp_ci(name, "id") == 0) {
        if (n->id) web_free(n->id);
        n->id = js_web_strdup(value ? value : "");
    }
}

static void js_node_remove_attr(web_node_t *n, const char *name) {
    if (!n) return;
    web_attr_t **pp = &n->attrs;
    while (*pp) {
        if (strcmp_ci((*pp)->name, name) == 0) {
            web_attr_t *d = *pp;
            *pp = d->next;
            web_free(d->name);
            web_free(d->value);
            web_free(d);
            if (strcmp_ci(name, "id") == 0 && n->id) { web_free(n->id); n->id = NULL; }
            return;
        }
        pp = &(*pp)->next;
    }
}

static void collect_text(web_node_t *n, js_sb_t *sb) {
    for (web_node_t *c = n->first_child; c; c = c->next_sibling) {
        if (c->type == WEB_NODE_TEXT) sb_puts(sb, c->text ? c->text : "");
        else collect_text(c, sb);
    }
}

static void serialize_node(web_node_t *n, js_sb_t *sb) {
    if (n->type == WEB_NODE_TEXT) { sb_puts(sb, n->text ? n->text : ""); return; }
    if (n->type == WEB_NODE_COMMENT) return;
    sb_putc(sb, '<');
    sb_puts(sb, n->tag);
    for (web_attr_t *a = n->attrs; a; a = a->next) {
        sb_putc(sb, ' ');
        sb_puts(sb, a->name);
        sb_puts(sb, "=\"");
        sb_puts(sb, a->value);
        sb_putc(sb, '"');
    }
    sb_puts(sb, ">");
    for (web_node_t *c = n->first_child; c; c = c->next_sibling) serialize_node(c, sb);
    sb_puts(sb, "</");
    sb_puts(sb, n->tag);
    sb_putc(sb, '>');
}

static js_value_t dom_children(web_node_t *n) {
    js_object_t *arr = obj_new(1);
    if (!arr) return v_undef();
    int i = 0;
    for (web_node_t *c = n->first_child; c; c = c->next_sibling) {
        if (c->type != WEB_NODE_ELEMENT) continue;
        char key[12];
        int_to_key(i, key);
        obj_set(arr, key, dom_element(c));
        i++;
    }
    obj_set(arr, "length", v_num((double)i));
    return v_object(arr);
}

static void replace_children_with_text(web_node_t *n, const char *s) {
    while (n->first_child) node_detach(n->first_child);
    if (s && s[0]) {
        web_node_t *tn = (web_node_t*)web_malloc(sizeof(web_node_t));
        if (tn) {
            memset(tn, 0, sizeof(*tn));
            tn->type = WEB_NODE_TEXT;
            tn->text = js_web_strdup(s);
            node_append(n, tn);
        }
    }
}

static void inner_html_set(web_node_t *n, const char *s) {
    while (n->first_child) node_detach(n->first_child);
    if (!s || !s[0]) return;
    web_document_t *frag = web_parse_html(s, (unsigned int)strlen(s));
    if (frag && frag->root) {
        web_node_t *c = frag->root->first_child;
        while (c) {
            web_node_t *nx = c->next_sibling;
            node_detach(c);
            node_append(n, c);
            c = nx;
        }
    }
    if (frag) web_free_document(frag);
}

/* ─── Selector (tag / #id / .class) ─────────────────────── */
static void sel_parse(const char *sel, char *tag, int tagsz,
                      char *id, int idsz, char *cls, int clssz) {
    tag[0] = id[0] = cls[0] = '\0';
    char buf[128];
    strncpy(buf, sel, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char *p = buf;
    while (*p && *p != '#' && *p != '.') p++;
    char save = *p;
    *p = '\0';
    strncpy(tag, buf, (size_t)tagsz - 1);
    tag[tagsz - 1] = '\0';
    *p = save;
    while (*p) {
        if (*p == '#') {
            p++;
            char *e = p;
            while (*e && *e != '.') e++;
            save = *e;
            *e = '\0';
            strncpy(id, p, (size_t)idsz - 1);
            id[idsz - 1] = '\0';
            *e = save;
            p = e;
        } else if (*p == '.') {
            p++;
            char *e = p;
            while (*e && *e != '#') e++;
            save = *e;
            *e = '\0';
            strncpy(cls, p, (size_t)clssz - 1);
            cls[clssz - 1] = '\0';
            *e = save;
            p = e;
        } else p++;
    }
}

static int sel_matches(web_node_t *n, const char *tag, const char *id, const char *cls) {
    if (!n || n->type != WEB_NODE_ELEMENT) return 0;
    if (tag[0] && strcmp_ci(n->tag, tag) != 0) return 0;
    if (id[0] && (!n->id || strcmp(n->id, id) != 0)) return 0;
    if (cls[0] && !web_node_has_class(n, cls)) return 0;
    return 1;
}

static web_node_t *sel_first(web_node_t *n, const char *tag, const char *id, const char *cls) {
    if (!n) return NULL;
    if (sel_matches(n, tag, id, cls)) return n;
    for (web_node_t *c = n->first_child; c; c = c->next_sibling) {
        web_node_t *r = sel_first(c, tag, id, cls);
        if (r) return r;
    }
    return NULL;
}

/* ─── Stil objesi (JS_HOST_STYLE) ───────────────────────── */
static void hex6(char *out, u32 c) {
    static const char *h = "0123456789ABCDEF";
    out[0] = '#';
    out[1] = h[(c >> 20) & 15]; out[2] = h[(c >> 16) & 15];
    out[3] = h[(c >> 12) & 15]; out[4] = h[(c >> 8) & 15];
    out[5] = h[(c >> 4) & 15];  out[6] = h[c & 15];
    out[7] = '\0';
}

static void style_set_prop(web_node_t *n, const char *key, const char *value) {
    char csskey[32];
    if (strcmp_ci(key, "backgroundColor") == 0) strcpy(csskey, "background-color");
    else if (strcmp_ci(key, "fontSize") == 0) strcpy(csskey, "font-size");
    else if (strcmp_ci(key, "fontWeight") == 0) strcpy(csskey, "font-weight");
    else if (strcmp_ci(key, "textAlign") == 0) strcpy(csskey, "text-align");
    else { strncpy(csskey, key, sizeof(csskey) - 1); csskey[sizeof(csskey) - 1] = '\0'; }

    js_sb_t sb;
    sb_init(&sb);
    const char *inl = web_node_attr(n, "style");
    if (inl && inl[0]) {
        char buf[512];
        strncpy(buf, inl, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        char *tok = strtok(buf, ";");
        while (tok) {
            char *col = strchr(tok, ':');
            if (col) {
                *col = '\0';
                char *p = tok;
                while (*p == ' ') p++;
                char *pe = p + strlen(p);
                while (pe > p && pe[-1] == ' ') pe--;
                *pe = '\0';
                char *v = col + 1;
                while (*v == ' ') v++;
                if (strcmp_ci(p, csskey) != 0) {
                    sb_puts(&sb, p);
                    sb_puts(&sb, ": ");
                    sb_puts(&sb, v);
                    sb_puts(&sb, "; ");
                }
            }
            tok = strtok(NULL, ";");
        }
    }
    sb_puts(&sb, csskey);
    sb_puts(&sb, ": ");
    sb_puts(&sb, value);
    sb_puts(&sb, ";");
    js_node_set_attr(n, "style", sb.buf ? sb.buf : "");
}

static int dom_style_get_prop(js_object_t *o, const char *key, js_value_t *out) {
    web_node_t *n = (web_node_t*)o->host;
    if (!n) { *out = v_undef(); return 1; }
    web_style_t st;
    web_compute_style(n, g_css, &st);
    char buf[64];
    if (strcmp_ci(key, "color") == 0) { hex6(buf, st.color); *out = v_str(js_strdup(buf)); return 1; }
    if (strcmp_ci(key, "background") == 0 || strcmp_ci(key, "backgroundColor") == 0) {
        if (st.has_bg) hex6(buf, st.bg_color);
        else buf[0] = '\0';
        *out = v_str(js_strdup(buf)); return 1;
    }
    if (strcmp_ci(key, "fontSize") == 0) {
        *out = v_str(js_strdup(st.font_size == WEB_FONT_BIG ? "32px" : "16px")); return 1;
    }
    if (strcmp_ci(key, "fontWeight") == 0) {
        *out = v_str(js_strdup(st.font_bold ? "bold" : "normal")); return 1;
    }
    if (strcmp_ci(key, "display") == 0) {
        const char *d = st.display == WEB_DISPLAY_NONE ? "none"
                        : st.display == WEB_DISPLAY_INLINE ? "inline" : "block";
        *out = v_str(js_strdup(d)); return 1;
    }
    if (strcmp_ci(key, "textAlign") == 0) {
        const char *a = st.text_align == WEB_ALIGN_CENTER ? "center"
                        : st.text_align == WEB_ALIGN_RIGHT ? "right" : "left";
        *out = v_str(js_strdup(a)); return 1;
    }
    return 0;
}

static int dom_style_set_prop(js_object_t *o, const char *key, js_value_t v) {
    web_node_t *n = (web_node_t*)o->host;
    if (!n) return 1;
    char *s = to_string(v);
    style_set_prop(n, key, s ? s : "");
    return 1;
}

/* ─── Element natives ───────────────────────────────────── */
static js_value_t js_native_append_child(js_value_t *args, int nargs, void *ud) {
    web_node_t *parent = (web_node_t*)ud;
    if (nargs < 1 || args[0].type != JV_OBJECT || !args[0].as.obj ||
        args[0].as.obj->is_host != JS_HOST_ELEMENT)
        return v_undef();
    node_append(parent, (web_node_t*)args[0].as.obj->host);
    return args[0];
}

static js_value_t js_native_insert_before(js_value_t *args, int nargs, void *ud) {
    web_node_t *parent = (web_node_t*)ud;
    if (nargs < 1 || args[0].type != JV_OBJECT || !args[0].as.obj ||
        args[0].as.obj->is_host != JS_HOST_ELEMENT)
        return v_undef();
    web_node_t *child = (web_node_t*)args[0].as.obj->host;
    web_node_t *ref = NULL;
    if (nargs >= 2 && args[1].type == JV_OBJECT && args[1].as.obj &&
        args[1].as.obj->is_host == JS_HOST_ELEMENT)
        ref = (web_node_t*)args[1].as.obj->host;
    node_detach(child);
    if (ref) {
        child->parent = parent;
        child->prev_sibling = ref->prev_sibling;
        child->next_sibling = ref;
        if (ref->prev_sibling) ref->prev_sibling->next_sibling = child;
        else parent->first_child = child;
        ref->prev_sibling = child;
    } else {
        node_append(parent, child);
    }
    return args[0];
}

static js_value_t js_native_remove_child(js_value_t *args, int nargs, void *ud) {
    web_node_t *parent = (web_node_t*)ud;
    if (nargs < 1 || args[0].type != JV_OBJECT || !args[0].as.obj ||
        args[0].as.obj->is_host != JS_HOST_ELEMENT)
        return v_undef();
    web_node_t *child = (web_node_t*)args[0].as.obj->host;
    if (child->parent != parent) return v_undef();
    node_detach(child);
    return args[0];
}

static js_value_t js_native_get_attribute(js_value_t *args, int nargs, void *ud) {
    web_node_t *n = (web_node_t*)ud;
    if (nargs < 1 || args[0].type != JV_STR) return v_null();
    const char *v = web_node_attr(n, args[0].str);
    return v ? v_str(js_strdup(v)) : v_null();
}

static js_value_t js_native_set_attribute(js_value_t *args, int nargs, void *ud) {
    web_node_t *n = (web_node_t*)ud;
    if (nargs < 1 || args[0].type != JV_STR) return v_undef();
    char *val = to_string(nargs >= 2 ? args[1] : v_undef());
    js_node_set_attr(n, args[0].str, val ? val : "");
    return v_undef();
}

static js_value_t js_native_remove_attribute(js_value_t *args, int nargs, void *ud) {
    web_node_t *n = (web_node_t*)ud;
    if (nargs < 1 || args[0].type != JV_STR) return v_undef();
    js_node_remove_attr(n, args[0].str);
    return v_undef();
}

static js_value_t js_native_get_elements_by_tag(js_value_t *args, int nargs, void *ud) {
    web_node_t *root = (web_node_t*)ud;
    if (nargs < 1 || args[0].type != JV_STR) return v_undef();
    if (!root && g_doc) root = g_doc->root;
    if (!root) return v_undef();
    web_node_t *out[256];
    int n = web_get_elements_by_tag(root, args[0].str, out, 256);
    js_object_t *arr = obj_new(1);
    if (!arr) return v_undef();
    for (int i = 0; i < n; i++) {
        char key[12];
        int_to_key(i, key);
        obj_set(arr, key, dom_element(out[i]));
    }
    obj_set(arr, "length", v_num((double)n));
    return v_object(arr);
}

static js_value_t js_native_get_element_by_id(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    if (nargs < 1 || args[0].type != JV_STR || !g_doc) return v_null();
    web_node_t *n = web_get_element_by_id(g_doc->root, args[0].str);
    return n ? dom_element(n) : v_null();
}

static js_value_t js_native_create_element(js_value_t *args, int nargs, void *ud) {
    (void)ud;
    if (nargs < 1 || args[0].type != JV_STR) return v_undef();
    web_node_t *n = (web_node_t*)web_malloc(sizeof(web_node_t));
    if (!n) return v_undef();
    memset(n, 0, sizeof(*n));
    n->type = WEB_NODE_ELEMENT;
    n->tag = js_web_strdup(args[0].str);
    return dom_element(n);
}

static js_value_t js_native_query_selector(js_value_t *args, int nargs, void *ud) {
    if (nargs < 1 || args[0].type != JV_STR) return v_null();
    web_node_t *root = (web_node_t*)ud;
    if (!root && g_doc) root = g_doc->root;
    if (!root) return v_null();
    char tag[48], id[64], cls[48];
    sel_parse(args[0].str, tag, sizeof(tag), id, sizeof(id), cls, sizeof(cls));
    web_node_t *m = sel_first(root, tag, id, cls);
    return m ? dom_element(m) : v_null();
}

static js_value_t js_native_query_selector_all(js_value_t *args, int nargs, void *ud) {
    if (nargs < 1 || args[0].type != JV_STR) return v_undef();
    web_node_t *root = (web_node_t*)ud;
    if (!root && g_doc) root = g_doc->root;
    if (!root) return v_undef();
    char tag[48], id[64], cls[48];
    sel_parse(args[0].str, tag, sizeof(tag), id, sizeof(id), cls, sizeof(cls));
    js_object_t *arr = obj_new(1);
    if (!arr) return v_undef();
    int i = 0;
    /* yigin tabanli DFS */
    web_node_t *stack[512];
    int sp = 0;
    stack[sp++] = root;
    while (sp > 0) {
        web_node_t *n = stack[--sp];
        if (sel_matches(n, tag, id, cls)) {
            char key[12];
            int_to_key(i, key);
            obj_set(arr, key, dom_element(n));
            i++;
        }
        for (web_node_t *c = n->first_child; c; c = c->next_sibling)
            if (sp < 511) stack[sp++] = c;
    }
    obj_set(arr, "length", v_num((double)i));
    return v_object(arr);
}

/* ─── Element prop get/set ──────────────────────────────── */
static int dom_elem_get_prop(js_object_t *o, const char *key, js_value_t *out) {
    web_node_t *n = (web_node_t*)o->host;
    if (!n) { *out = v_undef(); return 1; }
    if (strcmp_ci(key, "tagName") == 0 || strcmp_ci(key, "nodeName") == 0) {
        char buf[32];
        const char *t = n->tag ? n->tag : "";
        int i = 0;
        while (t[i] && i < 31) {
            buf[i] = (t[i] >= 'a' && t[i] <= 'z') ? (char)(t[i] - 32) : t[i];
            i++;
        }
        buf[i] = '\0';
        *out = v_str(js_strdup(buf));
        return 1;
    }
    if (strcmp_ci(key, "nodeType") == 0) { *out = v_num(1); return 1; }
    if (strcmp_ci(key, "id") == 0) {
        *out = v_str(js_strdup(n->id ? n->id : "")); return 1;
    }
    if (strcmp_ci(key, "className") == 0) {
        const char *c = web_node_attr(n, "class");
        *out = v_str(js_strdup(c ? c : "")); return 1;
    }
    if (strcmp_ci(key, "textContent") == 0 || strcmp_ci(key, "innerText") == 0) {
        js_sb_t sb;
        sb_init(&sb);
        collect_text(n, &sb);
        *out = v_str(sb.buf ? sb.buf : js_strdup(""));
        return 1;
    }
    if (strcmp_ci(key, "innerHTML") == 0) {
        js_sb_t sb;
        sb_init(&sb);
        for (web_node_t *c = n->first_child; c; c = c->next_sibling) serialize_node(c, &sb);
        *out = v_str(sb.buf ? sb.buf : js_strdup(""));
        return 1;
    }
    if (strcmp_ci(key, "value") == 0) {
        const char *v = web_node_attr(n, "value");
        *out = v_str(js_strdup(v ? v : "")); return 1;
    }
    if (strcmp_ci(key, "href") == 0) {
        const char *v = web_node_attr(n, "href");
        *out = v_str(js_strdup(v ? v : "")); return 1;
    }
    if (strcmp_ci(key, "src") == 0) {
        const char *v = web_node_attr(n, "src");
        *out = v_str(js_strdup(v ? v : "")); return 1;
    }
    if (strcmp_ci(key, "style") == 0) {
        js_object_t *so = obj_new(0);
        if (so) { so->is_host = JS_HOST_STYLE; so->host = n; }
        *out = so ? v_object(so) : v_undef();
        return 1;
    }
    if (strcmp_ci(key, "parentNode") == 0 || strcmp_ci(key, "parentElement") == 0) {
        *out = (n->parent && n->parent->type == WEB_NODE_ELEMENT)
               ? dom_element(n->parent) : v_null();
        return 1;
    }
    if (strcmp_ci(key, "firstChild") == 0 || strcmp_ci(key, "firstElementChild") == 0) {
        web_node_t *c;
        for (c = n->first_child; c; c = c->next_sibling)
            if (c->type == WEB_NODE_ELEMENT) break;
        *out = c ? dom_element(c) : v_null();
        return 1;
    }
    if (strcmp_ci(key, "lastChild") == 0 || strcmp_ci(key, "lastElementChild") == 0) {
        web_node_t *c;
        for (c = n->last_child; c; c = c->prev_sibling)
            if (c->type == WEB_NODE_ELEMENT) break;
        *out = c ? dom_element(c) : v_null();
        return 1;
    }
    if (strcmp_ci(key, "nextSibling") == 0 || strcmp_ci(key, "nextElementSibling") == 0) {
        web_node_t *c;
        for (c = n->next_sibling; c; c = c->next_sibling)
            if (c->type == WEB_NODE_ELEMENT) break;
        *out = c ? dom_element(c) : v_null();
        return 1;
    }
    if (strcmp_ci(key, "previousSibling") == 0 || strcmp_ci(key, "previousElementSibling") == 0) {
        web_node_t *c;
        for (c = n->prev_sibling; c; c = c->prev_sibling)
            if (c->type == WEB_NODE_ELEMENT) break;
        *out = c ? dom_element(c) : v_null();
        return 1;
    }
    if (strcmp_ci(key, "children") == 0 || strcmp_ci(key, "childNodes") == 0) {
        *out = dom_children(n);
        return 1;
    }
    if (strcmp_ci(key, "length") == 0) {
        int cnt = 0;
        for (web_node_t *c = n->first_child; c; c = c->next_sibling)
            if (c->type == WEB_NODE_ELEMENT) cnt++;
        *out = v_num((double)cnt);
        return 1;
    }
    if (strcmp_ci(key, "onclick") == 0) {
        for (js_evt_t *e = g_evts; e; e = e->next)
            if (e->node == n && !e->type) { *out = e->fn; return 1; }
        *out = v_null();
        return 1;
    }
    if (strcmp_ci(key, "appendChild") == 0) { *out = v_native(js_native_append_child, n); return 1; }
    if (strcmp_ci(key, "insertBefore") == 0) { *out = v_native(js_native_insert_before, n); return 1; }
    if (strcmp_ci(key, "removeChild") == 0) { *out = v_native(js_native_remove_child, n); return 1; }
    if (strcmp_ci(key, "getAttribute") == 0) { *out = v_native(js_native_get_attribute, n); return 1; }
    if (strcmp_ci(key, "setAttribute") == 0) { *out = v_native(js_native_set_attribute, n); return 1; }
    if (strcmp_ci(key, "removeAttribute") == 0) { *out = v_native(js_native_remove_attribute, n); return 1; }
    if (strcmp_ci(key, "getElementsByTagName") == 0) { *out = v_native(js_native_get_elements_by_tag, n); return 1; }
    if (strcmp_ci(key, "querySelector") == 0) { *out = v_native(js_native_query_selector, n); return 1; }
    if (strcmp_ci(key, "querySelectorAll") == 0) { *out = v_native(js_native_query_selector_all, n); return 1; }
    if (strcmp_ci(key, "addEventListener") == 0) { *out = v_native(js_native_add_event_listener, n); return 1; }
    if (strcmp_ci(key, "removeEventListener") == 0) { *out = v_native(js_native_remove_event_listener, n); return 1; }
    const char *av = web_node_attr(n, key);
    if (av) { *out = v_str(js_strdup(av)); return 1; }
    return 0;
}

static int dom_elem_set_prop(js_object_t *o, const char *key, js_value_t v) {
    web_node_t *n = (web_node_t*)o->host;
    if (!n) return 1;
    char *s = to_string(v);
    const char *sv = s ? s : "";
    if (strcmp_ci(key, "id") == 0) { js_node_set_attr(n, "id", sv); return 1; }
    if (strcmp_ci(key, "className") == 0) { js_node_set_attr(n, "class", sv); return 1; }
    if (strcmp_ci(key, "textContent") == 0 || strcmp_ci(key, "innerText") == 0) {
        replace_children_with_text(n, sv);
        return 1;
    }
    if (strcmp_ci(key, "innerHTML") == 0) { inner_html_set(n, sv); return 1; }
    if (strcmp_ci(key, "value") == 0) { js_node_set_attr(n, "value", sv); return 1; }
    if (strcmp_ci(key, "href") == 0) { js_node_set_attr(n, "href", sv); return 1; }
    if (strcmp_ci(key, "src") == 0) { js_node_set_attr(n, "src", sv); return 1; }
    if (strcmp_ci(key, "style") == 0) { js_node_set_attr(n, "style", sv); return 1; }
    if (strcmp_ci(key, "onclick") == 0) {
        js_evt_t *e;
        for (e = g_evts; e; e = e->next) if (e->node == n && !e->type) break;
        if (!e) {
            e = (js_evt_t*)js_malloc(sizeof(js_evt_t));
            if (e) {
                memset(e, 0, sizeof(*e));
                e->node = n;
                e->next = g_evts;
                g_evts = e;
            }
        }
        if (e) e->fn = v;
        return 1;
    }
    return 0;
}

/* ─── document / window / location ──────────────────────── */
static int dom_loc_get_prop(js_object_t *o, const char *key, js_value_t *out) {
    (void)o;
    if (strcmp_ci(key, "href") == 0) {
        *out = v_str(js_strdup(g_cur_url));
        return 1;
    }
    return 0;
}

static int dom_loc_set_prop(js_object_t *o, const char *key, js_value_t v) {
    (void)o;
    if (strcmp_ci(key, "href") == 0) {
        char *s = to_string(v);
        strncpy(g_pending_nav, s ? s : "", sizeof(g_pending_nav) - 1);
        g_pending_nav[sizeof(g_pending_nav) - 1] = '\0';
        return 1;
    }
    return 0;
}

static web_node_t *dom_body(void) {
    if (!g_doc) return NULL;
    web_node_t *out[2];
    int n = web_get_elements_by_tag(g_doc->root, "body", out, 2);
    return n > 0 ? out[0] : NULL;
}

static int dom_doc_get_prop(js_object_t *o, const char *key, js_value_t *out) {
    (void)o;
    if (strcmp_ci(key, "title") == 0) {
        *out = v_str(js_strdup(g_doc && g_doc->title ? g_doc->title : ""));
        return 1;
    }
    if (strcmp_ci(key, "body") == 0) {
        web_node_t *b = dom_body();
        *out = b ? dom_element(b) : v_null();
        return 1;
    }
    if (strcmp_ci(key, "documentElement") == 0) {
        *out = g_doc ? dom_element(g_doc->root) : v_null();
        return 1;
    }
    if (strcmp_ci(key, "getElementById") == 0) { *out = v_native(js_native_get_element_by_id, NULL); return 1; }
    if (strcmp_ci(key, "getElementsByTagName") == 0) { *out = v_native(js_native_get_elements_by_tag, NULL); return 1; }
    if (strcmp_ci(key, "querySelector") == 0) { *out = v_native(js_native_query_selector, NULL); return 1; }
    if (strcmp_ci(key, "querySelectorAll") == 0) { *out = v_native(js_native_query_selector_all, NULL); return 1; }
    if (strcmp_ci(key, "createElement") == 0) { *out = v_native(js_native_create_element, NULL); return 1; }
    return 0;
}

static int dom_doc_set_prop(js_object_t *o, const char *key, js_value_t v) {
    (void)o;
    if (strcmp_ci(key, "title") == 0) {
        char *s = to_string(v);
        if (g_doc) {
            if (g_doc->title) web_free(g_doc->title);
            g_doc->title = js_web_strdup(s ? s : "");
        }
        return 1;
    }
    return 0;
}

static int dom_get_prop(js_object_t *o, const char *key, js_value_t *out) {
    switch (o->is_host) {
        case JS_HOST_ELEMENT:  return dom_elem_get_prop(o, key, out);
        case JS_HOST_STYLE:    return dom_style_get_prop(o, key, out);
        case JS_HOST_LOCATION: return dom_loc_get_prop(o, key, out);
        case JS_HOST_DOCUMENT: return dom_doc_get_prop(o, key, out);
    }
    return 0;
}

static int dom_set_prop(js_object_t *o, const char *key, js_value_t v) {
    switch (o->is_host) {
        case JS_HOST_ELEMENT:  return dom_elem_set_prop(o, key, v);
        case JS_HOST_STYLE:    return dom_style_set_prop(o, key, v);
        case JS_HOST_LOCATION: return dom_loc_set_prop(o, key, v);
        case JS_HOST_DOCUMENT: return dom_doc_set_prop(o, key, v);
    }
    return 0;
}

/* Tarayıcıların eski ama yaygın kolaylığı: id="b" → window.b / b.
   Geçerli JS tanımlayıcısı olmayan id'leri özellikle dışarıda bırakır. */
static int js_id_is_ident(const char *id) {
    if (!id || !id[0]) return 0;
    if (!((id[0] >= 'a' && id[0] <= 'z') || (id[0] >= 'A' && id[0] <= 'Z') ||
          id[0] == '_' || id[0] == '$')) return 0;
    for (const char *p = id + 1; *p; p++)
        if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
              (*p >= '0' && *p <= '9') || *p == '_' || *p == '$')) return 0;
    return 1;
}

static void js_install_id_globals(js_env_t *g, js_object_t *win, web_node_t *n) {
    if (!n) return;
    if (n->type == WEB_NODE_ELEMENT && js_id_is_ident(n->id)) {
        js_value_t elem = dom_element(n);
        env_set(g, n->id, elem);
        if (win) obj_set(win, n->id, elem);
    }
    for (web_node_t *c = n->first_child; c; c = c->next_sibling)
        js_install_id_globals(g, win, c);
}

static void js_install_builtins(js_env_t *g) {
    env_set(g, "alert", v_native(js_native_alert, NULL));
    env_set(g, "prompt", v_native(js_native_prompt, NULL));
    env_set(g, "confirm", v_native(js_native_confirm, NULL));
    env_set(g, "parseInt", v_native(js_native_parse_int, NULL));
    env_set(g, "parseFloat", v_native(js_native_parse_float, NULL));
    env_set(g, "isNaN", v_native(js_native_isnan, NULL));
    env_set(g, "Number", v_native(js_native_number, NULL));
    env_set(g, "String", v_native(js_native_string, NULL));
    env_set(g, "Boolean", v_native(js_native_boolean, NULL));

    js_object_t *console = obj_new(0);
    if (console) {
        obj_set(console, "log", v_native(js_console_log, NULL));
        obj_set(console, "error", v_native(js_console_error, NULL));
    }
    env_set(g, "console", v_object(console));

    js_object_t *math = obj_new(0);
    if (math) {
        obj_set(math, "PI", v_num(3.141592653589793));
        obj_set(math, "E", v_num(2.718281828459045));
        obj_set(math, "min", v_native(js_math_min, NULL));
        obj_set(math, "max", v_native(js_math_max, NULL));
        obj_set(math, "floor", v_native(js_math_floor, NULL));
        obj_set(math, "ceil", v_native(js_math_ceil, NULL));
        obj_set(math, "round", v_native(js_math_round, NULL));
        obj_set(math, "pow", v_native(js_math_pow, NULL));
        obj_set(math, "sqrt", v_native(js_math_sqrt, NULL));
        obj_set(math, "abs", v_native(js_math_abs, NULL));
        obj_set(math, "random", v_native(js_math_random, NULL));
    }
    env_set(g, "Math", v_object(math));

    /* window + document (Faz 2) */
    js_object_t *loc = obj_new(0);
    if (loc) { loc->is_host = JS_HOST_LOCATION; }

    js_object_t *win = obj_new(0);
    if (win) {
        obj_set(win, "location", v_object(loc));
        obj_set(win, "alert", v_native(js_native_alert, NULL));
        obj_set(win, "prompt", v_native(js_native_prompt, NULL));
        obj_set(win, "confirm", v_native(js_native_confirm, NULL));
        if (console) obj_set(win, "console", v_object(console));
        obj_set(win, "setTimeout", v_native(js_native_set_timeout, NULL));
        obj_set(win, "clearTimeout", v_native(js_native_clear_timeout, NULL));
    }
    env_set(g, "window", v_object(win));
    env_set(g, "setTimeout", v_native(js_native_set_timeout, NULL));
    env_set(g, "clearTimeout", v_native(js_native_clear_timeout, NULL));

    js_object_t *doc = obj_new(0);
    if (doc) doc->is_host = JS_HOST_DOCUMENT;
    env_set(g, "document", v_object(doc));
    if (win) obj_set(win, "document", v_object(doc));
    if (g_doc) js_install_id_globals(g, win, g_doc->root);
}

/* ─── Halka acik API ────────────────────────────────────── */
void js_set_log_cb(js_log_fn fn) { g_log_cb = fn; }
void js_set_alert_cb(js_alert_fn fn) { g_alert_cb = fn; }
void js_set_prompt_cb(js_prompt_fn fn) { g_prompt_cb = fn; }
void js_set_confirm_cb(js_confirm_fn fn) { g_confirm_cb = fn; }

void js_set_page(void *doc, void *css, const char *url) {
    g_doc = (web_document_t*)doc;
    g_css = (web_css_rule_t*)css;
    if (url) {
        strncpy(g_cur_url, url, sizeof(g_cur_url) - 1);
        g_cur_url[sizeof(g_cur_url) - 1] = '\0';
    } else {
        g_cur_url[0] = '\0';
    }
}

const char *js_get_pending_nav(void) { return g_pending_nav; }
void js_clear_pending_nav(void) { g_pending_nav[0] = '\0'; }

void js_init(void) {
    g_log_cb = NULL;
    g_alert_cb = NULL;
    g_prompt_cb = NULL;
    g_confirm_cb = NULL;
}

static void js_invoke(js_value_t fn, js_value_t *args, int nargs) {
    if (fn.type == JV_NATIVE) {
        fn.as.nf.fn(args, nargs, fn.as.nf.ud);
        return;
    }
    if (fn.type != JV_FUNC) return;
    js_func_t *f = fn.as.fn;
    js_env_t *nenv = env_new(f->closure);
    if (!nenv) return;
    for (int i = 0; i < f->nparams; i++)
        env_set(nenv, f->params[i], i < nargs ? args[i] : v_undef());
    int old_ctrl = g_ctrl;
    js_value_t old_ret = g_retval;
    g_ctrl = 0;
    eval(f->body, nenv);
    g_ctrl = old_ctrl;
    g_retval = old_ret;
}

int js_dispatch_click(void *node) {
    web_node_t *n = (web_node_t*)node;
    if (!n) return 0;
    js_object_t *event = obj_new(0);
    if (event) {
        obj_set(event, "type", v_str(js_strdup("click")));
        obj_set(event, "target", dom_element(n));
        obj_set(event, "currentTarget", dom_element(n));
    }
    js_value_t args[1];
    args[0] = event ? v_object(event) : v_undef();
    int called = 0;
    for (js_evt_t *e = g_evts; e; e = e->next) {
        if (e->node != n) continue;
        if (e->type && strcmp_ci(e->type, "click") != 0) continue;
        js_invoke(e->fn, args, 1);
        called = 1;
    }
    return called;
}

int js_tick(unsigned int elapsed_ms) {
    g_now_ms += elapsed_ms;
    int called = 0;
    js_timer_t **p = &g_timers;
    while (*p) {
        js_timer_t *t = *p;
        if ((int)(g_now_ms - t->due_ms) < 0) {
            p = &t->next;
            continue;
        }
        *p = t->next; /* tek seferlik setTimeout */
        js_invoke(t->fn, NULL, 0);
        called++;
    }
    return called;
}

void js_reset(void) {
    js_block_t *b = g_blocks;
    while (b) {
        js_block_t *next = b->next;
        if (b->ptr) web_free(b->ptr);
        web_free(b);
        b = next;
    }
    g_blocks = NULL;
    g_globals = NULL;
    g_ctrl = 0;
    g_eval_depth = 0;
    g_doc = NULL;
    g_css = NULL;
    g_cur_url[0] = '\0';
    g_pending_nav[0] = '\0';
    g_evts = NULL;
    g_timers = NULL;
    g_now_ms = 0;
}

int js_run(const char *src, int len) {
    if (!src) return -1;
    parser_t p;
    memset(&p, 0, sizeof(p));
    p.lx.src = src;
    p.lx.len = len;
    p.lx.pos = 0;
    p.lx.line = 1;
    p.cur = lex_next(&p.lx);

    if (!g_globals) {
        g_globals = env_new(NULL);
        if (!g_globals) return -1;
        js_install_builtins(g_globals);
    }

    js_ast_t *prog = parse_program(&p);
    if (p.error) {
        js_log(p.err);
        return -1;
    }
    if (!prog) return -1;

    g_ctrl = 0;
    g_eval_depth = 0;
    eval(prog, g_globals);
    g_ctrl = 0;
    return 0;
}
