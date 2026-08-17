/*
 * CofeuOS TLS - Bilesen 7: Eliptik egri secp256r1 (NIST P-256)
 *
 * TLS 1.2 ECDHE_RSA key exchange icin: ephemeral anahtar uretimi ve
 * ECDH paylasilan gizli (premaster secret) hesabi.
 *
 * Buyuk sayi: bignum.h (rsa.c ile paylasilan genel bignum).
 * Indirgeme: bignum_mod (bolme tabanli, dogru ve test edilmis).
 *
 * malloc yok; sabit boyutlu buffer'lar.
 */
#include <stdint.h>
#include "../include/string.h"
#include "../include/bignum.h"
#include "../include/ec.h"

/* secp256r1 parametreleri (RFC 8422 / SEC 2) */
static const unsigned char P256_P[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x01,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};
static const unsigned char P256_B[32] = {
    0x5A,0xC6,0x35,0xD8,0xAA,0x3A,0x93,0xE7,
    0xB3,0xEB,0xBD,0x55,0x76,0x98,0x86,0xBC,
    0x65,0x1D,0x06,0xB0,0xCC,0x53,0xB0,0xF6,
    0x3B,0xCE,0x3C,0x3E,0x27,0xD2,0x60,0x4B
};
static const unsigned char P256_N[32] = {
    0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xBC,0xE6,0xFA,0xAD,0xA7,0x17,0x9E,0x84,
    0xF3,0xB9,0xCA,0xC2,0xFC,0x63,0x25,0x51
};
static const unsigned char P256_GX[32] = {
    0x6B,0x17,0xD1,0xF2,0xE1,0x2C,0x42,0x47,
    0xF8,0xBC,0xE6,0xE5,0x63,0xA4,0x40,0xF2,
    0x77,0x03,0x7D,0x81,0x2D,0xEB,0x33,0xA0,
    0xF4,0xA1,0x39,0x45,0xD8,0x98,0xC2,0x96
};
static const unsigned char P256_GY[32] = {
    0x4F,0xE3,0x42,0xE2,0xFE,0x1A,0x7F,0x9B,
    0x8E,0xE7,0xEB,0x4A,0x7C,0x0F,0x9E,0x16,
    0x2B,0xCE,0x33,0x57,0x6B,0x31,0x5E,0xCE,
    0xCB,0xB6,0x40,0x68,0x37,0xBF,0x51,0xF5
};

static bignum_t g_p, g_b, g_n, g_Gx, g_Gy, g_pm2, g_two;

static void ec_init(void) {
    static int done = 0;
    if (done) return;
    done = 1;
    bn_from_bytes(&g_p,  P256_P,  32);
    bn_from_bytes(&g_b,  P256_B,  32);
    bn_from_bytes(&g_n,  P256_N,  32);
    bn_from_bytes(&g_Gx, P256_GX, 32);
    bn_from_bytes(&g_Gy, P256_GY, 32);
    bn_zero(&g_two);
    g_two.d[0] = 2;
    g_two.n = 1;
    bignum_sub(&g_p, &g_two, &g_pm2);   /* p - 2 */
}

/* ---- alan islemleri (tum operand'lar < p) ---- */

static void fe_mul(bignum_t *r, const bignum_t *a, const bignum_t *b) {
    bignum_t tmp;
    bignum_mul(a, b, &tmp);
    bignum_mod(&tmp, &g_p, r);
}

static void fe_sqr(bignum_t *r, const bignum_t *a) {
    fe_mul(r, a, a);
}

static void fe_add(bignum_t *r, const bignum_t *a, const bignum_t *b) {
    bignum_add(a, b, r);
    bignum_mod(r, &g_p, r);
}

static void fe_sub(bignum_t *r, const bignum_t *a, const bignum_t *b) {
    if (bn_cmp(a, b) >= 0) {
        bignum_sub(a, b, r);
    } else {
        bignum_sub(b, a, r);       /* r = b - a (>0) */
        bignum_sub(&g_p, r, r);    /* r = p - (b-a) -> (a-b) mod p */
    }
}

