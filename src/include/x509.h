/*
 * CofeuOS TLS - X.509 sertifika zinciri dogrulama API
 *
 * malloc yok; sabit buffer'lar.
 */
#ifndef X509_H
#define X509_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sertifika yapisi */
struct x509_cert {
    const unsigned char *tbs_start;
    int tbs_len;
    const unsigned char *sig_alg_oid;
    int sig_alg_oid_len;
    const unsigned char *signature;
    int sig_len;
    const unsigned char *subject;
    int subject_len;
    const unsigned char *issuer;
    int issuer_len;
    const unsigned char *not_before;
    int not_before_len;
    const unsigned char *not_after;
    int not_after_len;
    const unsigned char *spki_start;
    int spki_len;
    const unsigned char *modulus;
    int mod_len;
    unsigned int exponent;
    int is_ca;
    int has_basic_constraints;
    const unsigned char *san_start;
    int san_len;
    int has_san;
    int key_usage;
    int has_key_usage;
    const unsigned char *eku_oids;
    int eku_oids_len;
    int has_ext_key_usage;
};

/* Sertifika zincirini dogrula.
 * certs: sertifika DER'leri (leaf once, sonra intermediate/root)
 * cert_lens: her sertifikanin uzunlugu
 * n: sertifika sayisi
 * trusted_cas: guvenilir root CA DER'leri
 * trusted_ca_lens: CA uzunluklari
 * n_ca: CA sayisi
 * return: 0 basari, -1 hata
 */
int x509_verify_chain(const unsigned char **certs, const int *cert_lens, int n,
                      const unsigned char **trusted_cas, const int *trusted_ca_lens, int n_ca);

/* Hostname vs SAN dogrulamasi (RFC 6125) */
int x509_verify_hostname(const struct x509_cert *cert, const char *hostname);

/* Key Usage kontrolu */
int x509_check_key_usage(const struct x509_cert *cert, int usage_mask);

/* Extended Key Usage kontrolu */
int x509_has_ext_key_usage(const struct x509_cert *cert,
                           const unsigned char *oid, int oid_len);

#ifdef __cplusplus
}
#endif

#endif /* X509_H */