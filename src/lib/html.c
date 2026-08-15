/*
 * HTML.C - CofeuTarayici HTML Parser -> DOM
 *
 * Platformdan bagimsiz: web_malloc/web_free ile bellek, string.h ile dize.
 */
#include "../include/web.h"
#include "../include/string.h"

/* ─── Yerel yardımcılar ─────────────────────────────────── */

static char *web_strdup(const char *s) {
    unsigned int len;
    char *p;
    if (!s) return NULL;
    len = (unsigned int)strlen(s);
    p = (char*)web_malloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len + 1);
    return p;
}

static char *web_strndup(const char *s, unsigned int n) {
    char *p = (char*)web_malloc(n + 1);
    if (!p) return NULL;
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static int tag_ci_eq(const char *a, const char *b) {
    return strcmp_ci(a, b) == 0;
}

/* Void (içeriksiz) etiketler */
static int is_void_tag(const char *tag) {
    static const char *voids[] = {"br","img","input","hr","meta","link","col",
                                  "area","base","embed","source","track","wbr"};
    for (unsigned int i = 0; i < sizeof(voids)/sizeof(voids[0]); i++)
        if (tag_ci_eq(tag, voids[i])) return 1;
    return 0;
}

/* Otomatik kapanan bloklar: aynı etiket üstteyse kapat */
static int auto_close_same(const char *tag) {
    if (tag_ci_eq(tag, "p") || tag_ci_eq(tag, "li") || tag_ci_eq(tag, "td") ||
        tag_ci_eq(tag, "th") || tag_ci_eq(tag, "dt") || tag_ci_eq(tag, "dd") ||
        tag_ci_eq(tag, "tr") || tag_ci_eq(tag, "option")) return 1;
    return 0;
}

static int is_block_heading(const char *tag) {
    return tag[0] == 'h' && tag[1] >= '1' && tag[1] <= '6' && tag[2] == '\0';
}

/* h1..h6: yeni başlık önceki başlığı kapatır */
static int auto_close_heading(const char *tag) {
    if (!is_block_heading(tag)) return 0;
    return 1;
}

/* ─── Düğüm işlemleri ───────────────────────────────────── */

static web_node_t *node_new(web_node_type_t type) {
    web_node_t *n = (web_node_t*)web_malloc(sizeof(web_node_t));
    if (!n) return NULL;
    memset(n, 0, sizeof(web_node_t));
    n->type = type;
    return n;
}

static void node_append_child(web_node_t *parent, web_node_t *child) {
    child->parent = parent;
    if (parent->last_child) {
        parent->last_child->next_sibling = child;
        child->prev_sibling = parent->last_child;
    } else {
        parent->first_child = child;
    }
    parent->last_child = child;
}

static web_attr_t *node_find_attr(web_node_t *n, const char *name) {
    web_attr_t *a;
    for (a = n->attrs; a; a = a->next)
        if (strcmp_ci(a->name, name) == 0) return a;
    return NULL;
}

const char *web_node_attr(const web_node_t *n, const char *name) {
    const web_attr_t *a;
    if (!n) return NULL;
    for (a = n->attrs; a; a = a->next)
        if (strcmp_ci(a->name, name) == 0) return a->value;
    return NULL;
}

static void node_set_attr(web_node_t *n, const char *name, const char *value) {
    web_attr_t *a = node_find_attr(n, name);
    char *val = web_strdup(value);
    if (a) {
        web_free(a->value);
        a->value = val;
        return;
    }
    a = (web_attr_t*)web_malloc(sizeof(web_attr_t));
    if (!a) return;
    a->name = web_strdup(name);
    a->value = val;
    a->next = n->attrs;
    n->attrs = a;
    if (strcmp_ci(name, "id") == 0) {
        if (n->id) web_free(n->id);
        n->id = web_strdup(value);
    }
}

static void node_free(web_node_t *n) {
    web_node_t *c, *next;
    web_attr_t *a, *an;
    if (!n) return;
    for (c = n->first_child; c; c = next) { next = c->next_sibling; node_free(c); }
    for (a = n->attrs; a; a = an) {
        an = a->next;
        if (a->name) web_free(a->name);
        if (a->value) web_free(a->value);
        web_free(a);
    }
    if (n->tag) web_free(n->tag);
    if (n->id) web_free(n->id);
    if (n->text) web_free(n->text);
    web_free(n);
}

/* ─── Entity çözümü ─────────────────────────────────────── */

static int decode_entity(const char *s, char *out) {
    /* s '&' ile başlar; çözülen karakter sayısı veya -1 döner */
    if (s[1] == '#') {
        unsigned int code = 0;
        const char *p = s + 2;
        if (*p == 'x' || *p == 'X') {
            p++;
            while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')) {
                code = code * 16 + (*p <= '9' ? *p - '0' : (to_lower(*p) - 'a' + 10));
                p++;
            }
        } else {
            while (*p >= '0' && *p <= '9') { code = code * 10 + (*p - '0'); p++; }
        }
        if (*p == ';' && code < 128) { out[0] = (char)code; return (int)(p - s) + 1; }
        return -1;
    }
    if (strncmp(s, "&amp;", 5) == 0)  { out[0] = '&';  return 5; }
    if (strncmp(s, "&lt;", 4) == 0)   { out[0] = '<';  return 4; }
    if (strncmp(s, "&gt;", 4) == 0)   { out[0] = '>';  return 4; }
    if (strncmp(s, "&quot;", 6) == 0) { out[0] = '"';  return 6; }
    if (strncmp(s, "&#39;", 5) == 0)  { out[0] = '\''; return 5; }
    if (strncmp(s, "&nbsp;", 6) == 0) { out[0] = ' ';  return 6; }
    return -1;
}