/* ---- egri noktalari ---- */

typedef struct { bignum_t X, Y, Z; } ec_jac_t;   /* Jacobian koordinatlar */
typedef struct { bignum_t x, y; }   ec_aff_t;    /* afin (x, y) */

/* Jacobian 2x noktasi (dbl-2009-l, a = -3) */
static void jac_double(ec_jac_t *R, const ec_jac_t *P) {
    bignum_t A, B, C, D, E, F, Z1Z1, tmp, ztmp;
    bignum_t Y1, Z1;  /* save inputs for Z3 computation */
    bn_copy(&Y1, &P->Y);
    bn_copy(&Z1, &P->Z);
    fe_sqr(&A, &P->X);                    /* A = X1^2 */
    fe_sqr(&B, &P->Y);                    /* B = Y1^2 */
    fe_sqr(&C, &B);                       /* C = Y1^4 */
    fe_add(&tmp, &P->X, &B);
    fe_sqr(&tmp, &tmp);                   /* (X1+B)^2 */
    fe_sub(&tmp, &tmp, &A);
    fe_sub(&tmp, &tmp, &C);
    fe_add(&D, &tmp, &tmp);               /* D = 2*((X1+B)^2 - A - C) */
    fe_sqr(&Z1Z1, &P->Z);                 /* Z1^2 */
    fe_sqr(&Z1Z1, &Z1Z1);                 /* Z1^4 */
    fe_add(&E, &A, &A);
    fe_add(&E, &E, &A);                   /* E = 3*A */
    fe_sub(&E, &E, &Z1Z1);
    fe_sub(&E, &E, &Z1Z1);
    fe_sub(&E, &E, &Z1Z1);                /* E = 3*A - 3*Z1^4 */
    fe_sqr(&F, &E);                       /* F = E^2 */
    fe_add(&R->X, &D, &D);
    fe_sub(&R->X, &F, &R->X);             /* X3 = F - 2*D */
    fe_sub(&tmp, &D, &R->X);
    fe_mul(&tmp, &E, &tmp);               /* E*(D - X3) */
    fe_add(&R->Y, &C, &C);
    fe_add(&R->Y, &R->Y, &R->Y);
    fe_add(&R->Y, &R->Y, &R->Y);          /* 8*C */
    fe_sub(&R->Y, &tmp, &R->Y);           /* Y3 */

    /* Z3 = 2*Y1*Z1 - use saved Y1, Z1 to avoid aliasing when R==P */
    fe_mul(&ztmp, &Y1, &Z1);
    fe_add(&R->Z, &ztmp, &ztmp);          /* Z3 = 2*Y1*Z1 */
}

/* Jacobian + afin (add-2007-bl) */
static void jac_add_affine(ec_jac_t *R, const ec_jac_t *P, const ec_aff_t *Q) {
    bignum_t Z1Z1, U2, S2, H, HH, I, J, r, V, tmp;
    fe_sqr(&Z1Z1, &P->Z);
    fe_mul(&U2, &Q->x, &Z1Z1);            /* U2 = X2*Z1Z1 */
    fe_mul(&tmp, &P->Z, &Z1Z1);
    fe_mul(&S2, &Q->y, &tmp);             /* S2 = Y2*Z1*Z1Z1 */
    fe_sub(&H, &U2, &P->X);
    fe_sqr(&HH, &H);
    fe_add(&I, &HH, &HH);
    fe_add(&I, &I, &I);                   /* I = 4*HH */
    fe_mul(&J, &H, &I);
    fe_sub(&tmp, &S2, &P->Y);
    fe_add(&r, &tmp, &tmp);               /* r = 2*(S2 - Y1) */
    fe_mul(&V, &P->X, &I);
    fe_sqr(&tmp, &r);
    fe_add(&R->X, &V, &V);
    fe_add(&R->X, &R->X, &J);
    fe_sub(&R->X, &tmp, &R->X);           /* X3 = r^2 - J - 2*V */
    fe_sub(&tmp, &V, &R->X);
    fe_mul(&tmp, &r, &tmp);
    fe_mul(&R->Y, &P->Y, &J);
    fe_add(&R->Y, &R->Y, &R->Y);
    fe_sub(&R->Y, &tmp, &R->Y);           /* Y3 = r*(V-X3) - 2*Y1*J */
    fe_add(&tmp, &P->Z, &H);
    fe_sqr(&tmp, &tmp);
    fe_sub(&tmp, &tmp, &Z1Z1);
    fe_sub(&R->Z, &tmp, &HH);             /* Z3 = (Z1+H)^2 - Z1Z1 - HH */
}

