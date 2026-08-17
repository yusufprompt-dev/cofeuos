/*
 * CofeuOS TLS - X.509 sertifika zinciri dogrulama
 *
 * malloc yok; sabit buffer'lar. DER parse + RSA-SHA256 imza dogrulama.
 * Root CA'lar /config/tls_trust.bin (binary DER listesi) icinde tutulur.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../include/string.h"
#include "../include/bignum.h"
#include "../include/rsa.h"
#include "../include/x509.h"
#include "../include/time.h"

/* ---- ASN.1 / DER yardimcilari ---- */

static int asn1_read_len(const unsigned char **p, const unsigned char *end) {
    if (*p >= end) return -1;
    int len = *(*p)++;
    if (len & 0x80) {
        int n = len & 0x7F;
        if (n > 4 || *p + n > end) return -1;
        len = 0;
        for (int i = 0; i < n; i++) {
            len = (len << 8) | *(*p)++;
        }
    }
    if (*p + len > end) return -1;
    return len;
}

static int asn1_skip(const unsigned char **p, const unsigned char *end, int tag) {
    if (*p >= end || **p != tag) return -1;
    (*p)++;
    int len = asn1_read_len(p, end);
    if (len < 0) return -1;
    *p += len;
    return 0;
}

static int asn1_get_sequence(const unsigned char **p, const unsigned char *end,
                             const unsigned char **out_start, int *out_len) {
    if (*p >= end || **p != 0x30) return -1;
    (*p)++;
    int len = asn1_read_len(p, end);
    if (len < 0) return -1;
    *out_start = *p;
    *out_len = len;
    *p += len;
    return 0;
}

static int asn1_get_oid(const unsigned char **p, const unsigned char *end,
                        const unsigned char **out_oid, int *out_oid_len) {
    if (*p >= end || **p != 0x06) return -1;
    (*p)++;
    int len = asn1_read_len(p, end);
    if (len < 0) return -1;
    *out_oid = *p;
    *out_oid_len = len;
    *p += len;
    return 0;
}

static int asn1_get_bitstring(const unsigned char **p, const unsigned char *end,
                              const unsigned char **out_bits, int *out_bits_len) {
    if (*p >= end || **p != 0x03) return -1;
    (*p)++;
    int len = asn1_read_len(p, end);
    if (len < 1) return -1;
    int unused = *(*p)++;
    if (unused != 0) return -1;
    *out_bits = *p;
    *out_bits_len = len - 1;
    *p += len - 1;
    return 0;
}

static int asn1_get_integer(const unsigned char **p, const unsigned char *end,
                            const unsigned char **out_int, int *out_int_len) {
    if (*p >= end || **p != 0x02) return -1;
    (*p)++;
    int len = asn1_read_len(p, end);
    if (len < 0) return -1;
    while (len > 1 && *p[0] == 0 && (*p[1] & 0x80) == 0) { (*p)++; len--; }
    *out_int = *p;
    *out_int_len = len;
    *p += len;
    return 0;
}

static int asn1_get_null(const unsigned char **p, const unsigned char *end) {
    if (*p + 2 > end) return -1;
    if ((*p)[0] != 0x05 || (*p)[1] != 0x00) return -1;
    *p += 2;
    return 0;
}

/* ---- OID eslesmeleri ---- */
static const unsigned char OID_SHA256_WITH_RSA[] = { 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b };
static const unsigned char OID_SHA256[] = { 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01 };
static const unsigned char OID_SUBJECT_ALT_NAME[] = { 0x55, 0x1d, 0x11 };
static const unsigned char OID_KEY_USAGE[] = { 0x55, 0x1d, 0x0f };
static const unsigned char OID_EXT_KEY_USAGE[] = { 0x55, 0x1d, 0x25 };

static int oid_equal(const unsigned char *a, int alen, const unsigned char *b, int blen) {
    return alen == blen && memcmp(a, b, alen) == 0;
}

/* ---- SHA-256 (tls_crypto.h ile ayni) ---- */
extern void sha256_hash(const uint8_t *data, size_t len, uint8_t out[32]);

/* ---- Time (for cert validity) ---- */
extern int time_get_unix(int64_t *out_ts);;

static int parse_time(const unsigned char **p, const unsigned char *end,
                      const unsigned char **out_time, int *out_len) {
    if (*p >= end) return -1;
    uint8_t tag = **p;
    if (tag != 0x17 && tag != 0x18) return -1;
    (*p)++;
    int len = asn1_read_len(p, end);
    if (len < 0) return -1;
    *out_time = *p;
    *out_len = len;
    *p += len;
    return 0;
}