/* ─── Parser ────────────────────────────────────────────── */

typedef struct {
    const char *src;
    unsigned int pos;
    unsigned int len;
} parser_t;

static int p_is_eof(parser_t *p) { return p->pos >= p->len; }
static char p_peek(parser_t *p)  { return p_is_eof(p) ? '\0' : p->src[p->pos]; }
static void p_skip_ws(parser_t *p) {
    while (!p_is_eof(p)) {
        char c = p_peek(p);
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->pos++;
        else break;
    }
}

static int p_starts_with(parser_t *p, const char *s) {
    unsigned int n = (unsigned int)strlen(s);
    if (p->pos + n > p->len) return 0;
    return memcmp(p->src + p->pos, s, n) == 0;
}

/* Buyuk/kucuk harf duyarsiz on-ek karsilastirmasi */
static int p_starts_with_ci(parser_t *p, const char *s) {
    unsigned int n = (unsigned int)strlen(s);
    if (p->pos + n > p->len) return 0;
    return strncmp_ci(p->src + p->pos, s, n) == 0;
}

/* Etiket adı okur (alfanümerik + -) */
static char *read_tag_name(parser_t *p) {
    unsigned int start = p->pos;
    while (!p_is_eof(p)) {
        char c = p_peek(p);
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == ':' || c == '_')
            p->pos++;
        else break;
    }
    if (p->pos == start) return NULL;
    return web_strndup(p->src + start, p->pos - start);
}

/* Nitelik okur: name, name=value, name="value" */
static void read_attributes(parser_t *p, web_node_t *node) {
    for (;;) {
        p_skip_ws(p);
        char c = p_peek(p);
        if (c == '\0' || c == '>' || c == '/') break;

        char *name = read_tag_name(p);
        if (!name) { p->pos++; continue; }

        p_skip_ws(p);
        char *value = web_strdup("");
        if (p_peek(p) == '=') {
            p->pos++;
            p_skip_ws(p);
            char quote = p_peek(p);
            if (quote == '"' || quote == '\'') {
                p->pos++;
                unsigned int start = p->pos;
                while (!p_is_eof(p) && p_peek(p) != quote) p->pos++;
                value = web_strndup(p->src + start, p->pos - start);
                if (!p_is_eof(p)) p->pos++;   /* kapatma tırnak */
            } else {
                unsigned int start = p->pos;
                while (!p_is_eof(p)) {
                    char q = p_peek(p);
                    if (q == ' ' || q == '\t' || q == '\n' || q == '\r' || q == '>') break;
                    p->pos++;
                }
                value = web_strndup(p->src + start, p->pos - start);
            }
        }
        node_set_attr(node, name, value);
        web_free(name);
        web_free(value);
    }
}

