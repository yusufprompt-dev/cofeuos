#ifndef MAIL_H
#define MAIL_H

#include "types.h"

#define MAIL_MSG_MAX     512
#define MAIL_ADDR_MAX    128
#define MAIL_SUBJECT_MAX 128
#define MAIL_INBOX_MAX   32
#define MAIL_OUTBOX_MAX  16

typedef struct {
    char from[MAIL_ADDR_MAX];
    char to[MAIL_ADDR_MAX];
    char subject[MAIL_SUBJECT_MAX];
    char body[MAIL_MSG_MAX];
    u64  timestamp;
    u8   read;
    u8   type;
} mail_message_t;

#define MAIL_TYPE_INCOMING  0
#define MAIL_TYPE_OUTGOING  1

typedef struct {
    char smtp_host[64];
    char pop3_host[64];
    char username[64];
    char password[64];
    u16  smtp_port;
    u16  pop3_port;
    u8   use_tls;
    u8   configured;
} mail_account_t;

void mail_init(void);
int  mail_configure(const char *smtp, u16 smtp_port, const char *pop3, u16 pop3_port,
                    const char *user, const char *pass);
int  mail_send(const char *to, const char *subject, const char *body);
int  mail_receive(mail_message_t *messages, int max_count);
int  mail_get_inbox(mail_message_t *messages, int max_count);
int  mail_get_outbox(mail_message_t *messages, int max_count);
int  mail_read(int index, mail_message_t *out);
int  mail_delete(int index);
int  mail_is_configured(void);

#endif
