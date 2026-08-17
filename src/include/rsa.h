/*
 * ============================================================================
 * RSA.H - CofeuOS RSA (PKCS#1 v1.5) API
 * ============================================================================
 */

#ifndef RSA_H
#define RSA_H

#include <stdint.h>

/* DER X.509 sertifikasindan RSA public key cikarir. 0: basarili */
int x509_extract_rsa_public_key(const unsigned char *cert_der, int cert_len,
                                unsigned char *out_modulus, int *out_mod_len,
                                unsigned int *out_exponent);

/* PKCS#1 v1.5 sifreleme (RSA key exchange pre-master). */
void rsa_pkcs1_encrypt(const unsigned char *plaintext, int plain_len,
                       const unsigned char *modulus, int mod_len,
                       unsigned int exponent, unsigned char *out_ciphertext);

/* PKCS#1 v1.5 imza dogrulama (SHA-256 DigestInfo). 0: dogru; -1: gecersiz */
int rsa_pkcs1_verify(const unsigned char *sig, int sig_len,
                     const unsigned char *modulus, int mod_len,
                     unsigned int exponent,
                     const unsigned char *digest, int digest_len);

#endif /* RSA_H */
