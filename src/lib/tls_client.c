/*
 * CofeuOS TLS - Bilesen 6: TLS 1.2 istemci durum makinesi
 *
 * 4 bileseni bir akista birlestirir: RSA (rsa.c), PRF/HMAC/SHA-256
 * (tls_prf.c), record layer (tls_record.c), handshake mesajlari
 * (tls_handshake.c). Cipher suite: TLS_RSA_WITH_AES_128_CBC_SHA256.
 *
 * IO soyutlamasi: tls_net_t callback'leri ile agdan bagimsizdir.
 * Native test POSIX soket baglar; UEFI tarafi network.c'deki soket
 * katmanini baglar. malloc yok; tum buffer'lar sabit boyutlu.
 *
 * Guvenlik: fatal alert'ler yakalanir (illegal_parameter vb.), buffer
 * limitleri asilirsa nazikce reddedilir (TERR_MSG_TOO_LARGE), ag
 * islemleri net katmaninin timeout'iyla sinirlidir.
 */
#include <stdint.h>
#include "../include/string.h"
#ifdef TLS_DUMP
#include <stdio.h>
#endif
#include "../include/tls.h"
#include "../include/x509.h"

/* dış API (tls_prf/tls_record/tls_handshake/rsa) */
extern int  tls_prf_sha256(const uint8_t *secret, size_t secret_len,
                           const char *label,
                           const uint8_t *seed, size_t seed_len,
                           uint8_t *out, size_t out_len);
extern int  tls12_key_block(const uint8_t master[48],
                            const uint8_t server_random[32],
                            const uint8_t client_random[32],
                            uint8_t *out, size_t out_len);
extern int  tls_record_encrypt(const uint8_t mac_key[32], const uint8_t enc_key[16],
                               const uint8_t explicit_iv[16], uint64_t seq,
                               uint8_t type, const uint8_t *plain, size_t plain_len,
                               uint8_t *out, size_t out_cap, size_t *out_len);
extern int  tls_record_decrypt(const uint8_t mac_key[32], const uint8_t enc_key[16],
                               uint64_t seq, uint8_t expected_type,
                               const uint8_t *in, size_t in_len,
                               uint8_t *out, size_t out_cap, size_t *out_len);
extern int  tls_build_client_hello(const uint8_t random[32],
                                    const uint8_t *session_id, size_t session_id_len,
                                    const char *host,
                                    uint8_t *out, size_t out_cap, size_t *out_len);
extern int  tls_parse_server_hello(const uint8_t *in, size_t in_len,
                                    uint8_t server_random[32],
                                    uint16_t *cipher_suite, uint8_t *compression);
extern int  tls_parse_certificate(const uint8_t *in, size_t in_len,
                                   uint8_t *leaf_der, size_t leaf_cap, size_t *leaf_len);
extern int  tls_parse_server_hello_done(const uint8_t *in, size_t in_len);
extern int  tls_build_client_key_exchange(const uint8_t premaster[48],
                                           const uint8_t *server_modulus, int server_mod_len,
                                           unsigned int server_exponent,
                                           uint8_t *out, size_t out_cap, size_t *out_len);
extern int  tls_master_secret(const uint8_t premaster[48],
                              const uint8_t client_random[32],
                              const uint8_t server_random[32],
                              uint8_t master[48]);
extern int  tls_finished_verify_data(const uint8_t master[48], const char *label,
                                      const sha256_context *hs_hash,
                                      uint8_t verify[12]);
extern void tls_build_change_cipher_spec(uint8_t out[6]);
extern int  x509_extract_rsa_public_key(const unsigned char *cert_der, int cert_len,
                                         unsigned char *out_modulus, int *out_mod_len,
                                         unsigned int *out_exponent);
extern int  x509_verify_chain(const unsigned char **certs, const int *cert_lens, int n,
                              const unsigned char **trusted_cas, const int *trusted_ca_lens, int n_ca);
extern int  trust_store_load_blob(const uint8_t *data, int size);
extern int  trust_store_get_certs(const unsigned char ***out_certs, int **out_lens, int *out_count);
extern int  tls_session_cache_store(tls_client_t *c, const uint8_t *session_id, size_t session_id_len,
                                    const uint8_t master[48], uint32_t time);
