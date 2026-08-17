/*
 * ============================================================================
 * BIGNUM.H - CofeuOS buyuk sayi aritmetigi (rsa.c / ec.c paylasir)
 *
 * malloc yok; sabit boyutlu buffer. Bignum: uint32_t[128] kelime dizisi.
 * 128 kelime = 4096 bit: 2048-bit moduli ile yapilan carpim sonucu (2x) icin.
 * Temsil: kucuk endian kelimeler, n = kullanilan kelime sayisi.
 * ============================================================================
 */

#ifndef BIGNUM_H
#define BIGNUM_H

#include <stdint.h>

#define BN_MAX_WORDS 128
#define BN_MAX_BYTES (BN_MAX_WORDS * 4)

typedef struct {
    uint32_t d[BN_MAX_WORDS];
    int n;                    /* kullanilan kelime sayisi (little-endian) */
} bignum_t;

void bn_zero(bignum_t *a);
void bn_copy(bignum_t *dst, const bignum_t *src);
int  bn_cmp(const bignum_t *a, const bignum_t *b);
void bn_trim(bignum_t *a);
void bn_from_bytes(bignum_t *a, const unsigned char *bytes, int len);
void bn_to_bytes(const bignum_t *a, unsigned char *bytes, int len);
int  bn_bitlen(const bignum_t *a);

void bignum_add(const bignum_t *a, const bignum_t *b, bignum_t *r);
void bignum_sub(const bignum_t *a, const bignum_t *b, bignum_t *r);
void bignum_mul(const bignum_t *a, const bignum_t *b, bignum_t *r);
void bignum_mod(const bignum_t *a, const bignum_t *m, bignum_t *r);
void bignum_modpow(const bignum_t *base, const bignum_t *exp,
                   const bignum_t *mod, bignum_t *result);

/* kelime bazli sola kaydirma (bignum.c icin yardimci) */
void bn_lshift_words(bignum_t *a, int words);

#endif /* BIGNUM_H */
