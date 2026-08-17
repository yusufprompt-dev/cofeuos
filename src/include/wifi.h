#ifndef WIFI_H
#define WIFI_H

#include "types.h"

#define WIFI_MAX_SSIDS      32
#define WIFI_SSID_MAX_LEN   33
#define WIFI_BSSID_LEN      6
#define WIFI_PWD_MAX_LEN    64

typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    u8   bssid[WIFI_BSSID_LEN];
    s8   rssi;
    u8   channel;
    u8   security;
    u8   active;
} wifi_scan_result_t;

typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PWD_MAX_LEN];
    u8   security;
    int  saved;
} wifi_saved_network_t;

/* Security types */
#define WIFI_SEC_OPEN    0
#define WIFI_SEC_WEP     1
#define WIFI_SEC_WPA2    2
#define WIFI_SEC_WPA3    3

void wifi_init(void);
int  wifi_scan(wifi_scan_result_t *results, int max_results);
int  wifi_connect(const char *ssid, const char *password);
int  wifi_disconnect(void);
int  wifi_status(char *ssid_out, int ssid_out_len, u8 *ip_out);
int  wifi_is_connected(void);

/* Saved networks */
int  wifi_save_network(const char *ssid, const char *password, u8 security);
int  wifi_forget_network(const char *ssid);
int  wifi_list_saved(wifi_saved_network_t *list, int max_entries);

#endif