int  tls_session_cache_find(tls_client_t *c, const uint8_t *session_id, size_t session_id_len,
                            uint8_t master[48]);
void tls_session_cache_prune(tls_client_t *c, uint32_t current_time);
extern int  time_get_unix(int64_t *out_ts);

void tls_client_init(tls_client_t *c, const tls_net_t *net,
                     const uint8_t client_random[32],
                     const uint8_t premaster_random[46]) {
    memset(c, 0, sizeof *c);
    c->net = *net;
    if (client_random)
        memcpy(c->client_random, client_random, 32);
    else if (net->get_random)
        net->get_random(net->ctx, c->client_random, 32);
    if (premaster_random)
        memcpy(c->premaster_random, premaster_random, 46);
    else if (net->get_random)
        net->get_random(net->ctx, c->premaster_random, 46);
#ifdef TLS_DUMP
    {
        fprintf(stderr, "TLS_DUMP cr=%02x", c->client_random[0]);
        for (int i = 1; i < 32; i++) fprintf(stderr, "%02x", c->client_random[i]);
        fprintf(stderr, " pm=%02x", c->premaster_random[0]);
        for (int i = 1; i < 46; i++) fprintf(stderr, "%02x", c->premaster_random[i]);
        fprintf(stderr, "\n");
    }
#endif
    c->state = TLS_STATE_INIT;
    sha256_init(&c->hs_hash);
}

/* Session cache functions */
int tls_session_cache_store(tls_client_t *c, const uint8_t *session_id, size_t session_id_len,
                            const uint8_t master[48], uint32_t time) {
    if (!c || !session_id || session_id_len == 0 || session_id_len > TLS_SESSION_ID_MAX)
        return TLS_ERR_PROTOCOL;
    if (!master) return TLS_ERR_PROTOCOL;

    int best_slot = -1;
    uint32_t oldest = 0xFFFFFFFF;
    for (int i = 0; i < TLS_SESSION_CACHE; i++) {
        if (!c->session_cache[i].valid) {
            best_slot = i;
            break;
        }
        if (c->session_cache[i].created_time < oldest) {
            oldest = c->session_cache[i].created_time;
            best_slot = i;
        }
    }
    if (best_slot == -1) return TLS_ERR_MSG_TOO_LARGE;

    c->session_cache[best_slot].session_id_len = session_id_len;
    memcpy(c->session_cache[best_slot].session_id, session_id, session_id_len);
    memcpy(c->session_cache[best_slot].master, master, 48);
    c->session_cache[best_slot].created_time = time;
    c->session_cache[best_slot].valid = 1;
    return TLS_OK;
}

int tls_session_cache_find(tls_client_t *c, const uint8_t *session_id, size_t session_id_len,
                           uint8_t master[48]) {
    if (!c || !session_id || session_id_len == 0 || session_id_len > TLS_SESSION_ID_MAX)
        return TLS_ERR_PROTOCOL;
    if (!master) return TLS_ERR_PROTOCOL;

    for (int i = 0; i < TLS_SESSION_CACHE; i++) {
        if (!c->session_cache[i].valid) continue;
        if (c->session_cache[i].session_id_len != session_id_len) continue;
        if (memcmp(c->session_cache[i].session_id, session_id, session_id_len) == 0) {
            extern int time_get_unix(int64_t *out_ts);
            int64_t now;
            if (time_get_unix(&now) >= 0) {
                if ((uint32_t)now - c->session_cache[i].created_time > TLS_SESSION_TIMEOUT) {
                    c->session_cache[i].valid = 0;
                    continue;
                }
            }
            memcpy(master, c->session_cache[i].master, 48);
            return TLS_OK;
        }
    }
    return TLS_ERR_PROTOCOL;
}

void tls_session_cache_prune(tls_client_t *c, uint32_t current_time) {
    for (int i = 0; i < TLS_SESSION_CACHE; i++) {
        if (c->session_cache[i].valid &&
            current_time - c->session_cache[i].created_time > TLS_SESSION_TIMEOUT) {
            c->session_cache[i].valid = 0;
        }
    }
}

