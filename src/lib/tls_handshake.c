/*
 * CofeuOS TLS - Bilesen 5: TLS 1.2 Handshake mesajlari (RFC 5246 7.4)
 *
 * Cipher suite: TLS_RSA_WITH_AES_128_CBC_SHA256 (0x003C)
 * Handshake header: type(1) || length(3). Mesajlar record layer ile
 * tasinir; bu modul mesajlarin encode/parse'ini ve kriptografik
 * akisini (premaster -> master secret -> Finished) yonetir.
 *
 * Bilesenler: tls_prf.c (PRF, SHA-256), rsa.c (RSA-PKCS1 v1.5).
 * malloc yok; sabit buffer + pointer aritmetigi.
 */
#include <stdint.h>
#include "../include/string.h"
#include "../include/tls_crypto.h"

#define TLS_MAJOR 3
#define TLS_MINOR 3

#define HS_CLIENT_HELLO          1
#define HS_SERVER_HELLO          2
#define HS_CERTIFICATE           11
#define HS_SERVER_HELLO_DONE     14
#define HS_CLIENT_KEY_EXCHANGE   16
#define HS_FINISHED              20

#define TLS_CIPHER_AES128_CBC_SHA256 0x003c
#define TLS_COMPRESSION_NULL         0x00
#define TLS_VERIFY_DATA_LEN          12
#define TLS_PREMASTER_LEN            48
#define TLS_MASTER_LEN               48
#define TLS_HS_MAX_BODY              16384

extern int  tls_prf_sha256(const uint8_t *secret, size_t secret_len,
                           const char *label,
                           const uint8_t *seed, size_t seed_len,
                           uint8_t *out, size_t out_len);
#define SHA256_OUT 32
extern void rsa_pkcs1_encrypt(const unsigned char *plaintext, int plain_len,
                              const unsigned char *modulus, int mod_len,
                              unsigned int exponent,
                              unsigned char *out_ciphertext);

static int put24(uint8_t *out, size_t out_cap, size_t pos, size_t v) {
    if (pos + 3 > out_cap) return -1;
    out[pos]     = (uint8_t)((v >> 16) & 0xff);
    out[pos + 1] = (uint8_t)((v >> 8) & 0xff);
    out[pos + 2] = (uint8_t)(v & 0xff);
    return 0;
}

static int put16(uint8_t *out, size_t out_cap, size_t pos, size_t v) {
    if (pos + 2 > out_cap) return -1;
    out[pos]     = (uint8_t)((v >> 8) & 0xff);
    out[pos + 1] = (uint8_t)(v & 0xff);
    return 0;
}

/*
 * ClientHello (ilk hello: session_id bos, tek cipher suite 0x003C,
 * tek compression 0x00, signature_algorithms + (varsa) SNI extension).
 * out: tum handshake mesaji (header dahil).
 */
