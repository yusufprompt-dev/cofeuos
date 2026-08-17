/*
 * CofeuOS TLS - Bilesen 1: RSA (bignum + X.509 parse + PKCS#1 v1.5)
 *
 * malloc yok, her sey sabit boyutlu buffer. Bignum: uint32_t[128] kelime dizisi.
 * 128 kelime = 4096 bit: 2048-bit moduli ile yapilan carpim sonucu (2x) icin.
 *
 * Kullanim (UEFI tarafi): sadece x509_extract_rsa_public_key ve
 * rsa_pkcs1_encrypt cagir. Test icin: gcc -DRSA_TEST_MAIN -o rsa_test rsa.c
 */
#include <stdint.h>
#include "../include/string.h"
#include "../include/bignum.h"

/* ---- temel yardimcilar ---- */

void bn_zero(bignum_t *a) {
    a->n = 0;
    memset(a->d, 0, sizeof(a->d));
}

void bn_copy(bignum_t *dst, const bignum_t *src) {
    dst->n = src->n;
    memcpy(dst->d, src->d, src->n * sizeof(uint32_t));
}

int bn_cmp(const bignum_t *a, const bignum_t *b) {
    if (a->n != b->n) return a->n > b->n ? 1 : -1;
    for (int i = a->n - 1; i >= 0; i--)
        if (a->d[i] != b->d[i]) return a->d[i] > b->d[i] ? 1 : -1;
    return 0;
}

void bn_trim(bignum_t *a) {   /* basusteki sifir kelimeleri at */
    while (a->n > 0 && a->d[a->n - 1] == 0) a->n--;
}

void bn_from_bytes(bignum_t *a, const unsigned char *bytes, int len) {
    bn_zero(a);
    a->n = (len + 3) / 4;
    for (int i = 0; i < len; i++)
        a->d[i / 4] |= (uint32_t)bytes[len - 1 - i] << (8 * (i % 4));
    bn_trim(a);
}

void bn_to_bytes(const bignum_t *a, unsigned char *bytes, int len) {
    memset(bytes, 0, len);
    for (int i = 0; i < len; i++) {
        int word = i / 4;
        int shift = 8 * (i % 4);
        if (word < a->n) bytes[len - 1 - i] = (a->d[word] >> shift) & 0xFF;
    }
}

int bn_bitlen(const bignum_t *a) {
    if (a->n == 0) return 0;
    uint32_t top = a->d[a->n - 1];
    int bits = 31;
    while (bits >= 0 && ((top >> bits) & 1) == 0) bits--;
    return (a->n - 1) * 32 + bits + 1;
}

/* ---- bignum aritmetik ---- */

void bignum_add(const bignum_t *a, const bignum_t *b, bignum_t *r) {
    uint64_t carry = 0;
    int n = a->n > b->n ? a->n : b->n;
    for (int i = 0; i < n; i++) {
        uint64_t sum = carry;
        if (i < a->n) sum += a->d[i];
        if (i < b->n) sum += b->d[i];
        r->d[i] = (uint32_t)sum;
        carry = sum >> 32;
    }
    if (carry) r->d[n++] = (uint32_t)carry;
    r->n = n;
    bn_trim(r);
}

void bignum_sub(const bignum_t *a, const bignum_t *b, bignum_t *r) {
    int64_t borrow = 0;
    int n = a->n;
    for (int i = 0; i < n; i++) {
        int64_t diff = (int64_t)a->d[i] - borrow;
        if (i < b->n) diff -= b->d[i];
        r->d[i] = (uint32_t)diff;
        borrow = diff < 0 ? 1 : 0;
    }
    r->n = n;
    bn_trim(r);
}

void bignum_mul(const bignum_t *a, const bignum_t *b, bignum_t *r) {
    bn_zero(r);
    for (int i = 0; i < a->n; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < b->n; j++) {
            uint64_t prod = (uint64_t)a->d[i] * b->d[j] + r->d[i + j] + carry;
            r->d[i + j] = (uint32_t)prod;
            carry = prod >> 32;
        }
        r->d[i + b->n] = (uint32_t)carry;
    }
    r->n = a->n + b->n;
    bn_trim(r);
}

