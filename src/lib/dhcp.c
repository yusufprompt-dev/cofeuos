#include "../include/dhcp.h"
#include "../include/string.h"
#include "../include/network.h"

static dhcp_client_t g_dhcp;

static void mem_copy(u8 *dst, const u8 *src, int len) {
    for (int i = 0; i < len; i++) dst[i] = src[i];
}
static void mem_zero(u8 *buf, int len) {
    for (int i = 0; i < len; i++) buf[i] = 0;
}

void dhcp_client_init(void) {
    mem_zero((u8*)&g_dhcp, sizeof(dhcp_client_t));
    g_dhcp.state = DHCP_STATE_INIT;
    g_dhcp.xid = 0xDEADBEEF;
    network_get_mac(g_dhcp.mac);
}

static void build_discover(u8 *buf, int *len_out) {
    mem_zero(buf, 548);
    buf[0] = 0x01;
    buf[1] = 0x06;
    buf[2] = 0x00;
    buf[3] = 0x00;
    buf[4] = (u8)(g_dhcp.xid >> 24);
    buf[5] = (u8)(g_dhcp.xid >> 16);
    buf[6] = (u8)(g_dhcp.xid >> 8);
    buf[7] = (u8)(g_dhcp.xid);
    buf[10] = 0x80; buf[11] = 0x00;
    buf[12] = g_dhcp.mac[0]; buf[13] = g_dhcp.mac[1];
    buf[14] = g_dhcp.mac[2]; buf[15] = g_dhcp.mac[3];
    buf[16] = g_dhcp.mac[4]; buf[17] = g_dhcp.mac[5];
    int pos = 236;
    buf[pos++] = 0x63; buf[pos++] = 0x82;
    buf[pos++] = 0x53; buf[pos++] = 0x63;
    buf[pos++] = 0x35; buf[pos++] = 0x01;
    buf[pos++] = 0x01;
    buf[pos++] = 0x37; buf[pos++] = 0x04;
    buf[pos++] = 0x01; buf[pos++] = 0x03;
    buf[pos++] = 0x06; buf[pos++] = 0x0F;
    buf[pos++] = 0xFF;
    *len_out = pos;
}

static int parse_offer(const u8 *buf, int len) {
    if (len < 240) return -1;
    u8 msg_type = 0;
    for (int i = 240; i < len - 2; i++) {
        if (buf[i] == 0x35 && i + 1 < len) { msg_type = buf[i + 2]; break; }
        if (buf[i] == 0xFF) break;
        if (buf[i] == 0x00) break;
        i += buf[i + 1] + 1;
    }
    if (msg_type != 0x02) return -2;
    g_dhcp.ip[0] = buf[16]; g_dhcp.ip[1] = buf[17];
    g_dhcp.ip[2] = buf[18]; g_dhcp.ip[3] = buf[19];
    for (int i = 240; i < len - 4; i++) {
        if (buf[i] == 0x01 && buf[i + 1] == 0x04) {
            g_dhcp.subnet[0] = buf[i+2]; g_dhcp.subnet[1] = buf[i+3];
            g_dhcp.subnet[2] = buf[i+4]; g_dhcp.subnet[3] = buf[i+5];
        }
        if (buf[i] == 0x03 && buf[i + 1] == 0x04) {
            g_dhcp.gateway[0] = buf[i+2]; g_dhcp.gateway[1] = buf[i+3];
            g_dhcp.gateway[2] = buf[i+4]; g_dhcp.gateway[3] = buf[i+5];
        }
        if (buf[i] == 0x06 && buf[i + 1] == 0x04) {
            g_dhcp.dns[0] = buf[i+2]; g_dhcp.dns[1] = buf[i+3];
            g_dhcp.dns[2] = buf[i+4]; g_dhcp.dns[3] = buf[i+5];
        }
        if (buf[i] == 0x33 && buf[i + 1] == 0x04) {
            g_dhcp.lease_time = ((u32)buf[i+2] << 24) | ((u32)buf[i+3] << 16) |
                                ((u32)buf[i+4] << 8) | buf[i+5];
        }
        if (buf[i] == 0xFF) break;
        if (buf[i] == 0x00) break;
        i += buf[i + 1] + 1;
    }
    return 0;
}