int tls_build_client_hello(const uint8_t random[32],
                           const uint8_t *session_id, size_t session_id_len,
                           const char *host,
                           uint8_t *out, size_t out_cap, size_t *out_len) {
    if (session_id_len > 32) return -1;
    size_t host_len = host ? strlen(host) : 0;
    if (host_len > 255) return -1;
    /* signature_algorithms: SHA256+RSA(0x0401), SHA384+RSA(0x0501),
       SHA512+RSA(0x0601) -> 6 bayt alan */
    size_t body = 2 + 32 + 1 + session_id_len + 2 + 2 + 1 + 1 + 14;
    if (host && host_len) body += 9 + host_len;   /* SNI extension */
    size_t total = 4 + body;
    if (total > out_cap) return -1;
    if (total > TLS_HS_MAX_BODY + 4) return -1;

    out[0] = HS_CLIENT_HELLO;
    if (put24(out, out_cap, 1, body) < 0) return -1;
    size_t p = 4;
    out[p++] = TLS_MAJOR;
    out[p++] = TLS_MINOR;
    memcpy(out + p, random, 32); p += 32;
    out[p++] = (uint8_t)session_id_len;
    if (session_id_len) {
        memcpy(out + p, session_id, session_id_len);
        p += session_id_len;
    }
    if (put16(out, out_cap, p, 2) < 0) return -1;       /* cipher_suites len */
    p += 2;
    out[p++] = (uint8_t)(TLS_CIPHER_AES128_CBC_SHA256 >> 8);
    out[p++] = (uint8_t)(TLS_CIPHER_AES128_CBC_SHA256 & 0xff);
    out[p++] = 1;                                        /* compression len */
    out[p++] = TLS_COMPRESSION_NULL;

    /* extensions: signature_algorithms (0x000d) [+ SNI (0x0000)] */
    if (put16(out, out_cap, p, 12 + (host && host_len ? 9 + host_len : 0)) < 0) return -1;
    p += 2;
    if (put16(out, out_cap, p, 0x000d) < 0) return -1;   /* ext type */
    p += 2;
    if (put16(out, out_cap, p, 8) < 0) return -1;        /* ext data len */
    p += 2;
    if (put16(out, out_cap, p, 6) < 0) return -1;        /* sigalgs list len */
    p += 2;
    out[p++] = 0x04; out[p++] = 0x01;                    /* SHA256 + RSA */
    out[p++] = 0x05; out[p++] = 0x01;                    /* SHA384 + RSA */
    out[p++] = 0x06; out[p++] = 0x01;                    /* SHA512 + RSA */

    if (host && host_len) {
        if (put16(out, out_cap, p, 0x0000) < 0) return -1;   /* server_name */
        p += 2;
        if (put16(out, out_cap, p, host_len + 5) < 0) return -1;  /* ext data len */
        p += 2;
        if (put16(out, out_cap, p, host_len + 3) < 0) return -1;  /* name list len */
        p += 2;
        out[p++] = 0x00;                                 /* host_name */
        if (put16(out, out_cap, p, host_len) < 0) return -1;
        p += 2;
        for (size_t i = 0; i < host_len; i++) out[p++] = (uint8_t)host[i];
    }
    if (p != total) return -1;

    *out_len = total;
    return 0;
}

/*
 * ServerHello parse. sifre takimi ve compression bizim sectiklerimizle
 * uyusmali; version 1.2 olmali. session_id (varsa) atlanir.
 */
int tls_parse_server_hello(const uint8_t *in, size_t in_len,
                           uint8_t server_random[32],
                           uint16_t *cipher_suite, uint8_t *compression) {
    if (in_len < 4) return -1;
    if (in[0] != HS_SERVER_HELLO) return -1;
    size_t body = ((size_t)in[1] << 16) | ((size_t)in[2] << 8) | in[3];
    if (body + 4 != in_len) return -1;
    if (body < 38) return -1;                            /* min govde */

    size_t p = 4;
    if (in[p] != TLS_MAJOR || in[p + 1] != TLS_MINOR) return -1;
    p += 2;
    memcpy(server_random, in + p, 32); p += 32;

    size_t sid_len = in[p]; p += 1;
    if (sid_len > 32 || p + sid_len + 3 > in_len) return -1;
    p += sid_len;

    uint16_t cs = (uint16_t)(((uint16_t)in[p] << 8) | in[p + 1]); p += 2;
    if (cs != TLS_CIPHER_AES128_CBC_SHA256) return -1;
    *cipher_suite = cs;

    uint8_t cm = in[p]; p += 1;
    if (cm != TLS_COMPRESSION_NULL) return -1;
    *compression = cm;

    return 0;
}

/*
 * Certificate parse: certificate_list<0..2^24-1>. Ilk sertifika (leaf)
 * leaf_der'e kopyalanir. Zincirdeki ek sertifikalar atlanir.
 */
int tls_parse_certificate(const uint8_t *in, size_t in_len,
                          uint8_t *leaf_der, size_t leaf_cap, size_t *leaf_len) {
    if (in_len < 4) return -1;
    if (in[0] != HS_CERTIFICATE) return -1;
    size_t body = ((size_t)in[1] << 16) | ((size_t)in[2] << 8) | in[3];
    if (body + 4 != in_len) return -1;

    size_t p = 4;
    if (body < 3) return -1;
    size_t list_len = ((size_t)in[p] << 16) | ((size_t)in[p + 1] << 8) | in[p + 2];
    p += 3;
    if (p + list_len != in_len) return -1;
    if (list_len < 3) return -1;

    size_t cert_len = ((size_t)in[p] << 16) | ((size_t)in[p + 1] << 8) | in[p + 2];
    p += 3;
    if (cert_len == 0 || p + cert_len > in_len) return -1;
    if (cert_len > leaf_cap) return -1;

    memcpy(leaf_der, in + p, cert_len);
    *leaf_len = cert_len;
    return 0;
}