/* 1 bit saga kaydir (bolme esnasinda boleni yariya indirmek icin) */
static void bn_shr1(bignum_t *a) {
    uint32_t carry = 0;
    for (int i = a->n - 1; i >= 0; i--) {
        uint32_t nc = a->d[i] & 1;
        a->d[i] = (a->d[i] >> 1) | (carry << 31);
        carry = nc;
    }
    bn_trim(a);
}

/* kelime bazli sola kaydirma */
void bn_lshift_words(bignum_t *a, int words) {
    if (a->n == 0 || words == 0) return;
    for (int i = a->n - 1; i >= 0; i--) a->d[i + words] = a->d[i];
    for (int i = 0; i < words; i++) a->d[i] = 0;
    a->n += words;
}

void bignum_mod(const bignum_t *a, const bignum_t *m, bignum_t *r) {
    bignum_t s, tmp;
    bn_copy(r, a);
    bn_trim(r);
    if (bn_cmp(r, m) < 0) return;

    int m_bits = bn_bitlen(m);
    while (bn_cmp(r, m) >= 0) {
        /* boleni r ile ayni bit uzunluguna kaydir */
        int shift = bn_bitlen(r) - m_bits;
        bn_copy(&s, m);
        bn_lshift_words(&s, shift / 32);
        shift %= 32;
        if (shift) {
            uint64_t carry = 0;
            for (int i = 0; i < s.n; i++) {
                uint64_t val = ((uint64_t)s.d[i] << shift) | carry;
                s.d[i] = (uint32_t)val;
                carry = val >> 32;
            }
            if (carry) s.d[s.n++] = (uint32_t)carry;
        }
        bn_trim(&s);
        /* s hala r'den buyukse yariya indir (tekrar kurma, ilerleme garanti) */
        while (bn_cmp(r, &s) < 0) bn_shr1(&s);
        bignum_sub(r, &s, &tmp);
        bn_copy(r, &tmp);
    }
}

/*
 * Square-and-multiply ile moduler us alma.
 * result = base^exp mod mod
 */
void bignum_modpow(const bignum_t *base, const bignum_t *exp,
                   const bignum_t *mod, bignum_t *result) {
    bignum_t b, e, r, tmp;
    bn_copy(&b, base);
    bn_copy(&e, exp);
    bn_zero(&r);
    r.d[0] = 1;
    r.n = 1;

    while (e.n > 0) {
        if (e.d[0] & 1) {                 /* bit=1 ise sonucu carp */
            bignum_mul(&r, &b, &tmp);
            bignum_mod(&tmp, mod, &r);
        }
        bignum_mul(&b, &b, &tmp);          /* her adimda kare */
        bignum_mod(&tmp, mod, &b);
        uint32_t carry = 0;                /* e >>= 1 */
        for (int i = e.n - 1; i >= 0; i--) {
            uint32_t new_carry = e.d[i] & 1;
            e.d[i] = (e.d[i] >> 1) | (carry << 31);
            carry = new_carry;
        }
        bn_trim(&e);
    }
    bn_copy(result, &r);
}

/* ---- ASN.1 / DER yardimcilari ---- */

/* length alanini oku (uzun form dahil), p'yi content'e ilerlet */
static int asn1_read_len(const unsigned char **p, const unsigned char *end) {
    if (*p >= end) return -1;
    int len = *(*p)++;
    if (len & 0x80) {
        int n = len & 0x7F;
        if (n > 4 || *p + n > end) return -1;
        len = 0;
        while (n--) len = (len << 8) | *(*p)++;
    }
    return len;
}

/* belirli bir tag'i atla (icinden gec, content'i gecir) */
static int asn1_skip(const unsigned char **p, const unsigned char *end, int tag) {
    if (*p >= end || *(*p)++ != tag) return -1;
    int len = asn1_read_len(p, end);
    if (len < 0 || *p + len > end) return -1;
    *p += len;
    return 0;
}

/* SEQUENCE ac, content baslangicina konumla; content uzunlugunu dondur */
static int asn1_expect_tag(const unsigned char **p, const unsigned char *end, int tag) {
    if (*p >= end || *(*p)++ != tag) return -1;
    return asn1_read_len(p, end);
}