/* ---- yapimci yardimcilar ---- */

static int put3(uint8_t *b, size_t v) {
    b[0] = (uint8_t)((v >> 16) & 0xff);
    b[1] = (uint8_t)((v >> 8) & 0xff);
    b[2] = (uint8_t)(v & 0xff);
    return 0;
}

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

static int net_send_record(tls_client_t *c, uint8_t type,
                           const uint8_t *payload, size_t payload_len,
                           int encrypted) {
    uint8_t out[5 + TLS_REC_MAX_PAYLOAD];
    size_t olen;
    int rc;

    if (payload_len > 16384) return TLS_ERR_MSG_TOO_LARGE;

    out[0] = type;
    out[1] = 3; out[2] = 3;

    if (encrypted) {
        uint8_t iv[16];
        c->net.get_random(c->net.ctx, iv, 16);
        rc = tls_record_encrypt(c->client_mac_key, c->client_enc_key, iv,
                                c->write_seq, type, payload, payload_len,
                                out + 5, sizeof(out) - 5, &olen);
        if (rc != 0) return TLS_ERR_CRYPTO;
        out[3] = (uint8_t)((olen >> 8) & 0xff);
        out[4] = (uint8_t)(olen & 0xff);
        if (c->net.send(c->net.ctx, out, olen + 5) != 0) return TLS_ERR_IO;
        c->write_seq++;
        return TLS_OK;
    }

    out[3] = (uint8_t)((payload_len >> 8) & 0xff);
    out[4] = (uint8_t)(payload_len & 0xff);
    memcpy(out + 5, payload, payload_len);
    if (c->net.send(c->net.ctx, out, payload_len + 5) != 0) return TLS_ERR_IO;
    c->write_seq++;
    return TLS_OK;
}

static int net_recv_record(tls_client_t *c, int encrypted,
                           uint8_t *recv_type, size_t *plen) {
    uint8_t hdr[5];
    size_t len;
    int rc;

    if (c->net.recv(c->net.ctx, hdr, 5) != 0) return TLS_ERR_IO;
    uint8_t type = hdr[0];
    if (hdr[1] != 3 || hdr[2] != 3) return TLS_ERR_PROTOCOL;
    len = ((size_t)hdr[3] << 8) | hdr[4];
    if (len > TLS_REC_MAX_PAYLOAD) return TLS_ERR_MSG_TOO_LARGE;
    if (len == 0) return TLS_ERR_PROTOCOL;

    if (c->net.recv(c->net.ctx, c->rec_buf, len) != 0) return TLS_ERR_IO;

    if (encrypted) {
        uint8_t out[TLS_REC_MAX_PAYLOAD];
        size_t olen;
        rc = tls_record_decrypt(c->server_mac_key, c->server_enc_key,
                                c->read_seq, type, c->rec_buf, len,
                                out, sizeof(out), &olen);
        if (rc != 0) return TLS_ERR_CRYPTO;
        c->read_seq++;
        memcpy(c->rec_buf, out, olen);
        *plen = olen;
    } else {
        c->read_seq++;
        *plen = len;
    }
    *recv_type = type;
    return TLS_OK;
}

static int handle_alert(tls_client_t *c, const uint8_t *payload, size_t len) {
    if (len < 2) return TLS_ERR_PROTOCOL;
    c->last_alert_level = payload[0];
    c->last_alert_desc  = payload[1];
    if (payload[0] == 2) return TLS_ERR_ALERT_FATAL;
    return TLS_OK;
}

