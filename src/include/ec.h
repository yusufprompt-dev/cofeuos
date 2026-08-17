/*
 * ============================================================================
 * EC.H - CofeuOS eliptik eğri kriptografisi (secp256r1 / NIST P-256)
 *
 * TLS 1.2 ECDHE_ECDSA yerine ECDHE_RSA key exchange icin kullanilir:
 *  - ec_p256_keypair: istemci ephemeral anahtar cifti uretir
 *  - ec_p256_ecdh:    paylasilan gizli (premaster) X-koordinati
 *
 * malloc yok; tum buffer'lar sabit boyutlu. buyuk sayi: bignum.h.
 * ============================================================================
 */

#ifndef EC_H
#define EC_H

#include <stdint.h>
#include <stddef.h>

#define EC_CURVE_SECP256R1   23   /* TLS named_curve id (RFC 8422) */
#define EC_FIELD_BYTES       32
#define EC_POINT_UNCOMPRESSED 65  /* 0x04 || X || Y */

/* rng(ctx, out, len): rastgele bayt uretici (NULL olmamali) */
typedef void (*ec_rng_t)(void *ctx, uint8_t *out, size_t len);

/*
 * secp256r1 ephemeral anahtar cifti uretir.
 * priv: 32 bayt, pub: 65 bayt (0x04 || X || Y). 0: basarili.
 */
int ec_p256_keypair(ec_rng_t rng, void *rng_ctx,
                    uint8_t priv[EC_FIELD_BYTES],
                    uint8_t pub[EC_POINT_UNCOMPRESSED]);

/*
 * ECDH: shared = X(priv * peer_pub).
 * peer_pub 65 bayt uncompressed (dogrulanir). 0: basarili; -1: gecersiz.
 */
int ec_p256_ecdh(const uint8_t priv[EC_FIELD_BYTES],
                 const uint8_t peer_pub[EC_POINT_UNCOMPRESSED],
                 uint8_t shared[EC_FIELD_BYTES]);

#endif /* EC_H */