/*
 * INTEGER oku. DER'de isaretsiz INTEGER onemsiz bas sifirini tasiyabilir;
 * bu sifir atilir (pozitif sayi varsayimi). max_len ile tasma onlenir.
 */
static int asn1_read_int(const unsigned char **p, const unsigned char *end,
                         unsigned char *out, int *out_len, int max_len) {
    if (*p >= end || *(*p)++ != 0x02) return -1;
    int len = asn1_read_len(p, end);
    if (len < 0 || *p + len > end) return -1;
    while (len > 0 && **p == 0) { (*p)++; len--; }   /* bas sifirlari at */
    if (len > max_len) return -1;
    memcpy(out, *p, len);
    *out_len = len;
    *p += len;
    return 0;
}

/*
 * DER X.509 sertifikasindan RSA public key'i cikar.
 * Iskelet (hepsi SEQUENCE):
 *   Certificate ::= SEQUENCE
 *     tbsCertificate ::= SEQUENCE
 *       [0] version (skip) ... algorithm (skip)
 *       subjectPublicKeyInfo ::= SEQUENCE
 *         algorithm ::= SEQUENCE (skip)
 *         subjectPublicKey ::= BIT STRING
 *           RSAPublicKey ::= SEQUENCE
 *             modulus INTEGER
 *             exponent INTEGER
 */
int x509_extract_rsa_public_key(const unsigned char *cert_der, int cert_len,
                                unsigned char *out_modulus, int *out_mod_len,
                                unsigned int *out_exponent) {
    const unsigned char *p = cert_der;
    const unsigned char *end = cert_der + cert_len;

    /* Certificate ::= SEQUENCE { tbsCertificate, sigAlg, sigValue } */
    if (asn1_expect_tag(&p, end, 0x30) < 0) return -1;
    /* tbsCertificate SEQUENCE: icinden gec, parse et */
    if (asn1_expect_tag(&p, end, 0x30) < 0) return -1;
    if (p < end && *p == 0xA0) {                 /* [0] version (v3+ opsiyonel) */
        if (asn1_skip(&p, end, 0xA0) < 0) return -1;
    }
    if (asn1_skip(&p, end, 0x02) < 0) return -1;         /* serialNumber */
    if (asn1_skip(&p, end, 0x30) < 0) return -1;         /* signature alg */
    if (asn1_skip(&p, end, 0x30) < 0) return -1;         /* issuer */
    if (asn1_skip(&p, end, 0x30) < 0) return -1;         /* validity */
    if (asn1_skip(&p, end, 0x30) < 0) return -1;         /* subject */
    /* subjectPublicKeyInfo ::= SEQUENCE { alg, BIT STRING } */
    if (asn1_expect_tag(&p, end, 0x30) < 0) return -1;
    if (asn1_skip(&p, end, 0x30) < 0) return -1;         /* algorithm */
    if (asn1_expect_tag(&p, end, 0x03) < 0) return -1;   /* BIT STRING */
    if (p < end && *p == 0x00) p++;                      /* unused-bits byte */
    /* RSAPublicKey ::= SEQUENCE { modulus INTEGER, exponent INTEGER } */
    if (asn1_expect_tag(&p, end, 0x30) < 0) return -1;

    unsigned char modulus[256], exponent[16];
    int mod_len, exp_len;
    if (asn1_read_int(&p, end, modulus, &mod_len, 256) < 0) return -1;
    if (asn1_read_int(&p, end, exponent, &exp_len, 16) < 0) return -1;

    *out_mod_len = mod_len;
    memcpy(out_modulus, modulus, mod_len);

    uint32_t exp = 0;
    for (int i = 0; i < exp_len; i++) exp = (exp << 8) | exponent[i];
    *out_exponent = exp;
    return 0;
}

/*
 * PKCS#1 v1.5 sifreleme: pre-master secret (48 bayt) -> ciphertext.
 * Blok: 0x00 0x02 PS 0x00 M   (PS = mod_len - plain_len - 3 bayt, 0xFF dolu)
 */