/* ServerHelloDone: bos govde olmali */
int tls_parse_server_hello_done(const uint8_t *in, size_t in_len) {
    if (in_len != 4) return -1;
    if (in[0] != HS_SERVER_HELLO_DONE) return -1;
    if (in[1] || in[2] || in[3]) return -1;
    return 0;
}

/*
 * ClientKeyExchange (RSA): EncryptedPreMasterSecret<2..2^16-1>.
 * premaster = 48 bayt (0x0303 + 46 random) -> rsa_pkcs1_encrypt.
 */
int tls_build_client_key_exchange(const uint8_t premaster[TLS_PREMASTER_LEN],
                                  const uint8_t *server_modulus, int server_mod_len,
                                  unsigned int server_exponent,
                                  uint8_t *out, size_t out_cap, size_t *out_len) {
    if (server_mod_len < 3 || server_mod_len > 256) return -1;
    size_t body = 2 + (size_t)server_mod_len;
    size_t total = 4 + body;
    if (total > out_cap) return -1;

    out[0] = HS_CLIENT_KEY_EXCHANGE;
    if (put24(out, out_cap, 1, body) < 0) return -1;
    if (put16(out, out_cap, 4, (size_t)server_mod_len) < 0) return -1;

    rsa_pkcs1_encrypt(premaster, TLS_PREMASTER_LEN,
                      server_modulus, server_mod_len, server_exponent,
                      out + 6);
    *out_len = total;
    return 0;
}

/*
 * master_secret = PRF(premaster, "master secret",
 *                     client_random || server_random)[0..47]
 */
int tls_master_secret(const uint8_t premaster[TLS_PREMASTER_LEN],
                      const uint8_t client_random[32],
                      const uint8_t server_random[32],
                      uint8_t master[TLS_MASTER_LEN]) {
    uint8_t seed[64];
    memcpy(seed, client_random, 32);
    memcpy(seed + 32, server_random, 32);
    return tls_prf_sha256(premaster, TLS_PREMASTER_LEN,
                          "master secret", seed, 64, master, TLS_MASTER_LEN);
}

/*
 * Finished verify_data (RFC 5246 7.4.9):
 *   verify_data = PRF(master_secret, finished_label,
 *                     Hash(handshake_messages))[0..11]
 * hs_hash: handshake mesajlarinin kumulatif SHA-256'si (kopyalanir).
 */
int tls_finished_verify_data(const uint8_t master[TLS_MASTER_LEN],
                             const char *label,
                             const sha256_context *hs_hash,
                             uint8_t verify[TLS_VERIFY_DATA_LEN]) {
    sha256_context copy = *hs_hash;
    uint8_t hash[SHA256_OUT];
    sha256_final(&copy, hash);
    return tls_prf_sha256(master, TLS_MASTER_LEN, label,
                          hash, SHA256_OUT, verify, TLS_VERIFY_DATA_LEN);
}

/* ChangeCipherSpec record: 14 03 03 00 01 01 (RFC 5246 7.1) */
void tls_build_change_cipher_spec(uint8_t out[6]) {
    out[0] = 20;                     /* content type change_cipher_spec */
    out[1] = TLS_MAJOR;
    out[2] = TLS_MINOR;
    out[3] = 0;
    out[4] = 1;
    out[5] = 1;                      /* current state */
}

#ifdef TLS_HANDSHAKE_TEST_MAIN
/* ---- native gcc testi ---- */
#include <stdio.h>

