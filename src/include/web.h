/*
 * WEB.H - CofeuTarayici Web Motoru (DOM + CSS + Layout)
 *
 * Platformdan bagimsiz: bellek icin web_malloc/web_free kullanilir.
 * OS build'de bu fonksiyonlar kmalloc/kfree (g_mem_arena) uzerinden,
 * native testlerde malloc/free uzerinden tanimlanir.
 */
#ifndef _WEB_H
#define _WEB_H

#include "types.h"

/* ─── Bellek ─────────────────────────────────────────────── */
void *web_malloc(unsigned int size);
void  web_free(void *ptr);

/* ─── Font metrikleri (video fontuyla uyumlu) ────────────── */
#define WEB_CHAR_W       8
#define WEB_CHAR_H       16
#define WEB_LINE_H       16
#define WEB_CHAR_W_BIG   16
#define WEB_LINE_H_BIG   32

/* ─── DOM ────────────────────────────────────────────────── */
typedef enum {
    WEB_NODE_ELEMENT = 0,
    WEB_NODE_TEXT,
    WEB_NODE_COMMENT
} web_node_type_t;

typedef struct web_attr {
    char *name;
    char *value;
    struct web_attr *next;
} web_attr_t;

typedef struct web_node {
    web_node_type_t type;
    char *tag;
    char *id;
    web_attr_t *attrs;
    struct web_node *parent;
    struct web_node *first_child;
    struct web_node *last_child;
    struct web_node *next_sibling;
    struct web_node *prev_sibling;
    char *text;             /* metin düğümleri için */
} web_node_t;

typedef struct {
    web_node_t *root;       /* <html> kökü */
    char *title;
} web_document_t;

web_document_t *web_parse_html(const char *html, unsigned int len);
void web_free_document(web_document_t *doc);

web_node_t *web_get_element_by_id(const web_node_t *node, const char *id);
int  web_get_elements_by_tag(const web_node_t *node, const char *tag, web_node_t **out, int max);
int  web_node_has_class(const web_node_t *n, const char *cls);
const char *web_node_attr(const web_node_t *n, const char *name);

/* ─── CSS ────────────────────────────────────────────────── */
typedef struct web_css_decl {
    char *prop;
    char *value;
    struct web_css_decl *next;
} web_css_decl_t;

typedef struct web_css_rule {
    char *selector;
    web_css_decl_t *decls;
    struct web_css_rule *next;
} web_css_rule_t;

web_css_rule_t *web_parse_css(const char *css, unsigned int len);
void web_free_css(web_css_rule_t *rules);

typedef enum {
    WEB_DISPLAY_BLOCK = 0,
    WEB_DISPLAY_INLINE,
    WEB_DISPLAY_NONE
} web_display_t;

/* text-align */
#define WEB_ALIGN_LEFT   0
#define WEB_ALIGN_CENTER 1
#define WEB_ALIGN_RIGHT  2

/* font-size */
#define WEB_FONT_NORMAL  0
#define WEB_FONT_BIG     1

typedef struct web_style {
    int display;
    u32 color;
    u32 bg_color;
    int has_bg;
    u32 border_color;
    int border_w;
    int font_bold;
    int font_size;
    int text_align;
    int margin_t, margin_r, margin_b, margin_l;
    int padding_t, padding_r, padding_b, padding_l;
    int width;      /* -1 otomatik */
    int height;     /* -1 otomatik */
} web_style_t;

/* Düğüm için nihai stili hesaplar (kurallar + style attr + kalıtım) */
void web_compute_style(web_node_t *node, web_css_rule_t *rules, web_style_t *out);

/* 32-bit renk yardımcısı: "#rrggbb" veya isim */
u32 web_parse_color(const char *s);

/* ─── Layout ─────────────────────────────────────────────── */
typedef enum {
    WEB_RUN_TEXT = 0,
    WEB_RUN_IMAGE
} web_run_type_t;

typedef struct web_run {
    web_run_type_t type;
    web_node_t *node;
    char *text;             /* kelime */
    int w, h;
    int x, y;
    int line;               /* hizalama için satır no */
    u32 color;
    u32 bg;
    int has_bg;
    u32 border_color;
    int border_w;
    int font_bold;
    int font_size;
    int is_link;
    struct web_run *next;
} web_run_t;

typedef struct web_box {
    web_node_t *node;
    struct web_box *parent;
    struct web_box *first_child;
    struct web_box *last_child;
    struct web_box *next_sibling;
    struct web_box *prev_sibling;
    web_run_t *runs;
    int x, y, w, h;             /* border kutusu */
    int content_x, content_y;   /* içerik orijini */
    int content_w, content_h;
    u32 bg_color;
    int has_bg;
    u32 border_color;
    int border_w;
} web_box_t;

/* DOM'u viewport (vw, vh) içine yerleştirir. Dönen toplam yükseklik. */
int web_layout(web_node_t *root, web_css_rule_t *rules, int vw, int vh, web_box_t **out_root);

void web_free_boxes(web_box_t *box);

int web_text_width(const char *s, int big);

#endif
