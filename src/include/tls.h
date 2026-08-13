/*
 * ============================================================================
 * TLS.H - CofeuOS TLS 1.2 istemcisi (TLS_RSA_WITH_AES_128_CBC_SHA256)
 *
 * malloc yok; tum buffer'lar sabit boyutlu (hafif OS tasarimi).
 * IO soyutlamasi: tls_net_t callback'leri ile agdan bagimsizdir.
 * network.c'deki raw TCP katmani bu callback'leri baglar.
 * ============================================================================
 */

#ifndef TLS_H
#define TLS_H

#include <stdint.h>
#include <stddef.h>
#include "sha256.h"

/* ---- limitler (saldirgan/bozuk sunucuya karsi ust sinirlar) ---- */
#define TLS_MAX_HS_MSG       32768                  /* tek handshake mesaji ust siniri */
#define TLS_REC_MAX_PAYLOAD  (16384 + 16 + 32 + 256) /* iv+mac+pad payi */
#define TLS_VERIFY_DATA_LEN  12
#define TLS_MASTER_LEN       48

/* record content type'lari */
#define CT_CHANGE_CIPHER_SPEC 20
#define CT_ALERT              21
#define CT_HANDSHAKE          22
#define CT_APPLICATION_DATA   23

/* hata kodlari */
typedef enum {
    TLS_OK = 0,
    TLS_ERR_IO = -1,           /* send/recv hatasi (timeout dahil) */
    TLS_ERR_PROTOCOL = -2,     /* yapisal/protokol ihlali */
    TLS_ERR_ALERT_FATAL = -3,  /* fatal alert alindi (last_alert_desc) */
    TLS_ERR_CRYPTO = -4,       /* MAC/finished/RSA dogrulama hatasi */
    TLS_ERR_MSG_TOO_LARGE = -5 /* buffer ust siniri asildi */
} tls_err_t;

/* ag soyutlamasi: callback'ler tam veriyi okur/yazar, hata -1 */
typedef struct tls_net {
    void *ctx;
    int (*send)(void *ctx, const uint8_t *data, size_t len);
    int (*recv)(void *ctx, uint8_t *data, size_t len);   /* tam len al, timeout -1 */
    void (*get_random)(void *ctx, uint8_t *out, size_t len);
} tls_net_t;

typedef struct tls_client {
    tls_net_t net;

    uint8_t client_random[32];
    uint8_t server_random[32];
    uint8_t session_id[32];
    size_t  session_id_len;

    uint8_t premaster_random[46];
    uint8_t premaster[48];
    uint8_t master[48];

    uint8_t client_mac_key[32], server_mac_key[32];
    uint8_t client_enc_key[16], server_enc_key[16];

    uint64_t write_seq, read_seq;

    uint8_t hs_buf[TLS_MAX_HS_MSG];     /* alinan handshake mesaj tamponu */
    size_t  hs_off, hs_len;
    uint8_t rec_buf[TLS_REC_MAX_PAYLOAD];

    sha256_context hs_hash;             /* kumulatif handshake SHA-256 */

    int   state;                        /* TLS_STATE_* */
    int   step;                         /* son tamamlanan handshake adimi */
    int   error;                        /* son hata kodu */
    uint8_t last_alert_level, last_alert_desc;

    uint8_t peer_cert_hash[32];         /* leaf sertifika SHA-256 (TOFU icin) */
    int     peer_cert_hash_valid;
    const char *sni_host;               /* ClientHello SNI (NULL: extension yok) */
} tls_client_t;

#define TLS_STATE_INIT        0
#define TLS_STATE_CONNECTED   1
#define TLS_STATE_FAILED      2

/* ---- genel API ---- */

void tls_client_init(tls_client_t *c, const tls_net_t *net,
                     const uint8_t client_random[32],
                     const uint8_t premaster_random[46]);
int  tls_client_handshake(tls_client_t *c);
int  tls_client_write(tls_client_t *c, const uint8_t *data, size_t len);
int  tls_client_read(tls_client_t *c, uint8_t *out, size_t cap, size_t *out_len);
int  tls_client_last_alert(tls_client_t *c, uint8_t *level, uint8_t *desc);
const char *tls_err_str(int err);

#endif /* TLS_H */