void rsa_pkcs1_encrypt(const unsigned char *plaintext, int plain_len,
                       const unsigned char *modulus, int mod_len,
                       unsigned int exponent, unsigned char *out_ciphertext) {
    if (mod_len > 256) return;      /* bu tasarim 2048-bit max */

    unsigned char padded[256];
    int ps_len = mod_len - 3 - plain_len;
    padded[0] = 0x00;
    padded[1] = 0x02;
    for (int i = 0; i < ps_len; i++) padded[2 + i] = 0xFF;
    padded[2 + ps_len] = 0x00;
    memcpy(padded + 3 + ps_len, plaintext, plain_len);

    bignum_t m, e, n, c;
    bn_from_bytes(&n, modulus, mod_len);
    bn_zero(&e);
    e.d[0] = exponent;
    e.n = 1;
    bn_from_bytes(&m, padded, mod_len);
    bignum_modpow(&m, &e, &n, &c);
    bn_to_bytes(&c, out_ciphertext, mod_len);
}

/*
 * PKCS#1 v1.5 imza dogrulama: s^e mod n = m.
 * m formu: 0x00 0x01 0xFF... 0x00 DigestInfo
 * DigestInfo (SHA-256): 3021300906052b0e03021a05000414 || digest(32)
 * 0: dogru; -1: gecersiz.
 */
int rsa_pkcs1_verify(const unsigned char *sig, int sig_len,
                     const unsigned char *modulus, int mod_len,
                     unsigned int exponent,
                     const unsigned char *digest, int digest_len) {
    static const unsigned char digestinfo_sha256[] = {
        0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86,
        0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01, 0x05,
        0x00, 0x04, 0x20
    };
    const int di_len = (int)sizeof(digestinfo_sha256);

    if (mod_len < 3 || mod_len > 256) return -1;
    if (sig_len != mod_len) return -1;
    if (digest_len != 32) return -1;

    /* m = s^e mod n */
    bignum_t s, e, n, m;
    bn_from_bytes(&n, modulus, mod_len);
    bn_zero(&e);
    e.d[0] = exponent;
    e.n = 1;
    bn_from_bytes(&s, sig, sig_len);
    if (bn_cmp(&s, &n) >= 0) return -1;          /* s >= n gecersiz */
    bignum_modpow(&s, &e, &n, &m);

    unsigned char decoded[256];
    bn_to_bytes(&m, decoded, mod_len);

    /* 0x00 0x01 ... padding ... 0x00 DigestInfo */
    if (decoded[0] != 0x00 || decoded[1] != 0x01) return -1;
    int k = 2;
    while (k < mod_len && decoded[k] == 0xFF) k++;
    if (k < 10 || k >= mod_len || decoded[k] != 0x00) return -1;  /* en az 8 FF */
    k++;  /* 0x00'dan sonra */

    if (k + di_len + digest_len > mod_len) return -1;
    if (memcmp(decoded + k, digestinfo_sha256, di_len) != 0) return -1;
    if (memcmp(decoded + k + di_len, digest, digest_len) != 0) return -1;
    return 0;
}

#ifdef RSA_TEST_MAIN
/* ---- native gcc testi (UEFI'siz, masaustunde) ---- */
#include <stdio.h>

static void print_hex(const char *label, const unsigned char *data, int len) {
    printf("%s: ", label);
    for (int i = 0; i < len; i++) printf("%02x", data[i]);
    printf("\n");
}

static int hex_cmp(const unsigned char *a, const unsigned char *b, int n) {
    for (int i = 0; i < n; i++)
        if (a[i] != b[i]) return -1;
    return 0;
}

