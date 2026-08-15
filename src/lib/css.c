/*
 * CSS.C - CofeuTarayici CSS Ayristirici + Seçici + Hesaplanan Stil
 */
#include "../include/web.h"
#include "../include/string.h"

/* ─── Yardımcılar ───────────────────────────────────────── */

static char *dup_range(const char *s, unsigned int n) {
    char *p = (char*)web_malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int is_ws(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
static char to_lower_(char c) { return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c; }
static int tag_ci_eq(const char *a, const char *b) { return strcmp_ci(a, b) == 0; }

/* ─── CSS ayrıştırma ────────────────────────────────────── */

web_css_rule_t *web_parse_css(const char *css, unsigned int len) {
    web_css_rule_t *head = NULL, *tail = NULL;
    unsigned int i = 0;

    while (i < len) {
        /* kural başlangıcına ilerle */
        while (i < len && (is_ws(css[i]) || css[i] == '}')) i++;
        if (i >= len) break;

        /* selector: '{' bul */
        unsigned int sel_start = i;
        while (i < len && css[i] != '{') i++;
        if (i >= len) break;
        unsigned int sel_end = i;

        /* decl bloğu: '}' bul */
        i++;
        unsigned int decl_start = i;
        while (i < len && css[i] != '}') i++;
        if (i >= len) break;
        unsigned int decl_end = i;
        i++; /* '}' */

        /* selector'ı temizle */
        unsigned int a = sel_start;
        while (a < sel_end && is_ws(css[a])) a++;
        unsigned int b = sel_end;
        while (b > a && is_ws(css[b - 1])) b--;
        if (a >= b) continue;

        web_css_rule_t *r = (web_css_rule_t*)web_malloc(sizeof(web_css_rule_t));
        if (!r) continue;
        r->selector = dup_range(css + a, b - a);
        r->decls = NULL;

        /* decl'leri ayır: ';' ile böl, ':' ile ayır */
        unsigned int j = decl_start;
        web_css_decl_t **dp = &r->decls;
        while (j < decl_end) {
            unsigned int ds = j;
            while (j < decl_end && css[j] != ';') j++;
            unsigned int de = j;
            j++; /* ';' */

            unsigned int col = ds;
            while (col < de && css[col] != ':') col++;
            if (col >= de) continue;

            unsigned int p1 = ds;
            while (p1 < col && is_ws(css[p1])) p1++;
            unsigned int p2 = col;
            while (p2 > p1 && is_ws(css[p2 - 1])) p2--;

            unsigned int v1 = col + 1;
            while (v1 < de && is_ws(css[v1])) v1++;
            unsigned int v2 = de;
            while (v2 > v1 && is_ws(css[v2 - 1])) v2--;

            if (p1 >= p2) continue;

            web_css_decl_t *d = (web_css_decl_t*)web_malloc(sizeof(web_css_decl_t));
            if (!d) continue;
            d->prop = dup_range(css + p1, p2 - p1);
            d->value = dup_range(css + v1, v2 - v1);
            d->next = NULL;
            *dp = d;
            dp = &d->next;
        }

        if (head) tail->next = r; else head = r;
        tail = r;
    }
    return head;
}

void web_free_css(web_css_rule_t *rules) {
    web_css_rule_t *r, *rn;
    web_css_decl_t *d, *dn;
    for (r = rules; r; r = rn) {
        rn = r->next;
        if (r->selector) web_free(r->selector);
        for (d = r->decls; d; d = dn) {
            dn = d->next;
            if (d->prop) web_free(d->prop);
            if (d->value) web_free(d->value);
            web_free(d);
        }
        web_free(r);
    }
}

/* ─── Renk ──────────────────────────────────────────────── */

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    c = to_lower_(c);
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

u32 web_parse_color(const char *s) {
    static const struct { const char *name; u32 rgb; } named[] = {
        {"black",0x000000},{"white",0xFFFFFF},{"red",0xFF0000},{"green",0x008000},
        {"lime",0x00FF00},{"blue",0x0000FF},{"yellow",0xFFFF00},{"cyan",0x00FFFF},
        {"aqua",0x00FFFF},{"magenta",0xFF00FF},{"fuchsia",0xFF00FF},{"gray",0x808080},
        {"grey",0x808080},{"silver",0xC0C0C0},{"maroon",0x800000},{"olive",0x808000},
        {"purple",0x800080},{"teal",0x008080},{"navy",0x000080},{"orange",0xFFA500},
        {"brown",0xA52A2A},{"pink",0xFFC0CB},{"darkblue",0x00008B},{"lightgray",0xD3D3D3},
        {"lightgrey",0xD3D3D3},{"darkgray",0xA9A9A9},{"gold",0xFFD700},{"indigo",0x4B0082}
    };
    if (!s) return 0x000000;
    if (s[0] == '#') {
        unsigned int n = 0;
        unsigned int v = 0;
        const char *p = s + 1;
        while (*p && n < 6 && hex_val(*p) >= 0) { v = (v << 4) | (unsigned int)hex_val(*p); p++; n++; }
        if (n == 3) {
            /* #rgb -> #rrggbb */
            unsigned int r = (v >> 8) & 0xF, g = (v >> 4) & 0xF, b = v & 0xF;
            return (r << 20) | (r << 16) | (g << 12) | (g << 8) | (b << 4) | b;
        }
        if (n == 6) return v;
        return 0x000000;
    }
    for (unsigned int i = 0; i < sizeof(named)/sizeof(named[0]); i++) {
        if (strcmp_ci(s, named[i].name) == 0) return named[i].rgb;
    }
    return 0x000000;
}

/* ─── Seçici eşleştirme ────────────────────────────────── */

typedef struct {
    char tag[24];       /* boş = herhangi */
    char id[24];
    char classes[4][24];
    int nclass;
} compound_t;

typedef struct {
    compound_t parts[8];
    int nparts;         /* 1 = basit; >1 = descendant (sağdan sola) */
    int specificity;
    int ok;
} parsed_selector_t;

/* bir compound ayrıştır: "div.box#x", ".cls", "#id", "tag", "*" */
static void parse_compound(const char *s, compound_t *c) {
    memset(c, 0, sizeof(compound_t));
    const char *p = s;
    while (*p) {
        if (*p == '.') {
            p++;
            unsigned int n = 0;
            while (p[n] && !is_ws(p[n]) && p[n] != '.' && p[n] != '#' && p[n] != ':') n++;
            if (c->nclass < 4 && n > 0 && n < 24) {
                memcpy(c->classes[c->nclass], p, n);
                c->classes[c->nclass][n] = '\0';
                c->nclass++;
            }
            p += n;
        } else if (*p == '#') {
            p++;
            unsigned int n = 0;
            while (p[n] && !is_ws(p[n]) && p[n] != '.' && p[n] != '#' && p[n] != ':') n++;
            if (n > 0 && n < 24) { memcpy(c->id, p, n); c->id[n] = '\0'; }
            p += n;
        } else if (*p == ':') {
            /* pseudo: yok say (v1) */
            while (*p && !is_ws(*p) && *p != '.' && *p != '#' && *p != ',') p++;
        } else {
            unsigned int n = 0;
            while (p[n] && !is_ws(p[n]) && p[n] != '.' && p[n] != '#' && p[n] != ':') n++;
            if (n > 0 && n < 24 && !(n == 1 && p[0] == '*')) {
                memcpy(c->tag, p, n);
                c->tag[n] = '\0';
            }
            p += n;
        }
    }
}

static void parse_selector(const char *sel, parsed_selector_t *out) {
    memset(out, 0, sizeof(*out));
    /* virgülleri ayrı ele almak üzere çağıran taraf böler; burada tek seçici */
    char buf[160];
    unsigned int blen = 0;
    while (*sel && blen < 159) buf[blen++] = *sel++;
    buf[blen] = '\0';

    /* boşlukla ayrılmış compound'ları ayır (descendant) */
    int nparts = 0;
    const char *p = buf;
    for (;;) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        const char *start = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (nparts < 8) {
            char part[40];
            unsigned int n = (unsigned int)(p - start);
            if (n > 39) n = 39;
            memcpy(part, start, n);
            part[n] = '\0';
            parse_compound(part, &out->parts[nparts]);
            nparts++;
        }
    }
    out->nparts = nparts;
    out->ok = nparts > 0;
}

static int compound_matches(const compound_t *c, const web_node_t *n) {
    if (n->type != WEB_NODE_ELEMENT) return 0;
    if (c->tag[0] && !tag_ci_eq(n->tag, c->tag)) return 0;
    if (c->id[0]) {
        if (!n->id || strcmp(n->id, c->id) != 0) return 0;
    }
    for (int i = 0; i < c->nclass; i++) {
        if (!web_node_has_class(n, c->classes[i])) return 0;
    }
    return 1;
}

/* n, sel'deki compound'larla eşleşir mi; specificity döner (0 = eşleşmez) */
static int selector_matches(const char *selector, const web_node_t *n, int *spec) {
    /* virgüllü listeyi böl, her birinde dene */
    char buf[200];
    unsigned int blen = 0;
    while (*selector && blen < 199) buf[blen++] = *selector++;
    buf[blen] = '\0';

    int best_spec = 0;
    char *tok = strtok(buf, ",");
    while (tok) {
        parsed_selector_t ps;
        parse_selector(tok, &ps);
        if (!ps.ok) { tok = strtok(NULL, ","); continue; }

        /* sağdan sola: son compound düğümle, öncekiler atalarla eşleşmeli */
        int spec_here = 0;
        for (int i = 0; i < ps.nparts; i++) {
            const compound_t *cp = &ps.parts[i];
            spec_here += (cp->id[0] ? 100 : 0);
            spec_here += cp->nclass * 10;
            spec_here += (cp->tag[0] ? 1 : 0);
        }

        int match = 0;
        if (ps.nparts == 1) {
            match = compound_matches(&ps.parts[0], n);
        } else {
            if (!compound_matches(&ps.parts[ps.nparts - 1], n)) match = 0;
            else {
                /* atalarda kalanlar sırayla aranır */
                int pi = ps.nparts - 2;
                const web_node_t *anc = n->parent;
                while (pi >= 0 && anc) {
                    if (compound_matches(&ps.parts[pi], anc)) pi--;
                    anc = anc->parent;
                }
                match = (pi < 0);
            }
        }
        if (match && spec_here > best_spec) best_spec = spec_here;
        tok = strtok(NULL, ",");
    }
    if (spec) *spec = best_spec;
    return best_spec > 0 ? 1 : 0;
}

/* ─── Varsayılan stiller ────────────────────────────────── */

static int is_block_tag(const char *tag) {
    static const char *blocks[] = {"html","body","div","p","h1","h2","h3","h4","h5","h6",
        "ul","ol","li","table","tr","td","th","blockquote","pre","center","hr","form",
        "header","footer","section","article","nav","aside","main","figure","figcaption",
        "details","summary","dl","dt","dd","button"};
    for (unsigned int i = 0; i < sizeof(blocks)/sizeof(blocks[0]); i++)
        if (tag_ci_eq(tag, blocks[i])) return 1;
    return 0;
}

static int is_hidden_tag(const char *tag) {
    static const char *hids[] = {"script","style","head","title","meta","link","template"};
    for (unsigned int i = 0; i < sizeof(hids)/sizeof(hids[0]); i++)
        if (tag_ci_eq(tag, hids[i])) return 1;
    return 0;
}

static void style_defaults(const char *tag, web_style_t *st, int inherit) {
    memset(st, 0, sizeof(web_style_t));
    st->color = 0x000000;
    st->border_color = 0x000000;
    st->width = -1;
    st->height = -1;
    st->font_size = inherit; /* varsayılan: aynı */
    st->font_bold = 0;
    st->text_align = 0;

    if (!tag) { st->display = WEB_DISPLAY_BLOCK; return; }

    if (is_hidden_tag(tag)) {
        st->display = WEB_DISPLAY_NONE;
    } else if (is_block_tag(tag)) {
        st->display = WEB_DISPLAY_BLOCK;
        if (tag_ci_eq(tag, "p") || tag_ci_eq(tag, "h1") || tag_ci_eq(tag, "h2") ||
            tag_ci_eq(tag, "h3") || tag_ci_eq(tag, "h4") || tag_ci_eq(tag, "h5") ||
            tag_ci_eq(tag, "h6") || tag_ci_eq(tag, "div") || tag_ci_eq(tag, "ul")) {
            st->margin_t = 6; st->margin_b = 6;
        }
        if (tag_ci_eq(tag, "h1")) { st->font_size = WEB_FONT_BIG; st->font_bold = 1; st->margin_t = 8; st->margin_b = 6; }
        if (tag_ci_eq(tag, "h2") || tag_ci_eq(tag, "h3")) { st->font_bold = 1; st->margin_t = 7; st->margin_b = 5; }
        if (tag_ci_eq(tag, "h4") || tag_ci_eq(tag, "h5") || tag_ci_eq(tag, "h6")) st->font_bold = 1;
        if (tag_ci_eq(tag, "ul") || tag_ci_eq(tag, "ol")) { st->margin_l = 22; st->margin_t = 4; st->margin_b = 4; }
        if (tag_ci_eq(tag, "li")) { st->margin_l = 4; }
        if (tag_ci_eq(tag, "body")) { st->margin_t = 0; st->margin_b = 0; }
        if (tag_ci_eq(tag, "center")) st->text_align = WEB_ALIGN_CENTER;
        if (tag_ci_eq(tag, "pre")) { st->bg_color = 0xE8E8E8; st->has_bg = 1; st->border_w = 1; st->border_color = 0xCCCCCC; }
        if (tag_ci_eq(tag, "td") || tag_ci_eq(tag, "th")) { st->border_w = 1; st->border_color = 0x808080; st->padding_l = 4; st->padding_r = 4; st->padding_t = 2; st->padding_b = 2; }
        if (tag_ci_eq(tag, "table")) { st->border_w = 1; st->border_color = 0x808080; }
        if (tag_ci_eq(tag, "button")) {
            /* Tarayıcı denetimi: görünür, tıklanabilir, varsayılan boyutlu. */
            st->width = 88;
            st->margin_t = 3; st->margin_b = 3;
            st->padding_l = 7; st->padding_r = 7;
            st->padding_t = 3; st->padding_b = 3;
            st->bg_color = 0xE0E0E0; st->has_bg = 1;
            st->border_w = 1; st->border_color = 0x606060;
        }
    } else {
        st->display = WEB_DISPLAY_INLINE;
        if (tag_ci_eq(tag, "a")) { st->color = 0x0000CC; }
        if (tag_ci_eq(tag, "b") || tag_ci_eq(tag, "strong")) st->font_bold = 1;
    }
}

/* ─── Sayı ayrıştırma (px, em vs) ───────────────────────── */

static int parse_len(const char *s) {
    int v = 0;
    const char *p = s;
    while (*p && is_ws(*p)) p++;
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; }
    return v;
}

