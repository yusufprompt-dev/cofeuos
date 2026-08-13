/*
 * ============================================================================
 * TLS_PRF.C - TLS 1.2 PRF (SHA-256 + HMAC-SHA256 + P_SHA256)
 *
 * malloc yok, her sey sabit boyutlu buffer + pointer aritmetigi.
 * SHA-256, CofeuOS'un mevcut sha256.c'sinden kullanilir (tek implementasyon).
 * Kaynaklar: RFC 2104/4231 (HMAC), RFC 5246 (TLS 1.2 PRF).
 *
 * Test icin: gcc -DTLS_PRF_TEST_MAIN -O2 -o prf_test tls_prf.c sha256.c
 * ============================================================================
 */
#include <stdint.h>
#include "../include/string.h"
#include "../include/tls_crypto.h"

#define SHA256_BLOCK 64
#define SHA256_OUT   32
#define HMAC_MSG_MAX 512              /* HMAC mesajlari icin ust sinir (TLS) */

/* ---- HMAC-SHA256 (RFC 2104/4231) ---- */

void hmac_sha256(const uint8_t *key, size_t key_len,
                 const uint8_t *msg, size_t msg_len,
                 uint8_t out[32]) {
    uint8_t kp[SHA256_BLOCK];          /* 0 ile doldurulmus anahtar blogu */
    uint8_t inner[SHA256_BLOCK + HMAC_MSG_MAX];
    uint8_t ihash[SHA256_OUT];
    size_t i;

    if (key_len > SHA256_BLOCK) {
        sha256_hash(key, key_len, kp); /* uzun anahtar: once hash */
        for (i = SHA256_OUT; i < SHA256_BLOCK; i++) kp[i] = 0x00;
    } else {
        memcpy(kp, key, key_len);
        for (i = key_len; i < SHA256_BLOCK; i++) kp[i] = 0x00;
    }

    for (i = 0; i < SHA256_BLOCK; i++) inner[i] = kp[i] ^ 0x36;
    memcpy(inner + SHA256_BLOCK, msg, msg_len);
    sha256_hash(inner, SHA256_BLOCK + msg_len, ihash);

    for (i = 0; i < SHA256_BLOCK; i++) inner[i] = kp[i] ^ 0x5c;
    memcpy(inner + SHA256_BLOCK, ihash, SHA256_OUT);
    sha256_hash(inner, SHA256_BLOCK + SHA256_OUT, out);
}

/* ---- Incremental HMAC (buyuk mesajlar icin, or. TLS record MAC) ---- */

void hmac_sha256_begin(hmac_sha256_ctx *c, const uint8_t *key, size_t key_len) {
    size_t i;
    if (key_len > SHA256_BLOCK) {
        uint8_t hk[SHA256_OUT];
        sha256_hash(key, key_len, hk);
        memcpy(c->kp, hk, SHA256_OUT);
        for (i = SHA256_OUT; i < SHA256_BLOCK; i++) c->kp[i] = 0x00;
    } else {
        memcpy(c->kp, key, key_len);
        for (i = key_len; i < SHA256_BLOCK; i++) c->kp[i] = 0x00;
    }
    uint8_t ipad[SHA256_BLOCK];
    for (i = 0; i < SHA256_BLOCK; i++) ipad[i] = c->kp[i] ^ 0x36;
    sha256_init(&c->inner);
    sha256_update(&c->inner, ipad, SHA256_BLOCK);
}

void hmac_sha256_update(hmac_sha256_ctx *c, const uint8_t *data, size_t len) {
    sha256_update(&c->inner, data, len);
}

void hmac_sha256_final(hmac_sha256_ctx *c, uint8_t out[32]) {
    uint8_t ihash[SHA256_OUT];
    sha256_final(&c->inner, ihash);
    uint8_t opad[SHA256_BLOCK];
    for (size_t i = 0; i < SHA256_BLOCK; i++) opad[i] = c->kp[i] ^ 0x5c;
    sha256_context outer;
    sha256_init(&outer);
    sha256_update(&outer, opad, SHA256_BLOCK);
    sha256_update(&outer, ihash, SHA256_OUT);
    sha256_final(&outer, out);
}

/* ---- TLS 1.2 PRF: P_SHA256(secret, label || seed) ---- */

#define PRF_SEED_BUF 128               /* label + seed icin sabit buffer */

int tls_prf_sha256(const uint8_t *secret, size_t secret_len,
                   const char *label,
                   const uint8_t *seed, size_t seed_len,
                   uint8_t *out, size_t out_len) {
    uint8_t seedbuf[PRF_SEED_BUF];
    size_t label_len = strlen(label);
    size_t total = label_len + seed_len;
    if (total > PRF_SEED_BUF) return -1;

    memcpy(seedbuf, label, label_len);
    memcpy(seedbuf + label_len, seed, seed_len);

    uint8_t a[SHA256_OUT];             /* A(1) = HMAC(secret, seed) */
    hmac_sha256(secret, secret_len, seedbuf, total, a);

    uint8_t chunk[SHA256_OUT];
    uint8_t msg[SHA256_OUT + PRF_SEED_BUF];
    size_t pos = 0;
    while (pos < out_len) {
        memcpy(msg, a, SHA256_OUT);
        memcpy(msg + SHA256_OUT, seedbuf, total);
        hmac_sha256(secret, secret_len, msg, SHA256_OUT + total, chunk);
        size_t take = SHA256_OUT;
        if (take > out_len - pos) take = out_len - pos;
        memcpy(out + pos, chunk, take);
        pos += take;
        hmac_sha256(secret, secret_len, a, SHA256_OUT, a);  /* A(n+1) */
    }
    return 0;
}

