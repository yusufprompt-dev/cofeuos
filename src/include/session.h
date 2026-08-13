/*
 * ============================================================================
 * SESSION.H - Kalıcı Oturum (Login) Hafızası
 *
 * Giriş bilgilerini RAM yerine kalıcı depolamaya yazar:
 *   1) EFI System Partition üzerindeki user_session.json dosyası
 *   2) UEFI NVRAM değişkeni (yığın depolama bulunmadığında)
 *   3) RAM dosya sistemindeki /etc/user_session.json aynası
 *
 * Parola, SHA-256 bütünlük özeti ile birlikte XOR karartmalı (obfuscated)
 * biçimde saklanır; düz metin olarak diske asla yazılmaz.
 * ============================================================================
 */

#ifndef _SESSION_H
#define _SESSION_H

#include "types.h"

#define SESSION_USER_MAX  32
#define SESSION_PASS_MAX  64
#define SESSION_HASH_HEX  65

typedef struct {
    char username[SESSION_USER_MAX];     /* giriş yapan kullanıcı */
    char password[SESSION_PASS_MAX];     /* çözülmüş parola (yalnızca RAM) */
    char password_hash[SESSION_HASH_HEX];/* SHA-256 hex özeti */
    int  valid;                          /* geçerli kayıtlı oturum var mı */
} login_session;

/* Kayıtlı oturumu okur; varsa 1, yoksa 0 döner. */
int  session_load(login_session *s);

/* Giriş bilgilerini güvenli biçimde kalıcı depolamaya yazar. */
int  session_save(const char *user, const char *pass);

/* Kayıtlı oturumu tüm kalıcı katmanlardan siler. */
void session_clear(void);

#endif /* _SESSION_H */