/* ─── Stil uygulama ────────────────────────────────────── */

static void apply_decl(web_style_t *st, const char *prop, const char *value) {
    if (strcmp_ci(prop, "color") == 0) { st->color = web_parse_color(value); return; }
    if (strcmp_ci(prop, "background-color") == 0 || strcmp_ci(prop, "background") == 0) {
        st->bg_color = web_parse_color(value); st->has_bg = 1; return;
    }
    if (strcmp_ci(prop, "border-color") == 0) { st->border_color = web_parse_color(value); return; }
    if (strcmp_ci(prop, "border-width") == 0) { st->border_w = parse_len(value); if (st->border_w > 4) st->border_w = 4; return; }
    if (strcmp_ci(prop, "border") == 0) { st->border_w = 1; st->border_color = 0x000000; return; }
    if (strcmp_ci(prop, "display") == 0) {
        if (strcmp_ci(value, "block") == 0) st->display = WEB_DISPLAY_BLOCK;
        else if (strcmp_ci(value, "inline") == 0) st->display = WEB_DISPLAY_INLINE;
        else if (strcmp_ci(value, "none") == 0) st->display = WEB_DISPLAY_NONE;
        return;
    }
    if (strcmp_ci(prop, "font-weight") == 0) {
        if (strcmp_ci(value, "bold") == 0 || strcmp_ci(value, "bolder") == 0 || parse_len(value) >= 600)
            st->font_bold = 1;
        else if (strcmp_ci(value, "normal") == 0) st->font_bold = 0;
        return;
    }
    if (strcmp_ci(prop, "text-align") == 0) {
        if (strcmp_ci(value, "center") == 0) st->text_align = WEB_ALIGN_CENTER;
        else if (strcmp_ci(value, "right") == 0) st->text_align = WEB_ALIGN_RIGHT;
        else st->text_align = WEB_ALIGN_LEFT;
        return;
    }
    if (strcmp_ci(prop, "font-size") == 0) {
        if (strcmp_ci(value, "small") == 0 || strcmp_ci(value, "smaller") == 0) st->font_size = WEB_FONT_NORMAL;
        else if (strcmp_ci(value, "large") == 0 || strcmp_ci(value, "larger") == 0 ||
                 strcmp_ci(value, "x-large") == 0 || strcmp_ci(value, "xx-large") == 0)
            st->font_size = WEB_FONT_BIG;
        else if (parse_len(value) >= 20) st->font_size = WEB_FONT_BIG;
        else st->font_size = WEB_FONT_NORMAL;
        return;
    }
    if (strcmp_ci(prop, "margin") == 0) {
        int v = parse_len(value);
        st->margin_t = st->margin_r = st->margin_b = st->margin_l = v;
        return;
    }
    if (strcmp_ci(prop, "margin-top") == 0) { st->margin_t = parse_len(value); return; }
    if (strcmp_ci(prop, "margin-right") == 0) { st->margin_r = parse_len(value); return; }
    if (strcmp_ci(prop, "margin-bottom") == 0) { st->margin_b = parse_len(value); return; }
    if (strcmp_ci(prop, "margin-left") == 0) { st->margin_l = parse_len(value); return; }
    if (strcmp_ci(prop, "padding") == 0) {
        int v = parse_len(value);
        st->padding_t = st->padding_r = st->padding_b = st->padding_l = v;
        return;
    }
    if (strcmp_ci(prop, "padding-top") == 0) { st->padding_t = parse_len(value); return; }
    if (strcmp_ci(prop, "padding-right") == 0) { st->padding_r = parse_len(value); return; }
    if (strcmp_ci(prop, "padding-bottom") == 0) { st->padding_b = parse_len(value); return; }
    if (strcmp_ci(prop, "padding-left") == 0) { st->padding_l = parse_len(value); return; }
    if (strcmp_ci(prop, "width") == 0) { st->width = parse_len(value); return; }
    if (strcmp_ci(prop, "height") == 0) { st->height = parse_len(value); return; }
}

