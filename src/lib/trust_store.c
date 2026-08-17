/*
 * CofeuOS TLS - Trust Store (guvenilir root CA listesi)
 *
 * /config/tls_trust.bin format:
 *   uint32_t magic (0x54525553 = "TRUST")
 *   uint32_t version (1)
 *   uint32_t count
 *   for each CA:
 *     uint32_t cert_len
 *     uint8_t cert_der[cert_len]
 *
 * malloc yok; sabit buffer'lar.
 */
#include <stdint.h>
#include <stddef.h>
#include "../include/string.h"
#include "../include/bignum.h"
#include "../include/x509.h"

#define TRUST_MAGIC 0x54525553  /* "TRUST" */
#define TRUST_VERSION 1

struct trust_store {
    uint8_t *data;
    int size;
    int capacity;
};

static int read_u32(const uint8_t **p, const uint8_t *end, uint32_t *out) {
    if (*p + 4 > end) return -1;
    *out = ((uint32_t)(*p)[0] << 24) | ((uint32_t)(*p)[1] << 16) |
           ((uint32_t)(*p)[2] << 8) | (*p)[3];
    *p += 4;
    return 0;
}

static int trust_store_parse(const uint8_t *data, int size,
                             const unsigned char ***out_certs,
                             int **out_lens, int *out_count) {
    if (size < 12) return -1;
    const uint8_t *p = data;
    const uint8_t *end = data + size;

    uint32_t magic, version, count;
    if (read_u32(&p, end, &magic) < 0) return -1;
    if (magic != TRUST_MAGIC) return -1;
    if (read_u32(&p, end, &version) < 0) return -1;
    if (version != TRUST_VERSION) return -1;
    if (read_u32(&p, end, &count) < 0) return -1;
    if (count > 256) return -1;

    const unsigned char **certs = (const unsigned char **)data; /* temp */
    int *lens = (int *)(data + size / 2);

    for (uint32_t i = 0; i < count; i++) {
        if (p + 4 > end) return -1;
        uint32_t cert_len;
        if (read_u32(&p, end, &cert_len) < 0) return -1;
        if (cert_len == 0 || cert_len > 16384) return -1;
        if (p + cert_len > end) return -1;

        certs[i] = p;
        lens[i] = (int)cert_len;
        p += cert_len;
    }

    *out_certs = certs;
    *out_lens = lens;
    *out_count = (int)count;
    return 0;
}

/* Global trust store blob (network.c tarafında yüklenir) */
static const uint8_t *g_trust_blob = NULL;
static int g_trust_blob_size = 0;

int trust_store_load_blob(const uint8_t *data, int size) {
    if (!data || size <= 0) return -1;
    g_trust_blob = data;
    g_trust_blob_size = size;
    return 0;
}

int trust_store_parse_blob(const uint8_t *data, int size,
                           const unsigned char ***out_certs,
                           int **out_lens, int *out_count) {
    return trust_store_parse(data, size, out_certs, out_lens, out_count);
}

int trust_store_get_certs(const unsigned char ***out_certs,
                          int **out_lens, int *out_count) {
    if (!g_trust_blob || g_trust_blob_size == 0) return -1;
    return trust_store_parse(g_trust_blob, g_trust_blob_size, out_certs, out_lens, out_count);
}

int trust_store_load(struct trust_store *ts, const char *path) {
    (void)ts; (void)path;
    /* UEFI dosya okuma network.c uzerinden yapilir.
       Bu fonksiyon placeholder - network.c icinde dosya okunup
       trust_store_load_blob cagrilacak. */
    return 0;
}

/* ---- test yardimcisi: trust blob olustur ---- */
#ifdef TRUST_STORE_TEST_MAIN
#include <stdio.h>

static void write_u32(uint8_t **p, uint32_t v) {
    (*p)[0] = (uint8_t)(v >> 24);
    (*p)[1] = (uint8_t)(v >> 16);
    (*p)[2] = (uint8_t)(v >> 8);
    (*p)[3] = (uint8_t)v;
    *p += 4;
}

static int make_test_blob(uint8_t *out, int cap) {
    uint8_t *p = out;
    if (cap < 12) return -1;
    write_u32(&p, TRUST_MAGIC);
    write_u32(&p, TRUST_VERSION);
    write_u32(&p, 0); /* count = 0 */
    return (int)(p - out);
}

int main(void) {
    uint8_t blob[4096];
    int len = make_test_blob(blob, sizeof blob);
    if (len < 0) { printf("FAIL: make_test_blob\n"); return 1; }
    printf("Test blob: %d bytes\n", len);

    const unsigned char **certs;
    int *lens;
    int count;
    if (trust_store_parse_blob(blob, len, &certs, &lens, &count) < 0) {
        printf("FAIL: parse empty\n"); return 1;
    }
    if (count != 0) { printf("FAIL: count != 0\n"); return 1; }
    printf("OK: empty trust store\n");

    /* test with 1 dummy cert */
    uint8_t *p = blob;
    write_u32(&p, TRUST_MAGIC);
    write_u32(&p, TRUST_VERSION);
    write_u32(&p, 1);
    write_u32(&p, 4);
    p[0] = 0x30; p[1] = 0x02; p[2] = 0x01; p[3] = 0x01; /* dummy DER */
    p += 4;
    len = (int)(p - blob);

    if (trust_store_parse_blob(blob, len, &certs, &lens, &count) < 0) {
        printf("FAIL: parse 1 cert\n"); return 1;
    }
    if (count != 1 || lens[0] != 4 || memcmp(certs[0], "\x30\x02\x01\x01", 4) != 0) {
        printf("FAIL: cert data mismatch\n"); return 1;
    }
    printf("OK: trust store parse 1 cert\n");

    printf("\nSONUC: OK\n");
    return 0;
}
#endif