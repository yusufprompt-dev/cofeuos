/*
 * ============================================================================
 * TLS_CRYPTO.H - CofeuOS TLS ortak kriptografik tipler
 * (tls_prf.c ve tls_record.c arasinda paylasilir)
 * ============================================================================
 */

#ifndef TLS_CRYPTO_H
#define TLS_CRYPTO_H

#include "sha256.h"

/* Incremental HMAC-SHA256 context'i (buyuk TLS record MAC'leri icin) */
typedef struct {
    sha256_context inner;
    uint8_t kp[64];
} hmac_sha256_ctx;

#endif /* TLS_CRYPTO_H */
