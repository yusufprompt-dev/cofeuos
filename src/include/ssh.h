#ifndef SSH_H
#define SSH_H

#include "types.h"

#define SSH_PORT            22
#define SSH_BUF_SIZE        4096
#define SSH_MAX_CHANNELS    8
#define SSH_MAX_KNOWN_HOSTS 16
#define SSH_KEY_MAX_LEN     512

typedef struct {
    u32 seq_sent;
    u32 seq_recv;
    int fd;
    int authenticated;
    int channel;
    u8  session_id[32];
    u8  server_key[SSH_KEY_MAX_LEN];
    int server_key_len;
} ssh_session_t;

typedef struct {
    char host[64];
    u8   fingerprint[32];
    int  port;
} ssh_known_host_t;

void ssh_init(void);
int  ssh_connect(const char *host, int port, const char *username);
int  ssh_authenticate_password(ssh_session_t *session, const char *password);
int  ssh_authenticate_key(ssh_session_t *session, const char *key_path);
int  ssh_exec(ssh_session_t *session, const char *command, char *output, int out_max);
int  ssh_disconnect(ssh_session_t *session);
int  ssh_shell(ssh_session_t *session);

int  ssh_known_hosts_add(const char *host, int port, const u8 *fingerprint);
int  ssh_known_hosts_check(const char *host, int port, const u8 *fingerprint);
int  ssh_known_hosts_remove(const char *host, int port);

#endif