#ifdef TLS_PRF_TEST_MAIN
/* ---- native gcc testi ---- */
#include <stdio.h>

static int hex_eq(const uint8_t *a, const char *hex, size_t n) {
    static const char hx[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        if (hx[(a[i] >> 4) & 0xf] != hex[2*i]) return -1;
        if (hx[a[i] & 0xf]        != hex[2*i+1]) return -1;
    }
    return 0;
}

int main(void) {
    int fail = 0;
    uint8_t out[200];
    const char *sha256_hex;

    /* ---- HMAC-SHA256 (RFC 4231 TC1-3 ve uzun anahtar) ---- */
    uint8_t key20[20], data50[50];
    memset(key20, 0x0b, 20);
    hmac_sha256(key20, 20, (const uint8_t *)"Hi There", 8, out);
    sha256_hex = "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7";
    if (hex_eq(out, sha256_hex, 32) == 0) printf("OK: HMAC TC1\n");
    else { printf("FAIL: HMAC TC1\n"); fail = 1; }

    hmac_sha256((const uint8_t *)"Jefe", 4, (const uint8_t *)"what do ya want for nothing?", 28, out);
    sha256_hex = "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843";
    if (hex_eq(out, sha256_hex, 32) == 0) printf("OK: HMAC TC2\n");
    else { printf("FAIL: HMAC TC2\n"); fail = 1; }

    memset(key20, 0xaa, 20);
    memset(data50, 0xdd, 50);
    hmac_sha256(key20, 20, data50, 50, out);
    sha256_hex = "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe";
    if (hex_eq(out, sha256_hex, 32) == 0) printf("OK: HMAC TC3\n");
    else { printf("FAIL: HMAC TC3\n"); fail = 1; }

    uint8_t key131[131];
    memset(key131, 0xaa, 131);
    hmac_sha256(key131, 131, (const uint8_t *)"Test Using Larger Than Block-Size Key - Hash Key First", 54, out);
    sha256_hex = "60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54";
    if (hex_eq(out, sha256_hex, 32) == 0) printf("OK: HMAC uzun anahtar\n");
    else { printf("FAIL: HMAC uzun anahtar\n"); fail = 1; }

    /* ---- TLS 1.2 PRF (Python hashlib/hmac referansi) ---- */
    uint8_t crand[32], srand[32], seed[64];
    for (int i = 0; i < 32; i++) { crand[i] = (uint8_t)i; srand[i] = (uint8_t)(i + 32); }
    memcpy(seed, crand, 32); memcpy(seed + 32, srand, 32);

    uint8_t secret48[48];
    memset(secret48, 0, 48);
    tls_prf_sha256(secret48, 48, "master secret", seed, 64, out, 48);
    sha256_hex = "c3d44e2c9ae333955f6f04c1c4e975d78de11fcb6a8882f26edaf84590024a85f953af3c98df4c135e927fb2354f6f9c";
    if (hex_eq(out, sha256_hex, 48) == 0) printf("OK: PRF master secret (48)\n");
    else { printf("FAIL: PRF master secret (48)\n"); fail = 1; }

    tls_prf_sha256((const uint8_t *)"abc", 3, "master secret", seed, 64, out, 48);
    sha256_hex = "b73d8a019203eef38807d08fddacb6773070fe0fbf69af7f0e5830d7ebe4777e952911773129df9fedd3a7c580106e2a";
    if (hex_eq(out, sha256_hex, 48) == 0) printf("OK: PRF master secret 'abc'\n");
    else { printf("FAIL: PRF master secret 'abc'\n"); fail = 1; }

    uint8_t seedrev[64];
    memcpy(seedrev, srand, 32); memcpy(seedrev + 32, crand, 32);
    uint8_t key32[32];
    memset(key32, 'K', 32);
    tls_prf_sha256(key32, 32, "key expansion", seedrev, 64, out, 104);
    sha256_hex = "aa94802f6a8fedaeb38f8a74d5bc059e7f3bee306712a85fd4dbd244e102c15f3c7ee55a09fbf1b73b4b6bf6e42349475208edd05eead491aec11b6cb5e192920b27413e48d805388f703f7368a3ea72a7bb6e57a471c1bf1e4920f6a3fbd6722b9926972bdd1251";
    if (hex_eq(out, sha256_hex, 104) == 0) printf("OK: PRF key expansion (104)\n");
    else { printf("FAIL: PRF key expansion (104)\n"); fail = 1; }

    uint8_t finseed[36];
    for (int i = 0; i < 36; i++) finseed[i] = 0x22;
    uint8_t secret11[48];
    memset(secret11, 0x11, 48);
    tls_prf_sha256(secret11, 48, "client finished", finseed, 36, out, 12);
    sha256_hex = "37904a32fc2087f54e39db2e";
    if (hex_eq(out, sha256_hex, 12) == 0) printf("OK: PRF client finished (12)\n");
    else { printf("FAIL: PRF client finished (12)\n"); fail = 1; }

    printf(fail ? "\nSONUC: FAIL\n" : "\nSONUC: OK\n");
    return fail;
}
#endif