static void jac_from_affine(ec_jac_t *J, const ec_aff_t *P) {
    bn_copy(&J->X, &P->x);
    bn_copy(&J->Y, &P->y);
    bn_zero(&J->Z);
    J->Z.d[0] = 1;
    J->Z.n = 1;
}

static int jac_to_affine(ec_aff_t *R, const ec_jac_t *J) {
    if (J->Z.n == 0 || (J->Z.n == 1 && J->Z.d[0] == 0)) return -1; /* infinity */
    bignum_t zinv, zinv2;
    bignum_modpow(&J->Z, &g_pm2, &g_p, &zinv);   /* zinv = Z^(p-2) */
    fe_sqr(&zinv2, &zinv);
    fe_mul(&R->x, &J->X, &zinv2);
    fe_mul(&R->y, &J->Y, &zinv2);
    fe_mul(&R->y, &R->y, &zinv);
    return 0;
}

static int bn_bit(const bignum_t *a, int bit) {
    int w = bit / 32;
    int b = bit % 32;
    return (w < a->n) ? ((a->d[w] >> b) & 1) : 0;
}

/* R = k*P (double-and-add, MSB'den) */
static void ec_scalar_mult(ec_aff_t *R, const bignum_t *k, const ec_aff_t *P) {
    ec_jac_t J;
    int started = 0;
    for (int i = bn_bitlen(k) - 1; i >= 0; i--) {
        if (started) jac_double(&J, &J);
        if (bn_bit(k, i)) {
            if (started) jac_add_affine(&J, &J, P);
            else { jac_from_affine(&J, P); started = 1; }
        }
    }
    if (!started) {
        bn_zero(&R->x);
        bn_zero(&R->y);
        return;
    }
    if (jac_to_affine(R, &J) != 0) {
        bn_zero(&R->x);
        bn_zero(&R->y);
    }
}

/* nokta egri uzerinde mi?  y^2 = x^3 - 3x + b (mod p) */
static int point_on_curve(const bignum_t *x, const bignum_t *y) {
    bignum_t lhs, rhs, t;
    fe_sqr(&lhs, y);
    fe_sqr(&t, x);
    fe_mul(&t, &t, x);                 /* x^3 */
    fe_add(&rhs, &t, &g_b);            /* x^3 + b */
    fe_add(&t, x, x);
    fe_add(&t, &t, x);                 /* 3x */
    fe_sub(&rhs, &rhs, &t);            /* x^3 - 3x + b */
    return bn_cmp(&lhs, &rhs) == 0;
}

static int bn_is_zero(const bignum_t *a) {
    return a->n == 0 || (a->n == 1 && a->d[0] == 0);
}