/* Metin düğümü üretir; entity çözümü yapılır */
static web_node_t *make_text_node(parser_t *p) {
    unsigned int start = p->pos;
    while (!p_is_eof(p) && p_peek(p) != '<') p->pos++;
    unsigned int raw_len = p->pos - start;

    char *buf = (char*)web_malloc(raw_len + 1);
    if (!buf) return NULL;
    unsigned int w = 0;
    for (unsigned int i = 0; i < raw_len; i++) {
        char c = p->src[start + i];
        if (c == '&') {
            char dec;
            int n = decode_entity(p->src + start + i, &dec);
            if (n > 0) { buf[w++] = dec; i += (unsigned int)n - 1; continue; }
        }
        buf[w++] = c;
    }
    buf[w] = '\0';

    if (w == 0) { web_free(buf); return NULL; }
    web_node_t *n = node_new(WEB_NODE_TEXT);
    if (!n) { web_free(buf); return NULL; }
    n->text = buf;
    return n;
}

/* Başlık (title) metnini kopyalar; p <title> açılışından hemen sonradır */
static void capture_title(web_document_t *doc, parser_t *p) {
    unsigned int start = p->pos;
    while (!p_is_eof(p) && p_peek(p) != '<') p->pos++;
    unsigned int raw_len = p->pos - start;

    char *buf = (char*)web_malloc(raw_len + 1);
    if (!buf) return;
    unsigned int w = 0;
    for (unsigned int i = 0; i < raw_len; i++) {
        char c = p->src[start + i];
        if (c == '&') {
            char dec;
            int n = decode_entity(p->src + start + i, &dec);
            if (n > 0) { buf[w++] = dec; i += (unsigned int)n - 1; continue; }
        }
        buf[w++] = c;
    }
    buf[w] = '\0';
    if (doc->title) web_free(doc->title);
    doc->title = buf;
}

/* <script> içeriğini RAW (entity çözmeden) </script> değin alır ve
   metin düğümü olarak script elemanına ekler. */
static void capture_script(parser_t *p, web_node_t *el) {
    unsigned int start = p->pos;
    while (!p_is_eof(p) && !p_starts_with_ci(p, "</script")) p->pos++;
    unsigned int raw_len = p->pos - start;

    char *buf = (char*)web_malloc(raw_len + 1);
    if (!buf) return;
    memcpy(buf, p->src + start, raw_len);
    buf[raw_len] = '\0';

    web_node_t *tn = node_new(WEB_NODE_TEXT);
    if (tn) {
        tn->text = buf;
        node_append_child(el, tn);
    } else {
        web_free(buf);
    }

    /* kapanış etiketinin artıklarını (") atla */
    while (!p_is_eof(p) && p_peek(p) != '>') p->pos++;
    if (!p_is_eof(p)) p->pos++;
}

