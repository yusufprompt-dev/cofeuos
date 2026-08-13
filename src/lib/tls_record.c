/*
 * CofeuOS TLS - Bilesen 4: TLS 1.2 Record Layer (RFC 5246 6.2)
 *
 * Cipher suite: TLS_RSA_WITH_AES_128_CBC_SHA256 (0x003C)
 *   - MAC:    HMAC-SHA256, mac_key 32 bayt
 *   - Simetri: AES-128-CBC, enc_key 16 bayt, explicit IV (her kayit yeni)
 *   - TLS CBC padding: P bayt, her biri (P-1) degerinde (PKCS#7'den farkli)
 *
 * Bilesenler: aes.c (blok sifre) + tls_prf.c (HMAC-SHA256, PRF).
 * malloc yok; tum buffer'lar sabit boyutlu.
 */
#include <stdint.h>
#include "../include/string.h"
#include "../include/tls_crypto.h"

#define TLS_REC_MAX_PLAIN   16384        /* TLS max fragment */
#define TLS_REC_MAX_TOTAL   (TLS_REC_MAX_PLAIN + 16 + 32 + 256)  /* +iv+mac+pad */
#define TLS_MAC_LEN         32
#define TLS_ENC_KEY_LEN     16
#define TLS_IV_LEN          16
#define TLS_MAJOR           3
#define TLS_MINOR           3
#define AES_ROUND_KEYS      176

extern void aes128_key_expand(const uint8_t key[16], uint8_t rk[AES_ROUND_KEYS]);
extern void aes128_encrypt_block(const uint8_t in[16], const uint8_t rk[AES_ROUND_KEYS],
                                 uint8_t out[16]);
extern void aes128_decrypt_block(const uint8_t in[16], const uint8_t rk[AES_ROUND_KEYS],
                                 uint8_t out[16]);
extern void hmac_sha256(const uint8_t *key, size_t key_len,
                        const uint8_t *msg, size_t msg_len, uint8_t out[32]);
extern void hmac_sha256_begin(hmac_sha256_ctx *c, const uint8_t *key, size_t key_len);
extern void hmac_sha256_update(hmac_sha256_ctx *c, const uint8_t *data, size_t len);
extern void hmac_sha256_final(hmac_sha256_ctx *c, uint8_t out[32]);
extern int  tls_prf_sha256(const uint8_t *secret, size_t secret_len,
                           const char *label,
                           const uint8_t *seed, size_t seed_len,
                           uint8_t *out, size_t out_len);

/* sabit-zamanli esitlik karsilastirmasi (MAC/pad dogrulama) */
static int ct_eq(const uint8_t *a, const uint8_t *b, size_t n) {
    uint8_t diff = 0;
    for (size_t i = 0; i < n; i++) diff |= a[i] ^ b[i];
    return diff == 0;
}

static void mac_header(uint8_t hdr[13], uint64_t seq, uint8_t type,
                       uint16_t plain_len) {
    for (int i = 0; i < 8; i++)
        hdr[i] = (uint8_t)(seq >> (56 - 8 * i));   /* seq_num, big-endian */
    hdr[8]  = type;
    hdr[9]  = TLS_MAJOR;
    hdr[10] = TLS_MINOR;
    hdr[11] = (uint8_t)((plain_len >> 8) & 0xff);
    hdr[12] = (uint8_t)(plain_len & 0xff);
}

/*
 * key_block = PRF(master_secret, "key expansion", server_random || client_random)
 * kullanici bu ciktiyi mac_key / enc_key / fixed_iv olarak boler.
 */
int tls12_key_block(const uint8_t master[48],
                    const uint8_t server_random[32], const uint8_t client_random[32],
                    uint8_t *out, size_t out_len) {
    uint8_t seed[64];
    memcpy(seed, server_random, 32);
    memcpy(seed + 32, client_random, 32);
    return tls_prf_sha256(master, 48, "key expansion", seed, 64, out, out_len);
}

/*
 * Kayit sifreleme. out = explicit_iv(16) || ciphertext.
 * plain_len >= 1 ve <= 16384. seq: bu kaydin sequence number'i.
 */
