#include "../include/ssh.h"
#include "../include/string.h"
#include "../include/network.h"

static ssh_known_host_t g_known_hosts[SSH_MAX_KNOWN_HOSTS];
static int g_known_count = 0;

static void mem_copy(u8 *dst, const u8 *src, int len) {
    for (int i = 0; i < len; i++) dst[i] = src[i];
}
static void mem_zero(u8 *buf, int len) {
    for (int i = 0; i < len; i++) buf[i] = 0;
}
static int str_len(const char *s) { int n = 0; while (s[n]) n++; return n; }
static void str_copy(char *dst, const char *src, int max) {
    int i = 0; while (src[i] && i < max - 1) { dst[i] = src[i]; i++; } dst[i] = '\0';
}

void ssh_init(void) {
    g_known_count = 0;
}

static void generate_session_id(u8 *out, int len) {
    static u32 seed = 0xDEADBEEF;
    for (int i = 0; i < len; i += 4) {
        seed ^= (seed << 13) ^ (seed >> 17) ^ (seed << 5);
        out[i] = (u8)(seed >> 24);
        if (i+1 < len) out[i+1] = (u8)(seed >> 16);
        if (i+2 < len) out[i+2] = (u8)(seed >> 8);
        if (i+3 < len) out[i+3] = (u8)(seed);
    }
}

int ssh_connect(const char *host, int port, const char *username) {
    if (!host || !host[0]) return -1;
    if (port <= 0) port = SSH_PORT;
    unsigned char ip[4] = {0};
    if (dns_resolve(host, ip) < 0) return -2;
    if (tcp_connect(ip, (u16)port) < 0) return -3;
    ssh_session_t *s = (void*)0;
    static ssh_session_t default_session;
    s = &default_session;
    mem_zero((u8*)s, sizeof(ssh_session_t));
    s->fd = 1;
    generate_session_id(s->session_id, 32);
    return 0;
}

int ssh_authenticate_password(ssh_session_t *session, const char *password) {
    if (!session || !password) return -1;
    session->authenticated = 1;
    return 0;
}

int ssh_authenticate_key(ssh_session_t *session, const char *key_path) {
    if (!session || !key_path) return -1;
    session->authenticated = 1;
    return 0;
}

int ssh_exec(ssh_session_t *session, const char *command, char *output, int out_max) {
    if (!session || !command || !output || out_max <= 0) return -1;
    if (!session->authenticated) return -2;
    str_copy(output, "SSH: komut calistirildi (simulasyon)", out_max);
    return 0;
}

int ssh_disconnect(ssh_session_t *session) {
    if (!session) return -1;
    if (session->fd > 0) {
        unsigned char ip[4] = {0};
        tcp_disconnect(ip, SSH_PORT);
    }
    mem_zero((u8*)session, sizeof(ssh_session_t));
    return 0;
}

int ssh_shell(ssh_session_t *session) {
    if (!session || !session->authenticated) return -1;
    return 0;
}

int ssh_known_hosts_add(const char *host, int port, const u8 *fingerprint) {
    if (!host || !fingerprint || g_known_count >= SSH_MAX_KNOWN_HOSTS) return -1;
    ssh_known_host_t *e = &g_known_hosts[g_known_count];
    str_copy(e->host, host, sizeof(e->host));
    e->port = port;
    mem_copy(e->fingerprint, fingerprint, 32);
    g_known_count++;
    return 0;
}

int ssh_known_hosts_check(const char *host, int port, const u8 *fingerprint) {
    if (!host || !fingerprint) return -1;
    for (int i = 0; i < g_known_count; i++) {
        int hlen = str_len(host);
        int elen = str_len(g_known_hosts[i].host);
        if (hlen == elen && g_known_hosts[i].port == port) {
            int match = 1;
            for (int j = 0; j < hlen; j++) {
                if (g_known_hosts[i].host[j] != host[j]) { match = 0; break; }
            }
            if (match) {
                int fmatch = 1;
                for (int j = 0; j < 32; j++) {
                    if (g_known_hosts[i].fingerprint[j] != fingerprint[j]) { fmatch = 0; break; }
                }
                return fmatch ? 0 : -2;
            }
        }
    }
    return -3;
}

int ssh_known_hosts_remove(const char *host, int port) {
    if (!host) return -1;
    for (int i = 0; i < g_known_count; i++) {
        int hlen = str_len(host);
        int elen = str_len(g_known_hosts[i].host);
        if (hlen == elen && g_known_hosts[i].port == port) {
            int match = 1;
            for (int j = 0; j < hlen; j++) {
                if (g_known_hosts[i].host[j] != host[j]) { match = 0; break; }
            }
            if (match) {
                g_known_hosts[i] = g_known_hosts[g_known_count - 1];
                g_known_count--;
                return 0;
            }
        }
    }
    return -1;
}
