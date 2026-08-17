#ifndef DHCP_H
#define DHCP_H

#include "types.h"

#define DHCP_STATE_INIT       0
#define DHCP_STATE_SELECTING  1
#define DHCP_STATE_REQUESTING 2
#define DHCP_STATE_BOUND      3
#define DHCP_STATE_RENEWING   4
#define DHCP_STATE_REBINDING  5

typedef struct {
    u8  state;
    u8  ip[4];
    u8  subnet[4];
    u8  gateway[4];
    u8  dns[4];
    u32 lease_time;
    u32 renew_time;
    u8  mac[6];
    u32 xid;
    int valid;
} dhcp_client_t;

void dhcp_client_init(void);
int  dhcp_client_discover(void);
int  dhcp_client_request(u8 *server_ip);
int  dhcp_client_renew(void);
int  dhcp_client_release(void);
int  dhcp_client_get_status(dhcp_client_t *out);
int  dhcp_client_is_bound(void);

#endif