static int parse_tbs_certificate(const unsigned char *cert, int cert_len,
                                 struct x509_cert *c) {
    const unsigned char *p = cert;
    const unsigned char *end = cert + cert_len;

    const unsigned char *tbs_start;
    int tbs_len;
    if (asn1_get_sequence(&p, end, &tbs_start, &tbs_len) < 0) return -1;
    c->tbs_start = tbs_start;
    c->tbs_len = tbs_len;

    p = tbs_start;
    const unsigned char *tbs_end = tbs_start + tbs_len;

    if (p < tbs_end && *p == 0xA0) {
        p++; int len = asn1_read_len(&p, tbs_end);
        if (len < 0) return -1;
        p += len;
    }

    if (asn1_skip(&p, tbs_end, 0x02) < 0) return -1;
    if (asn1_skip(&p, tbs_end, 0x30) < 0) return -1;

    const unsigned char *issuer_start;
    int issuer_len;
    if (asn1_get_sequence(&p, tbs_end, &issuer_start, &issuer_len) < 0) return -1;
    c->issuer = issuer_start;
    c->issuer_len = issuer_len;

    const unsigned char *validity_start;
    int validity_len;
    if (asn1_get_sequence(&p, tbs_end, &validity_start, &validity_len) < 0) return -1;
    const unsigned char *vp = validity_start;
    const unsigned char *ve = validity_start + validity_len;
    if (parse_time(&vp, ve, &c->not_before, &c->not_before_len) < 0) return -1;
    if (parse_time(&vp, ve, &c->not_after, &c->not_after_len) < 0) return -1;

    const unsigned char *subject_start;
    int subject_len;
    if (asn1_get_sequence(&p, tbs_end, &subject_start, &subject_len) < 0) return -1;
    c->subject = subject_start;
    c->subject_len = subject_len;

    const unsigned char *spki_start;
    int spki_len;
    if (asn1_get_sequence(&p, tbs_end, &spki_start, &spki_len) < 0) return -1;
    c->spki_start = spki_start;
    c->spki_len = spki_len;

    if (p < tbs_end && *p == 0xA3) {
        p++;
        int ext_len = asn1_read_len(&p, tbs_end);
        if (ext_len < 0) return -1;
        const unsigned char *ext_start = p;
        const unsigned char *ext_end = p + ext_len;

        const unsigned char *exts_start;
        int exts_len;
        if (asn1_get_sequence(&ext_start, ext_end, &exts_start, &exts_len) < 0) return -1;
        const unsigned char *ep = exts_start;
        const unsigned char *ee = exts_start + exts_len;

        while (ep < ee) {
            const unsigned char *ext_oid;
            int ext_oid_len;
            if (asn1_get_oid(&ep, ee, &ext_oid, &ext_oid_len) < 0) break;
            int critical = 0;
            if (ep < ee && *ep == 0x01) { critical = 1; ep++; }
            if (asn1_skip(&ep, ee, 0x04) < 0) break;

            if (oid_equal(ext_oid, ext_oid_len, (unsigned char[]){0x55,0x1D,0x13}, 3)) {
                const unsigned char *val = ep - 2;
                if (asn1_skip(&ep, ee, 0x30) < 0) break;
                if (ep < ee && *ep == 0x01) {
                    ep++;
                    int blen = asn1_read_len(&ep, ee);
                    if (blen == 1 && *ep == 0xFF) c->is_ca = 1;
                }
                c->has_basic_constraints = 1;
            }
            else if (oid_equal(ext_oid, ext_oid_len, OID_SUBJECT_ALT_NAME, 3)) {
                const unsigned char *val = ep - 2;
                if (asn1_skip(&ep, ee, 0x30) < 0) break;
                c->san_start = val;
                c->san_len = ep - val;
                c->has_san = 1;
            }
            else if (oid_equal(ext_oid, ext_oid_len, OID_KEY_USAGE, 3)) {
                const unsigned char *val = ep - 2;
                if (asn1_skip(&ep, ee, 0x03) < 0) break;
                if (ep < ee) {
                    c->key_usage = *ep;
                    c->has_key_usage = 1;
                }
            }
            else if (oid_equal(ext_oid, ext_oid_len, OID_EXT_KEY_USAGE, 3)) {
                const unsigned char *val = ep - 2;
                if (asn1_skip(&ep, ee, 0x30) < 0) break;
                c->eku_oids = val;
                c->eku_oids_len = ep - val;
                c->has_ext_key_usage = 1;
            }
        }
    }

    return 0;
}

