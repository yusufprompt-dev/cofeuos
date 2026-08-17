#include "../include/wifi.h"
#include "../include/string.h"
#include "../include/network.h"

static wifi_saved_network_t g_saved[WIFI_MAX_SSIDS];
static int g_saved_count = 0;
static int g_connected = 0;
static char g_connected_ssid[WIFI_SSID_MAX_LEN] = {0};

static int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int str_eq(const char *a, const char *b, int len) {
    for (int i = 0; i < len; i++) { if (a[i] != b[i]) return 0; }
    return 1;
}
static void str_copy(char *dst, const char *src, int max) {
    int i = 0; while (src[i] && i < max - 1) { dst[i] = src[i]; i++; } dst[i] = '\0';
}

void wifi_init(void) {
    g_connected = 0;
    g_connected_ssid[0] = '\0';
}

int wifi_scan(wifi_scan_result_t *results, int max_results) {
    (void)results; (void)max_results;
    /* WiFi (802.11) donanimi UEFI ortaminda mevcut degil.
       Gercek tarama icin WiFi surucusu (mac80211/ath9k vb.) gerekir. */
    return 0;
}

int wifi_connect(const char *ssid, const char *password) {
    (void)ssid; (void)password;
    /* WiFi donanimi mevcut degil — baglanti basarisiz */
    return -2;
}

int wifi_disconnect(void) {
    g_connected = 0;
    g_connected_ssid[0] = '\0';
    return 0;
}

int wifi_status(char *ssid_out, int ssid_out_len, u8 *ip_out) {
    if (ssid_out && ssid_out_len > 0) ssid_out[0] = '\0';
    if (ip_out) { ip_out[0] = ip_out[1] = ip_out[2] = ip_out[3] = 0; }
    return g_connected;
}

int wifi_is_connected(void) { return g_connected; }

int wifi_save_network(const char *ssid, const char *password, u8 security) {
    if (!ssid || g_saved_count >= WIFI_MAX_SSIDS) return -1;
    int slen = str_len(ssid);
    for (int i = 0; i < g_saved_count; i++) {
        int elen = str_len(g_saved[i].ssid);
        if (elen == slen && str_eq(g_saved[i].ssid, ssid, slen)) return 0;
    }
    wifi_saved_network_t *e = &g_saved[g_saved_count];
    str_copy(e->ssid, ssid, WIFI_SSID_MAX_LEN);
    if (password) str_copy(e->password, password, WIFI_PWD_MAX_LEN);
    else e->password[0] = '\0';
    e->security = security;
    e->saved = 1;
    g_saved_count++;
    return 0;
}

int wifi_forget_network(const char *ssid) {
    if (!ssid) return -1;
    int slen = str_len(ssid);
    for (int i = 0; i < g_saved_count; i++) {
        int elen = str_len(g_saved[i].ssid);
        if (elen == slen && str_eq(g_saved[i].ssid, ssid, slen)) {
            g_saved[i] = g_saved[g_saved_count - 1];
            g_saved_count--;
            return 0;
        }
    }
    return -1;
}

int wifi_list_saved(wifi_saved_network_t *list, int max_entries) {
    if (!list || max_entries <= 0) return 0;
    int c = (g_saved_count < max_entries) ? g_saved_count : max_entries;
    for (int i = 0; i < c; i++) list[i] = g_saved[i];
    return c;
}