int tls_record_encrypt(const uint8_t mac_key[32], const uint8_t enc_key[16],
                       const uint8_t explicit_iv[16],
                       uint64_t seq, uint8_t type,
                       const uint8_t *plain, size_t plain_len,
                       uint8_t *out, size_t out_cap, size_t *out_len) {
    if (plain_len == 0 || plain_len > TLS_REC_MAX_PLAIN) return -1;

    uint8_t hdr[13];
    mac_header(hdr, seq, type, (uint16_t)plain_len);

    uint8_t mac[TLS_MAC_LEN];
    hmac_sha256_ctx hc;
    hmac_sha256_begin(&hc, mac_key, TLS_MAC_LEN);
    hmac_sha256_update(&hc, hdr, 13);
    hmac_sha256_update(&hc, plain, plain_len);
    hmac_sha256_final(&hc, mac);

    size_t frag = plain_len + TLS_MAC_LEN;
    size_t pad  = 16 - (frag % 16);            /* 1..16, her zaman en az 1 */
    size_t total = frag + pad;
    size_t out_total = TLS_IV_LEN + total;
    if (out_total > out_cap) return -1;

    memcpy(out, explicit_iv, TLS_IV_LEN);
    memcpy(out + TLS_IV_LEN, plain, plain_len);
    memcpy(out + TLS_IV_LEN + plain_len, mac, TLS_MAC_LEN);
    for (size_t i = 0; i < pad; i++)
        out[TLS_IV_LEN + frag + i] = (uint8_t)(pad - 1);

    /* CBC sifrele (in-place: ciphertext prev olarak sonraki blok girdisidir) */
    uint8_t rk[AES_ROUND_KEYS];
    aes128_key_expand(enc_key, rk);
    uint8_t prev[TLS_IV_LEN], blk[TLS_IV_LEN];
    memcpy(prev, explicit_iv, TLS_IV_LEN);
    for (size_t off = 0; off < total; off += 16) {
        memcpy(blk, out + TLS_IV_LEN + off, 16);
        for (int i = 0; i < 16; i++) blk[i] ^= prev[i];
        aes128_encrypt_block(blk, rk, prev);
        memcpy(out + TLS_IV_LEN + off, prev, 16);
    }

    *out_len = out_total;
    return 0;
}

/*
 * Kayit cozme. in = explicit_iv(16) || ciphertext.
 * Pad + MAC dogrulanir; tip uyusmazligi da MAC sayesinde reddedilir.
 */
int tls_record_decrypt(const uint8_t mac_key[32], const uint8_t enc_key[16],
                       uint64_t seq, uint8_t expected_type,
                       const uint8_t *in, size_t in_len,
                       uint8_t *out, size_t out_cap, size_t *out_len) {
    if (in_len < TLS_IV_LEN + 16) return -1;
    size_t ct_len = in_len - TLS_IV_LEN;
    if ((ct_len % 16) != 0) return -1;
    if (ct_len > out_cap) return -1;           /* out buffer ct_len almali */

    /* CBC cozum */
    uint8_t rk[AES_ROUND_KEYS];
    aes128_key_expand(enc_key, rk);
    uint8_t prev[TLS_IV_LEN], dec[TLS_IV_LEN];
    memcpy(prev, in, TLS_IV_LEN);
    for (size_t off = 0; off < ct_len; off += 16) {
        aes128_decrypt_block(in + TLS_IV_LEN + off, rk, dec);
        for (int i = 0; i < 16; i++) dec[i] ^= prev[i];
        memcpy(prev, in + TLS_IV_LEN + off, 16);
        memcpy(out + off, dec, 16);
    }

    /* TLS CBC padding: son bayt (P-1), toplam P bayt, hepsi (P-1) */
    uint8_t last = out[ct_len - 1];
    size_t pad_len = (size_t)last + 1;
    if (pad_len > ct_len) return -1;
    size_t mac_and_plain = ct_len - pad_len;
    if (mac_and_plain < TLS_MAC_LEN) return -1;
    size_t plain_len = mac_and_plain - TLS_MAC_LEN;
    if (plain_len == 0 || plain_len > TLS_REC_MAX_PLAIN) return -1;
    if (plain_len > out_cap) return -1;
    for (size_t i = 0; i < pad_len; i++) {
        if (out[ct_len - 1 - i] != last) return -1;      /* pad dogrula */
    }

    /* MAC dogrula */
    const uint8_t *plain = out;
    const uint8_t *mac   = out + plain_len;
    uint8_t hdr[13];
    mac_header(hdr, seq, expected_type, (uint16_t)plain_len);
    uint8_t mac_calc[TLS_MAC_LEN];
    hmac_sha256_ctx hc;
    hmac_sha256_begin(&hc, mac_key, TLS_MAC_LEN);
    hmac_sha256_update(&hc, hdr, 13);
    hmac_sha256_update(&hc, plain, plain_len);
    hmac_sha256_final(&hc, mac_calc);
    if (!ct_eq(mac_calc, mac, TLS_MAC_LEN)) return -1;

    *out_len = plain_len;
    return 0;
}

