#ifndef WPA_SUPPLICANT_H
#define WPA_SUPPLICANT_H

#include "types.h"

#define WPA_NONCE_LEN  32
#define WPA_MIC_LEN    16
#define WPA_PMK_LEN    32
#define WPA_PTK_LEN    384
#define WPA_KEY_DATA_MAX 256

typedef struct {
    u8  pmk[WPA_PMK_LEN];
    u8  anonce[WPA_NONCE_LEN];
    u8  snonce[WPA_NONCE_LEN];
    u8  ptk[WPA_PTK_LEN];
    u8  mic[WPA_MIC_LEN];
    u8  addr1[6];
    u8  addr2[6];
    int step;
    int valid;
} wpa_handshake_t;

void  wpa_init(void);
int   wpa_derive_pmk(const char *ssid, const char *passphrase, u8 *pmk_out);
int   wpa_handshake_start(wpa_handshake_t *hs, const u8 *bssid);
int   wpa_handshake_step2(wpa_handshake_t *hs, const u8 *eapol_data, int eapol_len);
int   wpa_handshake_verify(wpa_handshake_t *hs, const u8 *eapol_data, int eapol_len);
int   wpa_is_complete(wpa_handshake_t *hs);

#endif