static int parse_utc_time(const unsigned char *s, int len, int64_t *out_ts) {
    if (len != 13 || s[12] != 'Z') return -1;
    int year = (s[0]-'0')*10 + (s[1]-'0');
    int month = (s[2]-'0')*10 + (s[3]-'0');
    int day = (s[4]-'0')*10 + (s[5]-'0');
    int hour = (s[6]-'0')*10 + (s[7]-'0');
    int min = (s[8]-'0')*10 + (s[9]-'0');
    int sec = (s[10]-'0')*10 + (s[11]-'0');
    if (year < 0 || year > 99 || month < 1 || month > 12 || day < 1 || day > 31 ||
        hour > 23 || min > 59 || sec > 59) return -1;
    int full_year = (year < 50) ? 2000 + year : 1900 + year;

    static const uint16_t days_before_month[12] = {
        0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334
    };
    int y = full_year - 1970;
    int leap_days = (y + 1) / 4 - (y + 69) / 100 + (y + 369) / 400;
    int days = y * 365 + leap_days + days_before_month[month - 1] + (day - 1);
    if (month > 2 && ((full_year % 4 == 0 && full_year % 100 != 0) || full_year % 400 == 0))
        days++;
    int64_t ts = ((int64_t)days * 86400) + hour * 3600 + min * 60 + sec;
    *out_ts = ts;
    return 0;
}

static int cert_is_valid_time(const struct x509_cert *c) {
    if (!c->not_before || !c->not_after) return 0;

    int64_t now;
    if (time_get_unix(&now) < 0) return 1;

    int64_t not_before, not_after;
    if (c->not_before_len == 13) {
        if (parse_utc_time(c->not_before, c->not_before_len, &not_before) < 0) return 0;
    } else return 0;
    if (c->not_after_len == 13) {
        if (parse_utc_time(c->not_after, c->not_after_len, &not_after) < 0) return 0;
    } else return 0;

    return (now >= not_before && now <= not_after) ? 1 : 0;
}

static int parse_certificate(const unsigned char *cert, int cert_len,
                             struct x509_cert *c) {
    memset(c, 0, sizeof(*c));
    const unsigned char *p = cert;
    const unsigned char *end = cert + cert_len;

    const unsigned char *cert_content;
    int cert_content_len;
    if (asn1_get_sequence(&p, end, &cert_content, &cert_content_len) < 0) return -1;

    p = cert_content;
    const unsigned char *content_end = cert_content + cert_content_len;

    if (parse_tbs_certificate(p, (int)(content_end - p), c) < 0) return -1;

    const unsigned char *sig_alg_oid;
    int sig_alg_oid_len;
    if (asn1_get_sequence(&p, content_end, &sig_alg_oid, &sig_alg_oid_len) < 0) return -1;
    const unsigned char *soid = sig_alg_oid;
    if (asn1_get_oid(&soid, sig_alg_oid + sig_alg_oid_len, &c->sig_alg_oid, &c->sig_alg_oid_len) < 0) return -1;
    if (asn1_get_null(&soid, sig_alg_oid + sig_alg_oid_len) < 0) return -1;
    if (!oid_equal(c->sig_alg_oid, c->sig_alg_oid_len, OID_SHA256_WITH_RSA, sizeof(OID_SHA256_WITH_RSA)))
        return -1;

    if (asn1_get_bitstring(&p, content_end, &c->signature, &c->sig_len) < 0) return -1;

    const unsigned char *sp = c->spki_start;
    const unsigned char *sp_end = c->spki_start + c->spki_len;
    const unsigned char *alg_oid;
    int alg_oid_len;
    if (asn1_get_sequence(&sp, sp_end, &alg_oid, &alg_oid_len) < 0) return -1;
    const unsigned char *aoid = alg_oid;
    if (asn1_get_oid(&aoid, alg_oid + alg_oid_len, &alg_oid, &alg_oid_len) < 0) return -1;
    if (!oid_equal(alg_oid, alg_oid_len, (unsigned char[]){0x2A,0x86,0x48,0x86,0xF7,0x0D,0x01,0x01,0x01}, 9))
        return -1;
    if (asn1_get_null(&aoid, alg_oid + alg_oid_len) < 0) return -1;

    const unsigned char *pubkey_bits;
    int pubkey_bits_len;
    if (asn1_get_bitstring(&sp, sp_end, &pubkey_bits, &pubkey_bits_len) < 0) return -1;

    const unsigned char *pk_start;
    int pk_len;
    if (asn1_get_sequence(&pubkey_bits, pubkey_bits + pubkey_bits_len, &pk_start, &pk_len) < 0) return -1;
    const unsigned char *pkp = pk_start;
    const unsigned char *pk_end = pk_start + pk_len;
    const unsigned char *mod; int mod_len;
    if (asn1_get_integer(&pkp, pk_end, &mod, &mod_len) < 0) return -1;
    const unsigned char *exp_bytes; int exp_len;
    if (asn1_get_integer(&pkp, pk_end, &exp_bytes, &exp_len) < 0) return -1;

    c->modulus = mod;
    c->mod_len = mod_len;
    c->exponent = 0;
    for (int i = 0; i < exp_len; i++) {
        c->exponent = (c->exponent << 8) | exp_bytes[i];
    }

    return 0;
}