static int hs_recv(tls_client_t *c, uint8_t expected_type,
                   uint8_t *out, size_t out_cap, size_t *out_len) {
    for (;;) {
        while (c->hs_off + 4 <= c->hs_len) {
            const uint8_t *m = c->hs_buf + c->hs_off;
            size_t mlen = 4 + ((size_t)m[1] << 16) + ((size_t)m[2] << 8) + m[3];
            if (mlen > TLS_MAX_HS_MSG) return TLS_ERR_MSG_TOO_LARGE;
            if (c->hs_off + mlen > c->hs_len) break;
            if (m[0] == expected_type) {
                if (mlen > out_cap) return TLS_ERR_MSG_TOO_LARGE;
                memcpy(out, m, mlen);
                *out_len = mlen;
                c->hs_off += mlen;
                if (c->hs_off == c->hs_len) { c->hs_off = c->hs_len = 0; }
                return TLS_OK;
            }
            c->hs_off += mlen;
            if (c->hs_off == c->hs_len) { c->hs_off = c->hs_len = 0; }
        }

        if (c->hs_off && c->hs_off == c->hs_len) { c->hs_off = c->hs_len = 0; }
        if (c->hs_off > 0) {
            memmove(c->hs_buf, c->hs_buf + c->hs_off, c->hs_len - c->hs_off);
            c->hs_len -= c->hs_off;
            c->hs_off = 0;
        }

        uint8_t type; size_t plen;
        int rc = net_recv_record(c, 0, &type, &plen);
        if (rc != TLS_OK) return rc;

        if (type == CT_ALERT) {
            rc = handle_alert(c, c->rec_buf, plen);
            if (rc != TLS_OK) return rc;
            continue;
        }
        if (type == CT_CHANGE_CIPHER_SPEC) {
            return TLS_ERR_PROTOCOL;
        }
        if (type == CT_APPLICATION_DATA) {
            return TLS_ERR_PROTOCOL;
        }
        if (type != CT_HANDSHAKE) return TLS_ERR_PROTOCOL;

        if (c->hs_len + plen > TLS_MAX_HS_MSG) return TLS_ERR_MSG_TOO_LARGE;
        memcpy(c->hs_buf + c->hs_len, c->rec_buf, plen);
        c->hs_len += plen;
    }
}

static int setup_keys(tls_client_t *c) {
    uint8_t kb[104];
    if (tls12_key_block(c->master, c->server_random, c->client_random, kb, 104) != 0)
        return TLS_ERR_CRYPTO;
    memcpy(c->client_mac_key, kb, 32);
    memcpy(c->server_mac_key, kb + 32, 32);
    memcpy(c->client_enc_key, kb + 64, 16);
    memcpy(c->server_enc_key, kb + 80, 16);
    return TLS_OK;
}

