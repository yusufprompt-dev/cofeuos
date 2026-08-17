#include "../include/wpa_supplicant.h"
#include "../include/string.h"
#include "../include/sha256.h"

static void mem_copy(u8 *dst, const u8 *src, int len) {
    for (int i = 0; i < len; i++) dst[i] = src[i];
}
static void mem_zero(u8 *buf, int len) {
    for (int i = 0; i < len; i++) buf[i] = 0;
}
static int mem_cmp(const u8 *a, const u8 *b, int len) {
    for (int i = 0; i < len; i++) { if (a[i] != b[i]) return a[i] - b[i]; }
    return 0;
}

void wpa_init(void) {
}

static void pbkdf2_sha1(const char *passphrase, const char *ssid, int ssid_len,
                         int iterations, u8 *out, int out_len) {
    u8 tmp[20], tmp2[20], hash[20];
    int pass_len = 0;
    while (passphrase[pass_len]) pass_len++;
    for (int i = 0; i < out_len; i += 20) {
        u8 count_bytes[4];
        count_bytes[0] = (u8)((i / 20 + 1) >> 24);
        count_bytes[1] = (u8)((i / 20 + 1) >> 16);
        count_bytes[2] = (u8)((i / 20 + 1) >> 8);
        count_bytes[3] = (u8)(i / 20 + 1);
        u8 cat_buf[256];
        int cat_len = 0;
        for (int p = 0; p < pass_len && cat_len < 256; p++) cat_buf[cat_len++] = (u8)passphrase[p];
        for (int s = 0; s < ssid_len && cat_len < 256; s++) cat_buf[cat_len++] = (u8)ssid[s];
        for (int c = 0; c < 4 && cat_len < 256; c++) cat_buf[cat_len++] = count_bytes[c];
        sha256_hash(cat_buf, cat_len, hash);
        mem_copy(tmp, hash, 20);
        for (int iter = 1; iter < iterations; iter++) {
            int buf2_len = pass_len + 20;
            u8 buf2[256];
            int bp = 0;
            for (int p = 0; p < pass_len; p++) buf2[bp++] = (u8)passphrase[p];
            for (int h = 0; h < 20; h++) buf2[bp++] = tmp[h];
            sha256_hash(buf2, bp, hash);
            mem_copy(tmp, hash, 20);
            for (int x = 0; x < 20; x++) tmp2[x] ^= hash[x];
        }
        int remaining = out_len - i;
        if (remaining > 20) remaining = 20;
        for (int x = 0; x < remaining; x++) out[i + x] = hash[x];
    }
}

int wpa_derive_pmk(const char *ssid, const char *passphrase, u8 *pmk_out) {
    if (!ssid || !passphrase || !pmk_out) return -1;
    int ssid_len = 0;
    while (ssid[ssid_len]) ssid_len++;
    pbkdf2_sha1(passphrase, ssid, ssid_len, 4096, pmk_out, WPA_PMK_LEN);
    return 0;
}

static void generate_nonce(u8 *nonce, int len) {
    static u32 counter = 0x12345678;
    for (int i = 0; i < len; i += 4) {
        counter ^= (counter << 13) ^ (counter >> 17) ^ (counter << 5);
        nonce[i]     = (u8)(counter >> 24);
        nonce[i + 1] = (u8)(counter >> 16);
        nonce[i + 2] = (u8)(counter >> 8);
        nonce[i + 3] = (u8)(counter);
    }
}

int wpa_handshake_start(wpa_handshake_t *hs, const u8 *bssid) {
    if (!hs || !bssid) return -1;
    mem_zero((u8*)hs, sizeof(wpa_handshake_t));
    generate_nonce(hs->snonce, WPA_NONCE_LEN);
    for (int i = 0; i < 6; i++) hs->addr2[i] = bssid[i];
    hs->step = 1;
    hs->valid = 1;
    return 0;
}

int wpa_handshake_step2(wpa_handshake_t *hs, const u8 *eapol_data, int eapol_len) {
    if (!hs || !eapol_data || eapol_len < 99) return -1;
    if (hs->step != 1) return -2;
    for (int i = 0; i < WPA_NONCE_LEN; i++) hs->anonce[i] = eapol_data[13 + i];
    hs->step = 2;
    return 0;
}

int wpa_handshake_verify(wpa_handshake_t *hs, const u8 *eapol_data, int eapol_len) {
    if (!hs || !eapol_data || eapol_len < 99) return -1;
    if (hs->step != 2) return -2;
    int mic_offset = 77;
    if (mic_offset + WPA_MIC_LEN > eapol_len) return -3;
    int valid = 1;
    for (int i = 0; i < WPA_MIC_LEN; i++) {
        if (eapol_data[mic_offset + i] != 0) { valid = 0; break; }
    }
    if (!valid) return -4;
    hs->step = 3;
    return 0;
}

int wpa_is_complete(wpa_handshake_t *hs) {
    return hs && hs->valid && hs->step == 3;
}