int ec_p256_keypair(ec_rng_t rng, void *rng_ctx,
                    uint8_t priv[EC_FIELD_BYTES],
                    uint8_t pub[EC_POINT_UNCOMPRESSED]) {
    if (!rng) return -1;
    ec_init();
    bignum_t k;
    do {
        uint8_t rb[EC_FIELD_BYTES];
        rng(rng_ctx, rb, sizeof rb);
        bn_from_bytes(&k, rb, sizeof rb);
        bignum_mod(&k, &g_n, &k);
    } while (bn_is_zero(&k));

    ec_aff_t G;
    bn_copy(&G.x, &g_Gx);
    bn_copy(&G.y, &g_Gy);
    ec_aff_t P;
    ec_scalar_mult(&P, &k, &G);

    bn_to_bytes(&k, priv, EC_FIELD_BYTES);
    pub[0] = 0x04;
    bn_to_bytes(&P.x, pub + 1, EC_FIELD_BYTES);
    bn_to_bytes(&P.y, pub + 1 + EC_FIELD_BYTES, EC_FIELD_BYTES);
    return 0;
}

int ec_p256_ecdh(const uint8_t priv[EC_FIELD_BYTES],
                 const uint8_t peer_pub[EC_POINT_UNCOMPRESSED],
                 uint8_t shared[EC_FIELD_BYTES]) {
    if (!priv || !peer_pub || !shared) return -1;
    ec_init();
    if (peer_pub[0] != 0x04) return -1;

    bignum_t k, x, y;
    bn_from_bytes(&k, priv, EC_FIELD_BYTES);
    if (bn_is_zero(&k) || bn_cmp(&k, &g_n) >= 0) return -1;
    bn_from_bytes(&x, peer_pub + 1, EC_FIELD_BYTES);
    bn_from_bytes(&y, peer_pub + 1 + EC_FIELD_BYTES, EC_FIELD_BYTES);
    if (bn_cmp(&x, &g_p) >= 0 || bn_cmp(&y, &g_p) >= 0) return -1;
    if (!point_on_curve(&x, &y)) return -1;

    ec_aff_t Q, S;
    bn_copy(&Q.x, &x);
    bn_copy(&Q.y, &y);
    ec_scalar_mult(&S, &k, &Q);
    bn_to_bytes(&S.x, shared, EC_FIELD_BYTES);
    return 0;
}

#ifdef EC_TEST_MAIN
/* ---- native gcc testi ---- */
#include <stdio.h>

static void print_hex(const char *label, const unsigned char *d, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) printf("%02x", d[i]);
    printf("\n");
}

static int hex_cmp(const unsigned char *a, const unsigned char *b, int n) {
    for (int i = 0; i < n; i++) if (a[i] != b[i]) return -1;
    return 0;
}

/* deterministik rng: sadece bir kez cagrilir, sabit deger dondurur */
static unsigned char g_fixed[32];
static void fixed_rng(void *ctx, uint8_t *out, size_t len) {
    (void)ctx;
    for (size_t i = 0; i < len; i++) out[i] = g_fixed[i];
}

