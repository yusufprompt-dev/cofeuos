#include "../include/mail.h"
#include "../include/string.h"

static mail_account_t g_account;
static mail_message_t g_inbox[MAIL_INBOX_MAX];
static int g_inbox_count = 0;
static mail_message_t g_outbox[MAIL_OUTBOX_MAX];
static int g_outbox_count = 0;

static void str_copy(char *dst, const char *src, int max) {
    int i = 0; while (src[i] && i < max - 1) { dst[i] = src[i]; i++; } dst[i] = '\0';
}

void mail_init(void) {
    g_account.configured = 0;
    g_inbox_count = 0;
    g_outbox_count = 0;
}

int mail_configure(const char *smtp, u16 smtp_port, const char *pop3, u16 pop3_port,
                   const char *user, const char *pass) {
    if (!smtp || !pop3 || !user || !pass) return -1;
    str_copy(g_account.smtp_host, smtp, 64);
    str_copy(g_account.pop3_host, pop3, 64);
    str_copy(g_account.username, user, 64);
    str_copy(g_account.password, pass, 64);
    g_account.smtp_port = smtp_port;
    g_account.pop3_port = pop3_port;
    g_account.use_tls = 1;
    g_account.configured = 1;
    return 0;
}

int mail_send(const char *to, const char *subject, const char *body) {
    if (!to || !subject || !body || g_outbox_count >= MAIL_OUTBOX_MAX) return -1;
    if (!g_account.configured) return -2;
    mail_message_t *msg = &g_outbox[g_outbox_count];
    str_copy(msg->from, g_account.username, MAIL_ADDR_MAX);
    str_copy(msg->to, to, MAIL_ADDR_MAX);
    str_copy(msg->subject, subject, MAIL_SUBJECT_MAX);
    str_copy(msg->body, body, MAIL_MSG_MAX);
    msg->type = MAIL_TYPE_OUTGOING;
    msg->read = 0;
    g_outbox_count++;
    return 0;
}

int mail_receive(mail_message_t *messages, int max_count) {
    if (!messages || max_count <= 0) return 0;
    int c = (g_inbox_count < max_count) ? g_inbox_count : max_count;
    for (int i = 0; i < c; i++) messages[i] = g_inbox[i];
    return c;
}

int mail_get_inbox(mail_message_t *messages, int max_count) {
    return mail_receive(messages, max_count);
}

int mail_get_outbox(mail_message_t *messages, int max_count) {
    if (!messages || max_count <= 0) return 0;
    int c = (g_outbox_count < max_count) ? g_outbox_count : max_count;
    for (int i = 0; i < c; i++) messages[i] = g_outbox[i];
    return c;
}

int mail_read(int index, mail_message_t *out) {
    if (index < 0 || index >= g_inbox_count || !out) return -1;
    *out = g_inbox[index];
    g_inbox[index].read = 1;
    return 0;
}

int mail_delete(int index) {
    if (index < 0 || index >= g_inbox_count) return -1;
    g_inbox[index] = g_inbox[g_inbox_count - 1];
    g_inbox_count--;
    return 0;
}

int mail_is_configured(void) { return g_account.configured; }