static int hex_to_bytes(const char *hex, uint8_t *out, size_t out_cap, size_t *n) {
    size_t hlen = 0;
    while (hex[hlen]) hlen++;
    if (hlen % 2) return -1;
    size_t blen = hlen / 2;
    if (blen > out_cap) return -1;
    for (size_t i = 0; i < blen; i++) {
        uint8_t hi = 0, lo = 0;
        char c;
        c = hex[2*i];   hi = (c >= '0' && c <= '9') ? (uint8_t)(c - '0') :
                            (c >= 'a' && c <= 'f') ? (uint8_t)(c - 'a' + 10) : 0xff;
        c = hex[2*i+1]; lo = (c >= '0' && c <= '9') ? (uint8_t)(c - '0') :
                            (c >= 'a' && c <= 'f') ? (uint8_t)(c - 'a' + 10) : 0xff;
        if (hi == 0xff || lo == 0xff) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    *n = blen;
    return 0;
}

int main(void) {
    int fail = 0;
    uint8_t buf[16384 + 8];
    size_t n, olen;

    /* 1. ClientHello (python referansi) */
    {
        uint8_t random[32];
        for (int i = 0; i < 32; i++) random[i] = (uint8_t)i;
        const char *exp_hex =
            "010000370303000102030405060708090a0b0c0d0e0f101112131415161718"
            "191a1b1c1d1e1f000002003c0100000c000d00080006040105010601";
        if (tls_build_client_hello(random, NULL, 0, NULL, buf, sizeof buf, &olen) != 0) {
            printf("FAIL: ClientHello build\n"); fail = 1;
        } else {
            size_t elen;
            hex_to_bytes(exp_hex, buf + sizeof(buf) - 256, 256, &elen);
            if (olen != elen || memcmp(buf, buf + sizeof(buf) - 256, elen) != 0) {
                printf("FAIL: ClientHello degeri\n"); fail = 1;
            } else printf("OK: ClientHello (python)\n");
        }
    }

    /* 2. ServerHello parse (python referansi) */
    {
        const char *sh_hex =
            "020000260303202122232425262728292a2b2c2d2e2f3031323334353637"
            "38393a3b3c3d3e3f00003c00";
        hex_to_bytes(sh_hex, buf, sizeof buf, &n);
        uint8_t srand[32];
        uint16_t cs = 0;
        uint8_t cm = 0xff;
        int ok = (tls_parse_server_hello(buf, n, srand, &cs, &cm) == 0);
        for (int i = 0; ok && i < 32; i++)
            if (srand[i] != (uint8_t)(i + 32)) ok = 0;
        if (ok && cs != 0x003c) ok = 0;
        if (ok && cm != 0x00) ok = 0;
        if (!ok) { printf("FAIL: ServerHello parse\n"); fail = 1; }
        else printf("OK: ServerHello parse (python)\n");

        /* bozuk version -> red */
        buf[4] = 3; buf[5] = 1;                     /* TLS 1.0 */
        if (tls_parse_server_hello(buf, n, srand, &cs, &cm) == 0) {
            printf("FAIL: TLS 1.0 kabul edildi\n"); fail = 1;
        } else printf("OK: TLS 1.0 reddedildi\n");
    }

    /* 3. Certificate parse (openssl zinciri, leaf cikarilir) */
    {
        extern uint8_t chain_leaf_der[];
        extern unsigned int chain_leaf_der_len;
        uint8_t leaf[4096];
        size_t leaf_len = 0;
        if (tls_parse_certificate(chain_leaf_der, chain_leaf_der_len,
                                  leaf, sizeof leaf, &leaf_len) != 0) {
            printf("FAIL: Certificate parse\n"); fail = 1;
        } else {
            /* leaf DER SEQUENCE ile baslamali (0x30) */
            if (leaf_len < 4 || leaf[0] != 0x30) {
                printf("FAIL: Certificate leaf DER\n"); fail = 1;
            } else printf("OK: Certificate parse (openssl zinciri, leaf %d bayt)\n",
                          (int)leaf_len);
        }
    }

    /* 4. ServerHelloDone parse */
    {
        uint8_t done[4] = { 14, 0, 0, 0 };
        if (tls_parse_server_hello_done(done, 4) != 0) {
            printf("FAIL: ServerHelloDone\n"); fail = 1;
        } else printf("OK: ServerHelloDone parse\n");
        done[3] = 1;
        if (tls_parse_server_hello_done(done, 4) == 0) {
            printf("FAIL: dolu ServerHelloDone kabul edildi\n"); fail = 1;
        } else printf("OK: dolu ServerHelloDone reddedildi\n");
    }

    /* 4b. SNI extension (host="localhost") */
    {
        uint8_t random[32];
        for (int i = 0; i < 32; i++) random[i] = (uint8_t)i;
        const char *exp_hex =
            "010000490303000102030405060708090a0b0c0d0e0f101112131415161718"
            "191a1b1c1d1e1f000002003c0100001e000d000800060401050106010000000e"
            "000c0000096c6f63616c686f7374";
        if (tls_build_client_hello(random, NULL, 0, "localhost",
                                   buf, sizeof buf, &olen) != 0) {
            printf("FAIL: SNI ClientHello build\n"); fail = 1;
        } else {
            size_t elen;
            hex_to_bytes(exp_hex, buf + sizeof(buf) - 256, 256, &elen);
            if (olen != elen || memcmp(buf, buf + sizeof(buf) - 256, elen) != 0) {
                printf("FAIL: SNI ClientHello degeri (olen=%zu elen=%zu)\n", olen, elen);
                fail = 1;
            } else printf("OK: SNI ClientHello (python)\n");
        }
    }

    /* 5. Master secret (python referansi) */
    {
        const char *ms_hex =
            "30ca046932f25d320c3471a59d76080b7a8908ed767d36ecb80e1ed156e3d1"
            "7dc9f5ecd5fb4d1b410169fa9d9c442936";
        uint8_t premaster[48], cr[32], sr[32], master[48];
        memset(premaster, 0x13, 48);
        for (int i = 0; i < 32; i++) { cr[i] = (uint8_t)i; sr[i] = (uint8_t)(i + 32); }
        if (tls_master_secret(premaster, cr, sr, master) != 0) {
            printf("FAIL: master secret\n"); fail = 1;
        } else {
            size_t elen;
            hex_to_bytes(ms_hex, buf + sizeof(buf) - 256, 256, &elen);
            if (memcmp(master, buf + sizeof(buf) - 256, 48) != 0) {
                printf("FAIL: master secret degeri\n"); fail = 1;
            } else printf("OK: master secret (python)\n");
        }
    }

    /* 6. Finished verify_data (python referansi) */
    {
        const char *fh_hex = "c5c360030457969eee2b83f8";
        /* handshake mesajlari: ClientHello (59) + ServerHello (42) */
        uint8_t msgs[59 + 42], master[48], verify[12];
        uint8_t random[32];
        for (int i = 0; i < 32; i++) random[i] = (uint8_t)i;
        if (tls_build_client_hello(random, NULL, 0, NULL, msgs, sizeof msgs, &n) != 0) {
            printf("FAIL: Finished (ch build)\n"); fail = 1;
        } else {
            size_t sh_len;
            hex_to_bytes("020000260303202122232425262728292a2b2c2d2e2f303132"
                         "333435363738393a3b3c3d3e3f00003c00",
                         msgs + n, sizeof(msgs) - n, &sh_len);
            memset(master, 0x77, 48);
            sha256_context c;
            sha256_init(&c);
            sha256_update(&c, msgs, n + sh_len);
            if (tls_finished_verify_data(master, "client finished", &c, verify) != 0) {
                printf("FAIL: Finished\n"); fail = 1;
            } else {
                size_t elen;
                hex_to_bytes(fh_hex, buf + sizeof(buf) - 256, 256, &elen);
                if (memcmp(verify, buf + sizeof(buf) - 256, 12) != 0) {
                    printf("FAIL: Finished degeri\n"); fail = 1;
                } else printf("OK: Finished verify_data (python)\n");
            }
        }
    }

    /* 7. ClientKeyExchange: gercek openssl 512-bit sunucu anahtari ile,
       ciphertext openssl pkeyutl -decrypt ile dogrulanir (harici adim). */
    {
        extern uint8_t server_mod_der[];
        extern unsigned int server_mod_der_len;
        uint8_t premaster[48] = { 3, 3 };
        for (int i = 2; i < 48; i++) premaster[i] = (uint8_t)(i * 13 + 7);
        if (tls_build_client_key_exchange(premaster, server_mod_der, server_mod_der_len,
                                          65537, buf, sizeof buf, &olen) != 0) {
            printf("FAIL: ClientKeyExchange build\n"); fail = 1;
        } else {
            printf("OK: ClientKeyExchange build (mod %d bayt -> cipher %d bayt)\n",
                   (int)server_mod_der_len, (int)olen - 6);
            printf("CKE_CT: ");
            for (size_t i = 6; i < olen; i++) printf("%02x", buf[i]);
            printf("\n");
        }
    }

    /* 8. ChangeCipherSpec */
    {
        uint8_t ccs[6];
        tls_build_change_cipher_spec(ccs);
        uint8_t exp[6] = { 20, 3, 3, 0, 1, 1 };
        if (memcmp(ccs, exp, 6) != 0) {
            printf("FAIL: ChangeCipherSpec\n"); fail = 1;
        } else printf("OK: ChangeCipherSpec (14 03 03 00 01 01)\n");
    }

    printf(fail ? "\nSONUC: FAIL\n" : "\nSONUC: OK\n");
    return fail;
}
#endif