int tls_client_handshake(tls_client_t *c) {
    uint8_t msg[TLS_MAX_HS_MSG];
    size_t mlen;
    int rc;
    int resumption = 0;

    if (c->state == TLS_STATE_CONNECTED) return TLS_OK;

    /* Check session cache for resumption before starting handshake */
    uint8_t cached_master[48];
    int has_cached_session = 0;
    if (c->session_id_len > 0 && c->session_id_len <= TLS_SESSION_ID_MAX) {
        if (tls_session_cache_find(c, c->session_id, c->session_id_len, c->master) == TLS_OK) {
            has_cached_session = 1;
        }
    }

    rc = tls_build_client_hello(c->client_random, c->session_id, c->session_id_len,
                                c->sni_host, msg, sizeof msg, &mlen);
    if (rc != 0) return c->error = TLS_ERR_PROTOCOL;
    sha256_update(&c->hs_hash, msg, mlen);
    rc = net_send_record(c, CT_HANDSHAKE, msg, mlen, 0);
    if (rc != TLS_OK) return c->error = rc;
    c->step = 1;

    uint8_t srand[32]; uint16_t cs = 0; uint8_t cm = 0xff;
    rc = hs_recv(c, 2, msg, sizeof msg, &mlen);
    if (rc != TLS_OK) return c->error = rc;
    rc = tls_parse_server_hello(msg, mlen, srand, &cs, &cm);
    if (rc != 0) return c->error = TLS_ERR_PROTOCOL;
    memcpy(c->server_random, srand, 32);
    sha256_update(&c->hs_hash, msg, mlen);
    c->step = 2;

    uint8_t server_session_id[TLS_SESSION_ID_MAX];
    size_t server_session_id_len = 0;
    {
        const uint8_t *p = msg + 4;
        if (mlen >= 38) {
            p += 2;
            p += 32;
            if (p < msg + mlen) {
                uint8_t sid_len = *p++;
                if (sid_len > 0 && sid_len <= TLS_SESSION_ID_MAX && p + sid_len <= msg + mlen) {
                    memcpy(server_session_id, p, sid_len);
                    server_session_id_len = sid_len;
                }
            }
        }
    }

    if (has_cached_session &&
        server_session_id_len == c->session_id_len &&
        memcmp(server_session_id, c->session_id, c->session_id_len) == 0) {
        resumption = 1;
    }

    /* Certificate chain -> parse full chain -> verify against trusted CAs */
    if (!resumption) {
        unsigned char mod[256];
        int mod_len;
        unsigned int exp;

        rc = hs_recv(c, 11, msg, sizeof msg, &mlen);
        if (rc != TLS_OK) return c->error = rc;
        sha256_update(&c->hs_hash, msg, mlen);

        const unsigned char *p = msg + 4;
        const unsigned char *end = msg + mlen;
        if (end - p < 3) return c->error = TLS_ERR_PROTOCOL;
        size_t list_len = ((size_t)p[0] << 16) | ((size_t)p[1] << 8) | p[2];
        p += 3;
        if ((size_t)(end - p) != list_len) return c->error = TLS_ERR_PROTOCOL;

        const unsigned char *certs_data[16];
        int certs_data_lens[16];
        int n = 0;
        while (p < end && n < 16) {
            if (end - p < 3) return c->error = TLS_ERR_PROTOCOL;
            size_t cl = ((size_t)p[0] << 16) | ((size_t)p[1] << 8) | p[2];
            p += 3;
            if ((size_t)(end - p) < cl) return c->error = TLS_ERR_PROTOCOL;
            certs_data[n] = p;
            certs_data_lens[n] = (int)cl;
            n++;
            p += cl;
        }
        if (n == 0) return c->error = TLS_ERR_PROTOCOL;

        rc = x509_extract_rsa_public_key(certs_data[0], certs_data_lens[0],
                                         mod, &mod_len, &exp);
        if (rc != 0) return c->error = TLS_ERR_CRYPTO;

        const unsigned char **ca_certs = NULL;
        int *ca_lens = NULL;
        int n_ca = 0;
        rc = trust_store_get_certs(&ca_certs, &ca_lens, &n_ca);
        if (rc == 0 && n_ca > 0) {
            rc = x509_verify_chain(certs_data, certs_data_lens, n, ca_certs, ca_lens, n_ca);
            if (rc != 0) return c->error = TLS_ERR_CRYPTO;
        } else {
        }
        c->peer_cert_hash_valid = 1;
        sha256_hash(certs_data[0], certs_data_lens[0], c->peer_cert_hash);

        c->step = 3;

        rc = hs_recv(c, 14, msg, sizeof msg, &mlen);
        if (rc != TLS_OK) return c->error = rc;
        rc = tls_parse_server_hello_done(msg, mlen);
        if (rc != 0) return c->error = TLS_ERR_PROTOCOL;
        sha256_update(&c->hs_hash, msg, mlen);
        c->step = 4;

        if (resumption) {
            rc = setup_keys(c);
            if (rc != 0) return c->error = TLS_ERR_CRYPTO;
        } else {
            c->premaster[0] = 3; c->premaster[1] = 3;
            memcpy(c->premaster + 2, c->premaster_random, 46);
            rc = tls_master_secret(c->premaster, c->client_random, c->server_random,
                                   c->master);
            if (rc != 0) return c->error = TLS_ERR_CRYPTO;
            rc = setup_keys(c);
            if (rc != 0) return c->error = TLS_ERR_CRYPTO;

            rc = tls_build_client_key_exchange(c->premaster, mod, mod_len, exp,
                                               msg, sizeof msg, &mlen);
            if (rc != 0) return c->error = TLS_ERR_CRYPTO;
            sha256_update(&c->hs_hash, msg, mlen);
            rc = net_send_record(c, CT_HANDSHAKE, msg, mlen, 0);
            if (rc != TLS_OK) return c->error = rc;
            c->step = 5;
        }
    }

    uint8_t ccs[6];
    tls_build_change_cipher_spec(ccs);
    rc = net_send_record(c, CT_CHANGE_CIPHER_SPEC, ccs + 5, 1, 0);
    if (rc != TLS_OK) return c->error = rc;
    c->write_seq = 0;
    c->step = 6;

    uint8_t verify[12];
    rc = tls_finished_verify_data(c->master, "client finished", &c->hs_hash,
                                  verify);
    if (rc != 0) return c->error = TLS_ERR_CRYPTO;
    uint8_t fin[4 + 12];
    fin[0] = 20; put3(fin + 1, 12);
    memcpy(fin + 4, verify, 12);
    sha256_update(&c->hs_hash, fin, 16);
    rc = net_send_record(c, CT_HANDSHAKE, fin, 16, 1);
    if (rc != TLS_OK) return c->error = rc;
    c->step = 7;

    uint8_t type; size_t plen;
    rc = net_recv_record(c, 0, &type, &plen);
    if (rc != TLS_OK) return c->error = rc;
    if (type != CT_CHANGE_CIPHER_SPEC || plen != 1 || c->rec_buf[0] != 1)
        return c->error = TLS_ERR_PROTOCOL;
    c->read_seq = 0;
    c->step = 8;

    for (;;) {
        rc = net_recv_record(c, 1, &type, &plen);
        if (rc != TLS_OK) return c->error = rc;
        if (type == CT_ALERT) {
            rc = handle_alert(c, c->rec_buf, plen);
            if (rc != TLS_OK) return rc;
            continue;
        }
        if (type == CT_HANDSHAKE) break;
        return c->error = TLS_ERR_PROTOCOL;
    }
    if (plen != 16 || c->rec_buf[0] != 20) return c->error = TLS_ERR_PROTOCOL;
    rc = tls_finished_verify_data(c->master, "server finished", &c->hs_hash,
                                  verify);
    if (rc != 0) return c->error = TLS_ERR_CRYPTO;
    if (memcmp(c->rec_buf + 4, verify, 12) != 0) return c->error = TLS_ERR_CRYPTO;
    c->step = 9;

    if (!resumption && c->session_id_len > 0) {
        if (time_get_unix((int64_t *)&c->session_cache[0].created_time) >= 0) {
            tls_session_cache_store(c, c->session_id, c->session_id_len, c->master, (uint32_t)c->session_cache[0].created_time);
        }
    }

    c->state = TLS_STATE_CONNECTED;
    return TLS_OK;
}

