/*
 * JS.H - CofeuTarayici JavaScript Motoru API
 *
 * Faz 2: DOM erisimi. Sayfa yuklenirken <script> bloklari js_run ile
 * calistirilir; document/window/element objeleri web.h'deki DOM agacini
 * sarmalar. js_set_page ile motor, o anki belgeye baglanir.
 */
#ifndef _JS_H
#define _JS_H

#include "types.h"

/* ─── Dis olay geri cagrimlari ──────────────────────────── */
typedef void (*js_log_fn)(const char *msg);              /* console.log/error */
typedef void (*js_alert_fn)(const char *msg);            /* alert() */
typedef int  (*js_prompt_fn)(const char *msg, char *out, int out_size); /* prompt(): 1=ok 0=iptal */
typedef int  (*js_confirm_fn)(const char *msg);          /* confirm(): 1=evet 0=hayir */

/* ─── Motor ─────────────────────────────────────────────── */
/* Ilk cagrimda bir kez cagrilir; geri cagrimlari ayarlar. */
void js_init(void);

/* Butun JS bellegini temizler ve global ortami sifirlar.
   Her yeni sayfa yuklemesinde cagrilir. */
void js_reset(void);

/* Kaynak kodu calistirir. 0 basarili, 0 degilse hata (log'a yazilir). */
int js_run(const char *src, int len);

void js_set_log_cb(js_log_fn fn);
void js_set_alert_cb(js_alert_fn fn);
void js_set_prompt_cb(js_prompt_fn fn);
void js_set_confirm_cb(js_confirm_fn fn);

/* ─── DOM (Faz 2) ───────────────────────────────────────── */
/* Motoru o anki belgeye baglar: doc=web_document_t*, css=web_css_rule_t*,
   url=tarayicinin adres cubugu (window.location icin). */
void js_set_page(void *doc, void *css, const char *url);

/* window.location.href = "..." istegi varsa URL'yi dondurur ("" yoksa). */
const char *js_get_pending_nav(void);
void js_clear_pending_nav(void);

/* ─── Olaylar ve zamanlayicilar (Faz 3) ─────────────────── */
/* Tıklanan DOM düğümüne click olayını iletir; bir işleyici çalıştıysa 1. */
int js_dispatch_click(void *node);
/* Masaüstü saatinden geçen süreyi bildirir; çalışan zamanlayıcı sayısını döner. */
int js_tick(unsigned int elapsed_ms);

#endif /* _JS_H */