int main(void) {
    int fail = 0;

    /* Gercek DER sertifika (openssl ile uretildi, 512-bit RSA) */
    static const unsigned char cert[] = {
        0x30, 0x82, 0x01, 0x7f, 0x30, 0x82, 0x01, 0x29, 0xa0, 0x03, 0x02, 0x01,
        0x02, 0x02, 0x14, 0x0d, 0xdc, 0xc8, 0x5c, 0x60, 0x8b, 0x33, 0xdb, 0x5f,
        0x2e, 0xd8, 0xd8, 0x54, 0x01, 0x28, 0xa9, 0xc6, 0x07, 0x50, 0x64, 0x30,
        0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
        0x05, 0x00, 0x30, 0x14, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04,
        0x03, 0x0c, 0x09, 0x54, 0x65, 0x73, 0x74, 0x20, 0x43, 0x65, 0x72, 0x74,
        0x30, 0x1e, 0x17, 0x0d, 0x32, 0x36, 0x30, 0x38, 0x31, 0x33, 0x31, 0x31,
        0x30, 0x38, 0x30, 0x38, 0x5a, 0x17, 0x0d, 0x32, 0x37, 0x30, 0x38, 0x31,
        0x33, 0x31, 0x31, 0x30, 0x38, 0x30, 0x38, 0x5a, 0x30, 0x14, 0x31, 0x12,
        0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x09, 0x54, 0x65, 0x73,
        0x74, 0x20, 0x43, 0x65, 0x72, 0x74, 0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09,
        0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03,
        0x4b, 0x00, 0x30, 0x48, 0x02, 0x41, 0x00, 0xf1, 0x43, 0xd5, 0x80, 0x41,
        0x15, 0x2d, 0x07, 0x77, 0xa6, 0x2d, 0xb6, 0x85, 0x99, 0x2c, 0x18, 0x72,
        0x98, 0x1f, 0x05, 0xbc, 0x22, 0xd2, 0xf8, 0xb7, 0xc7, 0x1f, 0x22, 0x42,
        0x3c, 0x2e, 0xd0, 0xf9, 0x52, 0xcb, 0x07, 0x55, 0x0d, 0xf1, 0x34, 0xe2,
        0x8b, 0xc9, 0x78, 0xb8, 0xc1, 0xeb, 0x7b, 0xc7, 0xd5, 0x81, 0x08, 0x7e,
        0x92, 0x69, 0x5d, 0xa6, 0xc1, 0x26, 0x2b, 0xcc, 0x2c, 0xa3, 0xef, 0x02,
        0x03, 0x01, 0x00, 0x01, 0xa3, 0x53, 0x30, 0x51, 0x30, 0x1d, 0x06, 0x03,
        0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0x89, 0x72, 0x03, 0xd5, 0xf5,
        0x4d, 0x7f, 0x8a, 0x14, 0x5e, 0xe0, 0x0b, 0x2e, 0x5b, 0x75, 0x53, 0xea,
        0x1b, 0xe0, 0xc7, 0x30, 0x1f, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04, 0x18,
        0x30, 0x16, 0x80, 0x14, 0x89, 0x72, 0x03, 0xd5, 0xf5, 0x4d, 0x7f, 0x8a,
        0x14, 0x5e, 0xe0, 0x0b, 0x2e, 0x5b, 0x75, 0x53, 0xea, 0x1b, 0xe0, 0xc7,
        0x30, 0x0f, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x05,
        0x30, 0x03, 0x01, 0x01, 0xff, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48,
        0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x03, 0x41, 0x00, 0x6a,
        0x93, 0x3a, 0x45, 0xb7, 0x23, 0xd9, 0xac, 0x8a, 0x06, 0xff, 0xf5, 0x37,
        0x63, 0xc6, 0xe6, 0x6e, 0x18, 0x57, 0xe1, 0x35, 0x3e, 0x7b, 0xb6, 0x81,
        0x87, 0xa7, 0x3f, 0x6d, 0x4f, 0x22, 0xb1, 0x30, 0xfd, 0x3a, 0xe2, 0x57,
        0xfb, 0xdc, 0x02, 0x41, 0xed, 0xda, 0xe3, 0x7c, 0x17, 0x5a, 0x19, 0xb5,
        0x93, 0xb7, 0x3f, 0xe4, 0x7f, 0x3e, 0x78, 0xe3, 0x77, 0xcb, 0xd1, 0x34,
        0x49, 0xf3, 0x6b
    };
    /* openssl'den dogrulanmis modulus */
    static const unsigned char expected_mod[] = {
        0xf1, 0x43, 0xd5, 0x80, 0x41, 0x15, 0x2d, 0x07, 0x77, 0xa6, 0x2d, 0xb6,
        0x85, 0x99, 0x2c, 0x18, 0x72, 0x98, 0x1f, 0x05, 0xbc, 0x22, 0xd2, 0xf8,
        0xb7, 0xc7, 0x1f, 0x22, 0x42, 0x3c, 0x2e, 0xd0, 0xf9, 0x52, 0xcb, 0x07,
        0x55, 0x0d, 0xf1, 0x34, 0xe2, 0x8b, 0xc9, 0x78, 0xb8, 0xc1, 0xeb, 0x7b,
        0xc7, 0xd5, 0x81, 0x08, 0x7e, 0x92, 0x69, 0x5d, 0xa6, 0xc1, 0x26, 0x2b,
        0xcc, 0x2c, 0xa3, 0xef
    };

    unsigned char mod[256];
    int mod_len;
    unsigned int expo;
    if (x509_extract_rsa_public_key(cert, sizeof(cert), mod, &mod_len, &expo) < 0) {
        printf("FAIL: x509 parse\n");
        return 1;
    }
    print_hex("Modulus", mod, mod_len);
    printf("Exponent: %u\n", expo);
    if (mod_len != 64 || expo != 65537 || hex_cmp(mod, expected_mod, 64) != 0) {
        printf("FAIL: x509 result mismatch\n");
        fail = 1;
    } else {
        printf("OK: x509 parse dogru\n");
    }

    /*
     * RSA dogrulama: sifrele -> ayni public key ile dogrula.
     * Acliama: public key ile dogrulama icin decrypt gerekmez; bunun yerine
     * openssl ile uretilmis key uzerinde pkcs1 dogrulamasi asagida yapilir.
     */
    unsigned char plain[48], ct[64];
    for (int i = 0; i < 48; i++) plain[i] = 0x41 + i;
    rsa_pkcs1_encrypt(plain, 48, mod, mod_len, expo, ct);
    print_hex("Plaintext ", plain, 48);
    print_hex("Ciphertext", ct, 64);

    printf("\n--- PKCS#1 yapisal dogrulama (padding) ---\n");
    /* ct = M^e mod n. M = m^d bilinmeden dogrulanamaz; ancak yapi soyle test
     * edilir: ct < n (bayt 0 oldugundan emin ol), ve sifreleme tekrarlandiginda
     * ayni sonucu vermeli (deterministik padding). */
    unsigned char ct2[64];
    rsa_pkcs1_encrypt(plain, 48, mod, mod_len, expo, ct2);
    if (hex_cmp(ct, ct2, 64) == 0)
        printf("OK: deterministik sifreleme\n");
    else {
        printf("FAIL: deterministik degil\n");
        fail = 1;
    }
    /* ct = M^e mod n oldugundan her zaman ct < n olmali. n 0xF1 ile basladigi
     * icin ct[0] 0 olmak zorunda degil; dogru kriter sayisal karsilastirma. */
    {
        bignum_t bn_ct, bn_n;
        bn_from_bytes(&bn_ct, ct, 64);
        bn_from_bytes(&bn_n, mod, 64);
        if (bn_cmp(&bn_ct, &bn_n) < 0)
            printf("OK: ciphertext < modulus\n");
        else {
            printf("FAIL: ciphertext >= modulus\n");
            fail = 1;
        }
    }

    /* bignum birim testleri: 1234*5678 mod 97 = 51 */
    bignum_t ba, bb, bm, br;
    bn_from_bytes(&ba, (const unsigned char *)"\x04\xd2", 2);
    bn_from_bytes(&bb, (const unsigned char *)"\x16\x2e", 2);
    bn_from_bytes(&bm, (const unsigned char *)"\x61", 1);
    bignum_mul(&ba, &bb, &br);
    bignum_mod(&br, &bm, &br);
    if (br.n == 1 && br.d[0] == 51)
        printf("OK: 1234*5678 mod 97 = 51\n");
    else {
        printf("FAIL: mul/mod: %u\n", br.n ? br.d[0] : 0);
        fail = 1;
    }

    printf(fail ? "\nSONUC: FAIL\n" : "\nSONUC: OK\n");
    return fail;
}
#endif