void tls_seq_inc(uint64_t *seq) {
    (*seq)++;
}

#ifdef TLS_RECORD_TEST_MAIN
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

static int hex_eq_b(const uint8_t *a, const uint8_t *b, size_t n) {
    return memcmp(a, b, n) == 0;
}

int main(void) {
    int fail = 0;
    uint8_t mac_key[32], enc_key[16], iv[16];
    uint8_t buf[64 + TLS_REC_MAX_TOTAL];
    uint8_t dout[64 + TLS_REC_MAX_TOTAL];
    const size_t dcap = sizeof dout;
    size_t n, olen;

    memset(mac_key, 0x5c, 32);
    memset(enc_key, 0x2a, 16);
    for (int i = 0; i < 16; i++) iv[i] = (uint8_t)i;

    /* 1. key block (python referansi) */
    {
        uint8_t master[48], srand[32], crand[32], kb[104];
        const char *kb_hex =
            "8473b0c0acb5489b83c8a4fe6980387941d081567654a943e990f093e5b577a0"
            "1f5bac2b1cdd0ae7c878b1eefb7debdf7b06f76389bd8116783686e1c7de8692"
            "eb9d93ea6821484b05765758d52d813e55848f91c517bce3125f835ac6b58d2e"
            "5c75924e0f7cbefd";
        memset(master, 0x11, 48);
        memset(srand, 0x33, 32);
        memset(crand, 0x44, 32);
        if (tls12_key_block(master, srand, crand, kb, 104) != 0) {
            printf("FAIL: key_block donus\n"); fail = 1;
        } else if (hex_to_bytes(kb_hex, buf, sizeof buf, &n) || !hex_eq_b(kb, buf, 104)) {
            printf("FAIL: key_block degeri\n"); fail = 1;
        } else printf("OK: key_block (104 bayt, python)\n");
    }

    /* 2. encrypt vektoru: python ile birebir */
    {
        const char *rec_hex =
            "000102030405060708090a0b0c0d0e0f3caa86dd955157b8cfa89618b15194de"
            "d7454bae0eadf55006327f0f59db1910b04fdcbefffb54cc67b82da58e859c9a";
        const uint8_t plain[] = "Hello CofeuOS";          /* 13 bayt */
        size_t rec_len;
        uint8_t *exp = dout;
        hex_to_bytes(rec_hex, exp, dcap, &rec_len);
        if (tls_record_encrypt(mac_key, enc_key, iv, 0, 22, plain,
                               sizeof(plain) - 1,
                               buf, sizeof buf, &olen) != 0) {
            printf("FAIL: encrypt vektoru (donus)\n"); fail = 1;
        } else if (olen != rec_len || !hex_eq_b(buf, exp, rec_len)) {
            printf("FAIL: encrypt vektoru (deger)\n"); fail = 1;
        } else printf("OK: encrypt vektoru (python)\n");
    }

    /* 3. decrypt vektoru: python ciphertext -> plaintext */
    {
        const char *rec2_hex =
            "000102030405060708090a0b0c0d0e0f94566bbb26fb2bfc4b3fd6c60600e9dd"
            "156f6402897cd3b36d3070c4bdfa7a7eaed788c506fceb6eb1145e81946007e7";
        hex_to_bytes(rec2_hex, buf, sizeof buf, &n);
        if (tls_record_decrypt(mac_key, enc_key, 1, 23, buf, n,
                               dout, dcap, &olen) != 0) {
            printf("FAIL: decrypt vektoru\n"); fail = 1;
        } else if (olen != 9 || memcmp(dout, "payload-2", 9) != 0) {
            printf("FAIL: decrypt vektoru plaintext\n"); fail = 1;
        } else printf("OK: decrypt vektoru (python)\n");
    }

    /* 4. roundtrip: farkli seq/type/uzunluk */
    {
        uint8_t pt[TLS_REC_MAX_PLAIN];
        for (size_t i = 0; i < sizeof pt; i++) pt[i] = (uint8_t)(i * 7 + 3);
        int ok = 1;
        uint64_t seq = 0;
        size_t lens[] = { 1, 15, 16, 17, 31, 32, 33, 1000, TLS_REC_MAX_PLAIN };
        uint8_t types[] = { 22, 23, 20, 21 };
        for (unsigned li = 0; li < sizeof(lens)/sizeof(lens[0]); li++) {
            for (unsigned ti = 0; ti < sizeof(types)/sizeof(types[0]); ti++) {
                if (tls_record_encrypt(mac_key, enc_key, iv, seq, types[ti],
                                       pt, lens[li], buf, sizeof buf, &olen) != 0) { ok = 0; break; }
                if (tls_record_decrypt(mac_key, enc_key, seq, types[ti],
                                       buf, olen, dout, dcap, &n) != 0) { ok = 0; break; }
                if (n != lens[li] || !hex_eq_b(dout, pt, lens[li])) { ok = 0; break; }
                tls_seq_inc(&seq);
            }
            if (!ok) break;
        }
        if (!ok) { printf("FAIL: roundtrip\n"); fail = 1; }
        else printf("OK: roundtrip (9 uzunluk x 4 tip, seq buyuyen)\n");
    }

    /* 5. bozuk MAC -> red */
    {
        const uint8_t plain[] = "tamper me";              /* 9 bayt */
        tls_record_encrypt(mac_key, enc_key, iv, 0, 22, plain,
                           sizeof(plain) - 1,
                           buf, sizeof buf, &olen);
        buf[olen - 1] ^= 0x01;                     /* ciphertext son bayti */
        if (tls_record_decrypt(mac_key, enc_key, 0, 22, buf, olen,
                               dout, dcap, &n) == 0) {
            printf("FAIL: bozuk MAC kabul edildi\n"); fail = 1;
        } else printf("OK: bozuk MAC reddedildi\n");
    }

    /* 6. bozuk pad -> red */
    {
        const uint8_t plain[] = "pad tamper";             /* 10 bayt */
        tls_record_encrypt(mac_key, enc_key, iv, 0, 22, plain,
                           sizeof(plain) - 1,
                           buf, sizeof buf, &olen);
        /* son blokta pad baytini boz (10+32=42 -> pad 6) */
        size_t pad_i = olen - 6;
        buf[pad_i] ^= 0x40;
        if (tls_record_decrypt(mac_key, enc_key, 0, 22, buf, olen,
                               dout, dcap, &n) == 0) {
            printf("FAIL: bozuk pad kabul edildi\n"); fail = 1;
        } else printf("OK: bozuk pad reddedildi\n");
    }

    /* 7. yanlis tip -> red (MAC header'a tip girdigi icin) */
    {
        const uint8_t plain[] = "type check";
        tls_record_encrypt(mac_key, enc_key, iv, 0, 22, plain, 10,
                           buf, sizeof buf, &olen);
        if (tls_record_decrypt(mac_key, enc_key, 0, 23, buf, olen,
                               dout, dcap, &n) == 0) {
            printf("FAIL: yanlis tip kabul edildi\n"); fail = 1;
        } else printf("OK: yanlis tip reddedildi\n");
    }

    /* 8. kirik girdiler -> red */
    {
        int ok = 1;
        uint8_t tiny[20];
        if (tls_record_decrypt(mac_key, enc_key, 0, 22, tiny, 16,
                               dout, dcap, &n) == 0) ok = 0;
        if (tls_record_decrypt(mac_key, enc_key, 0, 22, buf, 31,      /* %16 uymaz */
                               dout, dcap, &n) == 0) ok = 0;
        if (tls_record_encrypt(mac_key, enc_key, iv, 0, 22, buf, 0,
                               dout, dcap, &n) == 0) ok = 0;
        if (tls_record_encrypt(mac_key, enc_key, iv, 0, 22, buf, TLS_REC_MAX_PLAIN + 1,
                               dout, dcap, &n) == 0) ok = 0;
        if (!ok) { printf("FAIL: kirik girdi kabul edildi\n"); fail = 1; }
        else printf("OK: kirik girdiler reddedildi\n");
    }

    printf(fail ? "\nSONUC: FAIL\n" : "\nSONUC: OK\n");
    return fail;
}
#endif