int main(void) {
    int fail = 0;
    ec_init();
    uint8_t o[32];

    /* 0. Generator noktasi egri uzerinde mi? (alan aritmetigi sinavi) */
    {
        bignum_t gx, gy;
        bn_from_bytes(&gx, P256_GX, 32);
        bn_from_bytes(&gy, P256_GY, 32);
        if (point_on_curve(&gx, &gy) != 1) {
            printf("FAIL: G egri uzerinde degil (alan aritmetigi!)\n"); fail = 1;
        } else printf("OK: G egri uzerinde\n");

        /* alan aritmetigi vektorleri (python) */
        {
            bignum_t r;
            fe_add(&r, &gx, &gy);
            static const unsigned char want_add[32] = {
                0xBA,0xFB,0x14,0xD5,0xDF,0x46,0xC1,0xE3,
                0x87,0xA4,0xD2,0x2F,0xDF,0xB3,0xDF,0x08,
                0xA2,0xD1,0xB0,0xD8,0x99,0x1C,0x92,0x6F,
                0xC0,0x57,0x79,0xAE,0x10,0x58,0x14,0x8B
            };
            bn_to_bytes(&r, o, 32);
            if (memcmp(o, want_add, 32) != 0) { printf("FAIL: fe_add\n"); fail = 1; }
            else printf("OK: fe_add\n");

            fe_mul(&r, &gx, &gy);
            static const unsigned char want_mul[32] = {
                0x82,0x3C,0xD1,0x5F,0x6D,0xD3,0xC7,0x19,
                0x33,0x56,0x50,0x64,0x51,0x3A,0x6B,0x2B,
                0xD1,0x83,0xE5,0x54,0xC6,0xA0,0x86,0x22,
                0xF7,0x13,0xEB,0xBB,0xFA,0xCE,0x98,0xBE
            };
            bn_to_bytes(&r, o, 32);
            if (memcmp(o, want_mul, 32) != 0) { printf("FAIL: fe_mul\n"); fail = 1; }
            else printf("OK: fe_mul\n");

            fe_sqr(&r, &gy);
            static const unsigned char want_sqr[32] = {
                0x55,0xDF,0x5D,0x58,0x50,0xF4,0x7B,0xAD,
                0x82,0x14,0x91,0x39,0x97,0x93,0x69,0xFE,
                0x49,0x8A,0x90,0x22,0xA4,0x12,0xB5,0xE0,
                0xBE,0xDD,0x2C,0xFC,0x21,0xC3,0xED,0x91
            };
            bn_to_bytes(&r, o, 32);
            if (memcmp(o, want_sqr, 32) != 0) { printf("FAIL: fe_sqr\n"); fail = 1; }
            else printf("OK: fe_sqr\n");

            fe_sqr(&r, &gx);
            static const unsigned char want_sqr_gx[32] = {
                0x98,0xF6,0xB8,0x4D,0x29,0xBE,0xF2,0xB2,
                0x81,0x81,0x9A,0x5E,0x0E,0x36,0x90,0xD8,
                0x33,0xB6,0x99,0x49,0x5D,0x69,0x4D,0xD1,
                0x00,0x2A,0xE5,0x6C,0x42,0x6B,0x3F,0x8C
            };
            bn_to_bytes(&r, o, 32);
            if (memcmp(o, want_sqr_gx, 32) != 0) { printf("FAIL: fe_sqr gx\n"); fail = 1; }
            else printf("OK: fe_sqr gx\n");

            fe_sub(&r, &gx, &gx);            /* Gx - Gx = 0 */
            if (!bn_is_zero(&r)) { printf("FAIL: fe_sub a-a\n"); fail = 1; }
            else printf("OK: fe_sub a-a\n");

            fe_sub(&r, &gx, &gy);            /* Gx - Gy (>0) */
            static const unsigned char want_subn[32] = {
                0x1B,0x34,0x8F,0x0F,0xE3,0x11,0xC2,0xAC,
                0x69,0xD4,0xFB,0x9A,0xE7,0x94,0xA2,0xDC,
                0x4B,0x35,0x4A,0x29,0xC2,0xB9,0xD4,0xD2,
                0x28,0xEA,0xF8,0xDD,0xA0,0xD9,0x70,0xA1
            };
            bn_to_bytes(&r, o, 32);
            if (memcmp(o, want_subn, 32) != 0) { printf("FAIL: fe_sub pozitif\n"); fail = 1; }
            else printf("OK: fe_sub pozitif\n");

            fe_sub(&r, &gy, &gx);            /* Gy - Gx (<0) -> mod p */
            static const unsigned char want_subp[32] = {
                0xE4,0xCB,0x70,0xEF,0x1C,0xEE,0x3D,0x54,
                0x96,0x2B,0x04,0x65,0x18,0x6B,0x5D,0x23,
                0xB4,0xCA,0xB5,0xD7,0x3D,0x46,0x2B,0x2D,
                0xD7,0x15,0x07,0x22,0x5F,0x26,0x8F,0x5E
            };
            bn_to_bytes(&r, o, 32);
            if (memcmp(o, want_subp, 32) != 0) { printf("FAIL: fe_sub negatif\n"); fail = 1; }
            else printf("OK: fe_sub negatif\n");
        }

        /* 2*G = G+G skalari ile dogrulama (python: 7cf27b18...) */
        ec_aff_t G, D;
        bn_copy(&G.x, &gx);
        bn_copy(&G.y, &gy);
        bignum_t two;
        bn_zero(&two);
        two.d[0] = 2;
        two.n = 1;
        ec_scalar_mult(&D, &two, &G);
        static const unsigned char want_2gx[32] = {
            0x7C, 0xF2, 0x7B, 0x18, 0x8D, 0x03, 0x4F, 0x7E,
            0x8A, 0x52, 0x38, 0x03, 0x04, 0xB5, 0x1A, 0xC3,
            0xC0, 0x89, 0x69, 0xE2, 0x77, 0xF2, 0x1B, 0x35,
            0xA6, 0x0B, 0x48, 0xFC, 0x47, 0x66, 0x99, 0x78,
        };
        static const unsigned char want_2gy[32] = {
            0x07, 0x77, 0x55, 0x10, 0xDB, 0x8E, 0xD0, 0x40,
            0x29, 0x3D, 0x9A, 0xC6, 0x9F, 0x74, 0x30, 0xDB,
            0xBA, 0x7D, 0xAD, 0xE6, 0x3C, 0xE9, 0x82, 0x29,
            0x9E, 0x04, 0xB7, 0x9D, 0x22, 0x78, 0x73, 0xD1,
        };
        bn_to_bytes(&D.x, o, 32);
        if (memcmp(o, want_2gx, 32) != 0) {
            printf("FAIL: 2G.x\n");
            print_hex("got ", o, 32);
            print_hex("want", want_2gx, 32);
            fail = 1;
        } else printf("OK: 2G.x\n");
        bn_to_bytes(&D.y, o, 32);
        if (memcmp(o, want_2gy, 32) != 0) {
            printf("FAIL: 2G.y\n");
            print_hex("got ", o, 32);
            print_hex("want", want_2gy, 32);
            fail = 1;
        } else printf("OK: 2G.y\n");
    }

    /* 1. NIST dogrulama vektoru: d -> Q */
    static const unsigned char d[] = {
        0xC9,0xAF,0xA9,0xD8,0x45,0xBA,0x75,0x16,
        0x6B,0x5C,0x21,0x57,0x67,0xB1,0xD6,0x93,
        0x4E,0x50,0xC3,0xDB,0x36,0xE8,0x9B,0x12,
        0x7B,0x8A,0x62,0x2B,0x12,0x0F,0x67,0x21
    };
    static const unsigned char Qx[] = {
        0x60,0xFE,0xD4,0xBA,0x25,0x5A,0x9D,0x31,
        0xC9,0x61,0xEB,0x74,0xC6,0x35,0x6D,0x68,
        0xC0,0x49,0xB8,0x92,0x3B,0x61,0xFA,0x6C,
        0xE6,0x69,0x62,0x2E,0x60,0xF2,0x9F,0xB6
    };
    static const unsigned char Qy[] = {
        0x79,0x03,0xFE,0x10,0x08,0xB8,0xBC,0x99,
        0xA4,0x1A,0xE9,0xE9,0x56,0x28,0xBC,0x64,
        0xF2,0xF1,0xB2,0x0C,0x2D,0x7E,0x9F,0x51,
        0x77,0xA3,0xC2,0x94,0xD4,0x46,0x22,0x99
    };
    memcpy(g_fixed, d, 32);
    uint8_t priv[32], pub[65];
    if (ec_p256_keypair(fixed_rng, NULL, priv, pub) != 0) {
        printf("FAIL: keypair\n"); return 1;
    }
    if (hex_cmp(priv, d, 32) != 0) { printf("FAIL: priv\n"); fail = 1; }
    else printf("OK: keypair priv (NIST)\n");
    if (hex_cmp(pub + 1, Qx, 32) != 0) { printf("FAIL: pub.x\n"); fail = 1; }
    else printf("OK: keypair pub.x (NIST)\n");
    if (hex_cmp(pub + 33, Qy, 32) != 0) { printf("FAIL: pub.y\n"); fail = 1; }
    else printf("OK: keypair pub.y (NIST)\n");

    /* 2. ECDH: d * Q beklenen paylasilan gizli ile karsilastirilir */
    uint8_t peer[65], shared[32];
    peer[0] = 0x04;
    memcpy(peer + 1, Qx, 32);
    memcpy(peer + 33, Qy, 32);
    int rc = ec_p256_ecdh(d, peer, shared);
    printf("ECDH rc=%d\n", rc);
    /* NIST vektoru icin python (cryptography) ile hesaplanmis deger */
    static const unsigned char exshared[32] = {
        0x23,0x88,0xEE,0x99,0x0C,0x93,0xC4,0xBB,
        0x75,0x72,0x03,0x22,0x5B,0x77,0x86,0xD6,
        0x99,0x50,0xD2,0xF0,0xDE,0x43,0xCD,0xF2,
        0x3D,0xC7,0x1F,0x5E,0xFA,0xA1,0x69,0xC8
    };
    if (rc != 0) { printf("FAIL: ecdh rc\n"); fail = 1; }
    else if (hex_cmp(shared, exshared, 32) != 0) {
        print_hex("got ", shared, 32);
        print_hex("want", exshared, 32);
        printf("FAIL: ecdh deger\n"); fail = 1;
    } else printf("OK: ecdh deger (python)\n");

    /* 3. ECDH simetrik: iki anahtar cifti ayni gizliyi vermeli */
    {
        static const unsigned char dA[] = {
            0xC9,0xAF,0xA9,0xD8,0x45,0xBA,0x75,0x16,
            0x6B,0x5C,0x21,0x57,0x67,0xB1,0xD6,0x93,
            0x4E,0x50,0xC3,0xDB,0x36,0xE8,0x9B,0x12,
            0x7B,0x8A,0x62,0x2B,0x12,0x0F,0x67,0x21
        };
        static const unsigned char dB[] = {
            0x01,0x10,0xDD,0x9E,0xF6,0x70,0x28,0xC7,
            0x2B,0x25,0xD8,0x23,0x6E,0x5E,0x0B,0x2D,
            0x28,0xF7,0xBD,0x8E,0x6D,0x7C,0x32,0xE0,
            0x44,0x44,0x22,0x91,0x9A,0xCB,0x44,0x8A
        };
        uint8_t pubA[65], pubB[65];
        memcpy(g_fixed, dA, 32);
        ec_p256_keypair(fixed_rng, NULL, priv, pubA);
        memcpy(g_fixed, dB, 32);
        ec_p256_keypair(fixed_rng, NULL, priv, pubB);
        uint8_t s1[32], s2[32];
        if (ec_p256_ecdh(dA, pubB, s1) != 0) { printf("FAIL: ecdh A*B\n"); fail = 1; }
        else if (ec_p256_ecdh(dB, pubA, s2) != 0) { printf("FAIL: ecdh B*A\n"); fail = 1; }
        else if (hex_cmp(s1, s2, 32) != 0) { printf("FAIL: ecdh simetrik\n"); fail = 1; }
        else printf("OK: ecdh simetrik\n");
    }

    /* 4. Gecersiz nokta reddi */
    {
        uint8_t bad[65];
        bad[0] = 0x04;
        memcpy(bad + 1, Qx, 32);
        for (int i = 0; i < 32; i++) bad[33 + i] = 0x00;  /* y=0: egri uzerinde degil */
        if (ec_p256_ecdh(d, bad, shared) == 0) {
            printf("FAIL: gecersiz nokta kabul edildi\n"); fail = 1;
        } else printf("OK: gecersiz nokta reddedildi\n");
    }

    printf(fail ? "\nSONUC: FAIL\n" : "\nSONUC: OK\n");
    return fail;
}
#endif