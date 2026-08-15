/*
 * LAYOUT.C - CofeuTarayici Layout Motoru
 *
 * DOM + hesaplanmış stiller -> web_box ağacı (kutu modeli, block/inline akış).
 * Çizim (web_render.c) bu ağacı framebuffer'a yazar.
 */
#include "../include/web.h"
#include "../include/string.h"

/* ─── Yerel yardımcılar ───────────────────────────────────── */

static int is_ws_(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static int tag_ci_eq_(const char *a, const char *b) { return strcmp_ci(a, b) == 0; }

static char *dup_range(const char *s, unsigned int n) {
    char *p = (char*)web_malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int parse_len(const char *s) {
    int v = 0;
    while (*s && is_ws_(*s)) s++;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); s++; }
    return v;
}

int web_text_width(const char *s, int big) {
    return (int)strlen(s) * (big ? WEB_CHAR_W_BIG : WEB_CHAR_W);
}

static int line_h_of(int big) { return big ? WEB_LINE_H_BIG : WEB_LINE_H; }

/* ─── Kutu üretimi ────────────────────────────────────────── */

static web_box_t *box_new(web_node_t *node) {
    web_box_t *b = (web_box_t*)web_malloc(sizeof(web_box_t));
    if (!b) return NULL;
    memset(b, 0, sizeof(web_box_t));
    b->node = node;
    return b;
}

static void box_add_child(web_box_t *parent, web_box_t *child) {
    child->parent = parent;
    if (parent->last_child) {
        parent->last_child->next_sibling = child;
        child->prev_sibling = parent->last_child;
    } else {
        parent->first_child = child;
    }
    parent->last_child = child;
}

static void run_append(web_run_t **head, web_run_t **tail, web_run_t *r) {
    if (*tail) (*tail)->next = r; else *head = r;
    *tail = r;
    r->next = NULL;
}

/* ─── Inline içerik toplama (metin kelimeleri + görseller) ─── */

static void collect_runs(web_box_t *b, web_node_t *node, web_css_rule_t *rules,
                         u32 cur_bg, int has_cur_bg,
                         web_run_t **head, web_run_t **tail, int *n_runs) {
    web_node_t *c;
    for (c = node->first_child; c; c = c->next_sibling) {
        if (c->type == WEB_NODE_TEXT) {
            web_style_t st;
            web_compute_style(c, rules, &st);
            const char *p = c->text;
            while (*p) {
                while (*p && is_ws_(*p)) p++;
                if (!*p) break;
                const char *ws = p;
                while (*p && !is_ws_(*p)) p++;
                if (p == ws) break;

                web_run_t *r = (web_run_t*)web_malloc(sizeof(web_run_t));
                if (!r) return;
                memset(r, 0, sizeof(web_run_t));
                r->type = WEB_RUN_TEXT;
                r->text = dup_range(ws, (unsigned int)(p - ws));
                r->node = c;
                r->font_bold = st.font_bold;
                r->font_size = st.font_size;
                r->color = st.color;
                r->bg = cur_bg;
                r->has_bg = has_cur_bg;
                r->is_link = (c->parent && tag_ci_eq_(c->parent->tag, "a"));
                r->w = web_text_width(r->text, st.font_size);
                r->h = line_h_of(st.font_size);
                run_append(head, tail, r);
                (*n_runs)++;
            }
        } else if (c->type == WEB_NODE_ELEMENT) {
            web_style_t st;
            web_compute_style(c, rules, &st);
            if (st.display == WEB_DISPLAY_NONE) continue;

            if (tag_ci_eq_(c->tag, "img")) {
                int iw = 100, ih = 80;
                const char *w = web_node_attr(c, "width");
                const char *h = web_node_attr(c, "height");
                if (w && parse_len(w) > 0) iw = parse_len(w);
                if (h && parse_len(h) > 0) ih = parse_len(h);
                if (iw > 300) iw = 300;
                if (ih > 260) ih = 260;
                web_run_t *r = (web_run_t*)web_malloc(sizeof(web_run_t));
                if (!r) return;
                memset(r, 0, sizeof(web_run_t));
                r->type = WEB_RUN_IMAGE;
                r->node = c;
                r->w = iw; r->h = ih;
                r->has_bg = 1; r->bg = 0xD0D0D0;
                r->border_w = 1; r->border_color = 0x808080;
                run_append(head, tail, r);
                (*n_runs)++;
            } else if (st.display == WEB_DISPLAY_INLINE) {
                u32 nb = has_cur_bg ? cur_bg : 0;
                int has_nb = has_cur_bg;
                if (st.has_bg) { nb = st.bg_color; has_nb = 1; }
                collect_runs(b, c, rules, nb, has_nb, head, tail, n_runs);
            }
        }
    }
}

