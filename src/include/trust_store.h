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
#ifndef TRUST_STORE_H
#define TRUST_STORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Trust store blob'ini bellege yukle (network.c dosya okuduktan sonra cagirir) */
int trust_store_load_blob(const uint8_t *data, int size);

/* Parsed CA sertifikalarini al (x509_verify_chain icin) */
int trust_store_get_certs(const unsigned char ***out_certs,
                          int **out_lens, int *out_count);

/* Legacy struct-based API (UEFI dosya sistemi icin) */
struct trust_store;
int trust_store_load(struct trust_store *ts, const char *path);
int trust_store_get_certs_legacy(struct trust_store *ts,
                                 const unsigned char ***out_certs,
                                 int **out_lens, int *out_count);

#ifdef __cplusplus
}
#endif

#endif /* TRUST_STORE_H */