/* ─── Hesaplanan stil ───────────────────────────────────── */

void web_compute_style(web_node_t *node, web_css_rule_t *rules, web_style_t *out) {
    if (!node) { memset(out, 0, sizeof(web_style_t)); return; }

    int is_el = (node->type == WEB_NODE_ELEMENT);
    const char *tag = is_el ? node->tag : NULL;

    /* kalıtım: üst stili varsayılan olarak al */
    int inherit_font = 0, inherit_align = 0, inherit_bold = 0;
    u32 inherit_color = 0x000000;
    if (node->parent) {
        web_style_t ps;
        web_compute_style(node->parent, rules, &ps);
        inherit_color = ps.color;
        inherit_align = ps.text_align;
        inherit_font = ps.font_size;
        inherit_bold = ps.font_bold;
    }

    style_defaults(tag, out, is_el ? inherit_font : 0);
    out->color = inherit_color;
    out->text_align = inherit_align;
    out->font_size = is_el ? (out->font_size > inherit_font ? out->font_size : inherit_font)
                           : inherit_font;
    out->font_bold = is_el ? (out->font_bold || inherit_bold) : inherit_bold;

    /* kural eşleştirme: topla, özgüllüğe göre uygula */
    static web_css_rule_t *matched[96];
    static int matched_spec[96];
    int nm = 0;

    for (web_css_rule_t *r = rules; r && nm < 96; r = r->next) {
        int spec;
        if (selector_matches(r->selector, node, &spec)) {
            matched[nm] = r;
            matched_spec[nm] = spec;
            nm++;
        }
    }
    /* basit sıralama: özgüllük + sıra */
    for (int i = 0; i < nm; i++) {
        for (int k = i + 1; k < nm; k++) {
            if (matched_spec[k] > matched_spec[i]) {
                web_css_rule_t *t = matched[i]; matched[i] = matched[k]; matched[k] = t;
                int ts = matched_spec[i]; matched_spec[i] = matched_spec[k]; matched_spec[k] = ts;
            }
        }
    }
    for (int i = 0; i < nm; i++)
        for (web_css_decl_t *d = matched[i]->decls; d; d = d->next)
            apply_decl(out, d->prop, d->value);

    /* style= niteliği en yüksek öncelik */
    if (is_el) {
        const char *inl = web_node_attr(node, "style");
        if (inl) {
            char buf[256];
            unsigned int n = (unsigned int)strlen(inl);
            if (n > 255) n = 255;
            memcpy(buf, inl, n);
            buf[n] = '\0';
            char *d = strtok(buf, ";");
            while (d) {
                char *col = strchr(d, ':');
                if (col) {
                    *col = '\0';
                    char *prop = d;
                    char *val = col + 1;
                    while (*prop == ' ') prop++;
                    char *pe = prop + strlen(prop);
                    while (pe > prop && pe[-1] == ' ') pe--;
                    *pe = '\0';
                    while (*val == ' ') val++;
                    char *ve = val + strlen(val);
                    while (ve > val && ve[-1] == ' ') ve--;
                    *ve = '\0';
                    apply_decl(out, prop, val);
                }
                d = strtok(NULL, ";");
            }
        }
        /* sunum etiketleri */
        if (tag_ci_eq(tag, "b") || tag_ci_eq(tag, "strong")) out->font_bold = 1;
        if (tag_ci_eq(tag, "center")) out->text_align = WEB_ALIGN_CENTER;
        if (tag_ci_eq(tag, "i") || tag_ci_eq(tag, "em")) out->color = 0x006060;
        if (tag_ci_eq(tag, "a")) out->color = 0x0000CC;
    }
}