/* ---- application data ---- */

int tls_client_write(tls_client_t *c, const uint8_t *data, size_t len) {
    if (c->state != TLS_STATE_CONNECTED) return TLS_ERR_PROTOCOL;
    int rc = net_send_record(c, CT_APPLICATION_DATA, data, len, 1);
    return rc;
}

int tls_client_read(tls_client_t *c, uint8_t *out, size_t cap, size_t *out_len) {
    if (c->state != TLS_STATE_CONNECTED) return TLS_ERR_PROTOCOL;
    uint8_t type; size_t plen;
    for (;;) {
        int rc = net_recv_record(c, 1, &type, &plen);
        if (rc != TLS_OK) return rc;
        if (type == CT_ALERT) {
            rc = handle_alert(c, c->rec_buf, plen);
            if (rc != TLS_OK) return rc;
            if (c->rec_buf[0] == 1 && c->rec_buf[1] == 0) {
                if (out_len) *out_len = 0;
                return TLS_OK;
            }
            continue;
        }
        if (type == CT_APPLICATION_DATA) {
            if (plen > cap) return TLS_ERR_MSG_TOO_LARGE;
            memcpy(out, c->rec_buf, plen);
            *out_len = plen;
            return TLS_OK;
        }
        return TLS_ERR_PROTOCOL;
    }
}

const char *tls_err_str(int err) {
    switch (err) {
        case TLS_OK: return "OK";
        case TLS_ERR_IO: return "IO/timeout";
        case TLS_ERR_PROTOCOL: return "protocol";
        case TLS_ERR_ALERT_FATAL: return "fatal alert";
        case TLS_ERR_CRYPTO: return "crypto";
        case TLS_ERR_MSG_TOO_LARGE: return "msg too large";
        default: return "unknown";
    }
}