static void build_request(u8 *buf, int *len_out, const u8 *server_ip) {
    mem_zero(buf, 548);
    buf[0] = 0x01; buf[1] = 0x06; buf[2] = 0x00; buf[3] = 0x00;
    buf[4] = (u8)(g_dhcp.xid >> 24); buf[5] = (u8)(g_dhcp.xid >> 16);
    buf[6] = (u8)(g_dhcp.xid >> 8);  buf[7] = (u8)(g_dhcp.xid);
    buf[10] = 0x80; buf[11] = 0x00;
    buf[12] = g_dhcp.mac[0]; buf[13] = g_dhcp.mac[1];
    buf[14] = g_dhcp.mac[2]; buf[15] = g_dhcp.mac[3];
    buf[16] = g_dhcp.mac[4]; buf[17] = g_dhcp.mac[5];
    buf[19] = g_dhcp.ip[0]; buf[20] = g_dhcp.ip[1];
    buf[21] = g_dhcp.ip[2]; buf[22] = g_dhcp.ip[3];
    int pos = 236;
    buf[pos++] = 0x63; buf[pos++] = 0x82;
    buf[pos++] = 0x53; buf[pos++] = 0x63;
    buf[pos++] = 0x35; buf[pos++] = 0x01;
    buf[pos++] = 0x03;
    buf[pos++] = 0x36; buf[pos++] = 0x04;
    buf[pos++] = server_ip[0]; buf[pos++] = server_ip[1];
    buf[pos++] = server_ip[2]; buf[pos++] = server_ip[3];
    buf[pos++] = 0x32; buf[pos++] = 0x04;
    buf[pos++] = g_dhcp.ip[0]; buf[pos++] = g_dhcp.ip[1];
    buf[pos++] = g_dhcp.ip[2]; buf[pos++] = g_dhcp.ip[3];
    buf[pos++] = 0x37; buf[pos++] = 0x04;
    buf[pos++] = 0x01; buf[pos++] = 0x03;
    buf[pos++] = 0x06; buf[pos++] = 0x0F;
    buf[pos++] = 0xFF;
    *len_out = pos;
}

int dhcp_client_discover(void) {
    g_dhcp.state = DHCP_STATE_SELECTING;
    g_dhcp.xid++;
    u8 buf[548];
    int len = 0;
    build_discover(buf, &len);
    unsigned char bcast[4] = {255, 255, 255, 255};
    tcp_connect(bcast, 67);
    tcp_disconnect(bcast, 67);
    g_dhcp.state = DHCP_STATE_REQUESTING;
    return 0;
}

int dhcp_client_request(u8 *server_ip) {
    if (!server_ip) return -1;
    g_dhcp.state = DHCP_STATE_REQUESTING;
    g_dhcp.xid++;
    u8 buf[548];
    int len = 0;
    build_request(buf, &len, server_ip);
    tcp_connect(server_ip, 67);
    tcp_disconnect(server_ip, 67);
    return 0;
}

int dhcp_client_renew(void) {
    if (g_dhcp.state != DHCP_STATE_BOUND) return -1;
    g_dhcp.state = DHCP_STATE_RENEWING;
    return dhcp_client_request(g_dhcp.gateway);
}

int dhcp_client_release(void) {
    mem_zero((u8*)&g_dhcp.ip, 4);
    mem_zero((u8*)&g_dhcp.gateway, 4);
    mem_zero((u8*)&g_dhcp.dns, 4);
    g_dhcp.state = DHCP_STATE_INIT;
    g_dhcp.valid = 0;
    return 0;
}

int dhcp_client_get_status(dhcp_client_t *out) {
    if (!out) return -1;
    *out = g_dhcp;
    return 0;
}

int dhcp_client_is_bound(void) {
    return g_dhcp.state == DHCP_STATE_BOUND;
}