web_document_t *web_parse_html(const char *html, unsigned int len) {
    web_document_t *doc = (web_document_t*)web_malloc(sizeof(web_document_t));
    if (!doc) return NULL;
    memset(doc, 0, sizeof(web_document_t));

    web_node_t *root = node_new(WEB_NODE_ELEMENT);
    if (!root) { web_free(doc); return NULL; }
    root->tag = web_strdup("html");
    doc->root = root;

    /* açık eleman yığını */
    web_node_t *stack[128];
    int depth = 0;
    stack[depth++] = root;

    parser_t p;
    p.src = html; p.pos = 0; p.len = len;


    while (!p_is_eof(&p)) {
        char c = p_peek(&p);

        if (c == '<') {
            if (p_starts_with(&p, "<!--")) {
                /* yorum: --> kadar atla */
                p.pos += 4;
                while (!p_is_eof(&p) && !p_starts_with(&p, "-->")) p.pos++;
                if (!p_is_eof(&p)) p.pos += 3;
                continue;
            }
            if (p_starts_with(&p, "<!")) {
                /* DOCTYPE vb: > kadar atla */
                while (!p_is_eof(&p) && p_peek(&p) != '>') p.pos++;
                if (!p_is_eof(&p)) p.pos++;
                continue;
            }
            if (p_starts_with(&p, "</")) {
                p.pos += 2;
                char *name = read_tag_name(&p);
                while (!p_is_eof(&p) && p_peek(&p) != '>') p.pos++;
                if (!p_is_eof(&p)) p.pos++;

                if (name) {
                    /* yığından eşleşen etiketi kapat; kökü asla kapatma
                       (depth=0 olursa stack[depth-1] geçersiz adres okur) */
                    for (int i = depth - 1; i >= 1; i--) {
                        if (stack[i] && tag_ci_eq(stack[i]->tag, name)) {
                            depth = i;
                            break;
                        }
                    }
                    web_free(name);
                }
                continue;
            }

            /* açılış etiketi */
            p.pos++;   /* '<' */
            char *name = read_tag_name(&p);
            if (!name) { p.pos++; continue; }

            int is_self_close = 0;

            /* html/head/body tekrarları: zaten açıksa yeni düğüm açma */
            if ((tag_ci_eq(name, "html") || tag_ci_eq(name, "head") || tag_ci_eq(name, "body"))) {
                int dup = 0;
                for (int i = depth - 1; i >= 0; i--)
                    if (stack[i] && tag_ci_eq(stack[i]->tag, name)) { dup = 1; break; }
                if (dup) {
                    while (!p_is_eof(&p) && p_peek(&p) != '>') p.pos++;
                    if (!p_is_eof(&p)) p.pos++;
                    web_free(name);
                    continue;
                }
            }

            web_node_t *el = node_new(WEB_NODE_ELEMENT);
            if (el) {
                el->tag = name;
                name = NULL;
                read_attributes(&p, el);

                /* kendi kapanma: <tag/> veya void */
                p_skip_ws(&p);
                if (p_peek(&p) == '/') {
                    p.pos++;
                    is_self_close = 1;
                }
                while (!p_is_eof(&p) && p_peek(&p) != '>') p.pos++;
                if (!p_is_eof(&p)) p.pos++;

                web_node_t *parent = depth > 0 ? stack[depth - 1] : root;
                node_append_child(parent, el);

                /* title içeriğini yakala */
                if (tag_ci_eq(el->tag, "title")) capture_title(doc, &p);

                /* script içeriği raw alınır; yığına itilmez */
                if (tag_ci_eq(el->tag, "script") && !is_self_close) {
                    capture_script(&p, el);
                    is_self_close = 1;
                }

                if (!is_self_close && !is_void_tag(el->tag) && depth < 128) {
                    /* otomatik kapanma: aynı blok etiketleri */
                    if (auto_close_same(el->tag) &&
                        depth > 1 && stack[depth - 1] &&
                        tag_ci_eq(stack[depth - 1]->tag, el->tag)) {
                        depth--;
                    }
                    if (auto_close_heading(el->tag) && depth > 1) {
                        /* üstte bir başlık varsa kapat */
                        for (int i = depth - 1; i >= 1; i--) {
                            if (is_block_heading(stack[i]->tag)) { depth = i; break; }
                        }
                    }
                    stack[depth++] = el;
                } else if (name) {
                    web_free(name);
                }
            } else {
                web_free(name);
            }
            continue;
        }

        /* metin */
        web_node_t *tn = make_text_node(&p);
        if (tn) {
            web_node_t *parent = depth > 0 ? stack[depth - 1] : root;
            node_append_child(parent, tn);
        }
    }

    return doc;
}

void web_free_document(web_document_t *doc) {
    if (!doc) return;
    if (doc->root) node_free(doc->root);
    if (doc->title) web_free(doc->title);
    web_free(doc);
}

/* ─── DOM erişimi ───────────────────────────────────────── */

web_node_t *web_get_element_by_id(const web_node_t *node, const char *id) {
    web_node_t *c;
    if (!node) return NULL;
    if (node->type == WEB_NODE_ELEMENT && node->id && strcmp(node->id, id) == 0)
        return (web_node_t*)node;
    for (c = node->first_child; c; c = c->next_sibling) {
        web_node_t *r = web_get_element_by_id(c, id);
        if (r) return r;
    }
    return NULL;
}

int web_get_elements_by_tag(const web_node_t *node, const char *tag, web_node_t **out, int max) {
    int count = 0;
    web_node_t *c;
    if (!node) return 0;
    if (node->type == WEB_NODE_ELEMENT && tag_ci_eq(node->tag, tag) && count < max)
        out[count++] = (web_node_t*)node;
    for (c = node->first_child; c && count < max; c = c->next_sibling)
        count += web_get_elements_by_tag(c, tag, out + count, max - count);
    return count;
}

int web_node_has_class(const web_node_t *n, const char *cls) {
    const char *v;
    if (!n) return 0;
    v = web_node_attr(n, "class");
    if (!v) return 0;
    /* boşlukla ayrılmış sınıflar içinde ara */
    while (*v) {
        while (*v == ' ' || *v == '\t') v++;
        if (*v == '\0') break;
        const char *start = v;
        while (*v && *v != ' ' && *v != '\t') v++;
        if ((int)(v - start) == (int)strlen(cls) && strncmp(start, cls, (size_t)(v - start)) == 0)
            return 1;
    }
    return 0;
}