/* ─── Satır akışı + hizalama ─────────────────────────────── */

typedef struct { int total_w; } line_t;

static int flow_runs(web_box_t *b, web_run_t *runs, int n_runs, int align) {
    if (n_runs == 0) return b->content_y;

    line_t *lines = (line_t*)web_malloc(sizeof(line_t) * (unsigned int)n_runs);
    if (!lines) return b->content_y;
    int n_lines = 0;
    int x = b->content_x;
    int y = b->content_y;
    int line_h = WEB_LINE_H;
    int max_x = b->content_x + b->content_w;
    web_run_t *r = runs;
    lines[0].total_w = 0;

    for (int i = 0; i < n_runs; i++, r = r->next) {
        if (x + r->w > max_x && x > b->content_x) {
            x = b->content_x;
            y += line_h;
            line_h = WEB_LINE_H;
            n_lines++;
            lines[n_lines].total_w = 0;
        }
        r->line = n_lines;
        r->x = x;
        r->y = y;
        x += r->w;
        lines[n_lines].total_w += r->w;
        if (r->h > line_h) line_h = r->h;
    }
    n_lines++;

    /* hizalama */
    r = runs;
    int li = 0;
    for (int i = 0; i < n_runs; i++, r = r->next) {
        if (r->line != li) { li = r->line; }
        int slack = b->content_w - lines[li].total_w;
        int off = 0;
        if (slack > 0) {
            if (align == WEB_ALIGN_CENTER) off = slack / 2;
            else if (align == WEB_ALIGN_RIGHT) off = slack;
        }
        r->x += off;
    }

    web_free(lines);
    return y + line_h;
}

/* ─── Blok yerleştirme ────────────────────────────────────── */

static void layout_block(web_box_t *b, web_css_rule_t *rules, int avail_w) {
    web_style_t st;
    web_compute_style(b->node, rules, &st);

    b->border_w = st.border_w;
    b->border_color = st.border_color;
    b->bg_color = st.bg_color;
    b->has_bg = st.has_bg;

    int bw = st.border_w;
    int pl = st.padding_l, pr = st.padding_r, pt = st.padding_t, pb = st.padding_b;

    int cw = avail_w - 2 * bw - pl - pr;
    if (cw < 0) cw = 0;
    b->w = cw + 2 * bw + pl + pr;
    if (st.width >= 0 && st.width < b->w) b->w = st.width;

    b->content_x = b->x + bw + pl;
    b->content_y = b->y + bw + pt;
    b->content_w = cw;

    /* inline içerik */
    web_run_t *runs = NULL, *tail = NULL;
    int n_runs = 0;
    collect_runs(b, b->node, rules, st.has_bg ? st.bg_color : 0, st.has_bg, &runs, &tail, &n_runs);
    b->runs = runs;

    int cy = flow_runs(b, runs, n_runs, st.text_align);
    if (runs == NULL) cy = b->content_y;

    /* blok çocuklar */
    web_node_t *c;
    for (c = b->node->first_child; c; c = c->next_sibling) {
        if (c->type != WEB_NODE_ELEMENT) continue;
        web_style_t cs;
        web_compute_style(c, rules, &cs);
        if (cs.display != WEB_DISPLAY_BLOCK) continue;

        web_box_t *cb = box_new(c);
        if (!cb) continue;
        box_add_child(b, cb);
        cb->x = b->content_x + cs.margin_l;
        cb->y = cy + cs.margin_t;
        layout_block(cb, rules, cw - cs.margin_l - cs.margin_r);
        cy = cb->y + cb->h + cs.margin_b;
    }

    b->content_h = cy - b->content_y;
    b->h = (cy + pb + bw) - b->y;
    if (b->h < 2 * bw) b->h = 2 * bw;
    if (st.height >= 0) b->h = st.height;
}

int web_layout(web_node_t *root, web_css_rule_t *rules, int vw, int vh, web_box_t **out_root) {
    (void)vh;
    if (!root) { *out_root = NULL; return 0; }
    web_box_t *b = box_new(root);
    if (!b) { *out_root = NULL; return 0; }
    b->x = 0;
    b->y = 0;
    layout_block(b, rules, vw);
    *out_root = b;
    return b->h;
}

/* ─── Serbest bırakma ─────────────────────────────────────── */

void web_free_boxes(web_box_t *box) {
    web_box_t *c, *cn;
    web_run_t *r, *rn;
    if (!box) return;
    for (r = box->runs; r; r = rn) {
        rn = r->next;
        if (r->text) web_free(r->text);
        web_free(r);
    }
    for (c = box->first_child; c; c = cn) {
        cn = c->next_sibling;
        web_free_boxes(c);
    }
    web_free(box);
}