/* ---- sertifika zinciri dogrulama ---- */

static int x509_name_equal(const unsigned char *a, int alen,
                           const unsigned char *b, int blen) {
    return alen == blen && memcmp(a, b, alen) == 0;
}

static int verify_signature(const struct x509_cert *cert,
                            const struct x509_cert *issuer) {
    uint8_t digest[32];
    sha256_hash(cert->tbs_start, cert->tbs_len, digest);

    return rsa_pkcs1_verify(cert->signature, cert->sig_len,
                            issuer->modulus, issuer->mod_len,
                            issuer->exponent,
                            digest, 32);
}

static int san_match_hostname(const unsigned char *san, int san_len,
                              const char *hostname) {
    if (!san || san_len <= 0 || !hostname) return 0;

    const unsigned char *p = san;
    const unsigned char *end = san + san_len;

    while (p < end) {
        if (p >= end) break;
        uint8_t tag = *p++;
        int len = asn1_read_len(&p, san + san_len);
        if (len < 0) return 0;

        if (tag == 0x82) {
            if (len == (int)strlen(hostname) &&
                memcmp(p, hostname, len) == 0) {
                return 1;
            }
        }
        p += len;
    }
    return 0;
}

int x509_verify_hostname(const struct x509_cert *cert, const char *hostname) {
    if (!cert || !cert->san_start || !cert->has_san) return 0;
    return san_match_hostname(cert->san_start, cert->san_len, hostname);
}

int x509_check_key_usage(const struct x509_cert *cert, int usage_mask) {
    if (!cert || !cert->has_key_usage) return 0;
    return (cert->key_usage & usage_mask) != 0;
}

int x509_has_ext_key_usage(const struct x509_cert *cert,
                           const unsigned char *oid, int oid_len) {
    if (!cert || !cert->has_ext_key_usage || !cert->eku_oids) return 0;

    const unsigned char *p = cert->eku_oids;
    const unsigned char *end = cert->eku_oids + cert->eku_oids_len;

    while (p < end) {
        const unsigned char *found_oid;
        int found_oid_len;
        if (asn1_get_oid(&p, end, &found_oid, &found_oid_len) < 0) break;
        if (oid_equal(found_oid, found_oid_len, oid, oid_len)) return 1;
    }
    return 0;
}

int x509_verify_chain(const unsigned char **certs, const int *cert_lens, int n,
                      const unsigned char **trusted_cas, const int *trusted_ca_lens, int n_ca) {
    if (n == 0) return -1;

    struct x509_cert chain[16];
    if (n > 16) return -1;

    for (int i = 0; i < n; i++) {
        if (parse_certificate(certs[i], cert_lens[i], &chain[i]) < 0)
            return -1;
    }

    for (int i = 0; i < n - 1; i++) {
        if (!verify_signature(&chain[i], &chain[i + 1]))
            return -1;
    }

    int verified = 0;
    for (int j = 0; j < n_ca; j++) {
        struct x509_cert ca;
        if (parse_certificate(trusted_cas[j], trusted_ca_lens[j], &ca) < 0)
            continue;
        if (x509_name_equal(chain[n - 1].subject, chain[n - 1].subject_len,
                            ca.subject, ca.subject_len) &&
            verify_signature(&chain[n - 1], &ca)) {
            verified = 1;
            break;
        }
    }

    if (!verified) return -1;

    for (int i = 0; i < n; i++) {
        if (!cert_is_valid_time(&chain[i])) return -1;
    }

    return 0;
}

/* ---- trust store yardimcilari (placeholder) ---- */

struct trust_store {
    unsigned char *data;
    int size;
    int capacity;
};

static int trust_store_load(struct trust_store *ts, const char *path) {
    (void)ts; (void)path;
    return 0;
}

static int trust_store_get_certs(struct trust_store *ts,
                                 const unsigned char ***out_certs,
                                 int **out_lens, int *out_count) {
    (void)ts; (void)out_certs; (void)out_lens; (void)out_count;
    return 0;
}

/* ---- yardimci: RSA-PKCS1 v1.5 SHA-256 imza dogrulama ---- */
/* Bu fonksiyon rsa.c'de tanimli: rsa_pkcs1_verify */

#ifdef X509_TEST_MAIN
#include <stdio.h>

int main(void) {
    printf("X509 test placeholder\n");
    return 0;
}
#endif