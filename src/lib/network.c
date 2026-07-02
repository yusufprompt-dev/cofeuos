/*
 * NETWORK.C - UEFI Simple Network Protocol
 */
#include "../include/network.h"
#include <efi.h>
#include <efilib.h>
#include <efinet.h>

static EFI_SIMPLE_NETWORK_PROTOCOL *g_net = (void*)0;
static EFI_SYSTEM_TABLE *g_st = (void*)0;

void network_init(void *st) {
    g_st = (EFI_SYSTEM_TABLE*)st;
    
    EFI_GUID net_guid = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
    EFI_STATUS status = uefi_call_wrapper(
        g_st->BootServices->LocateProtocol, 3,
        &net_guid, NULL, (VOID**)&g_net
    );
    
    if (EFI_ERROR(status)) {
        g_net = (void*)0;
        return;
    }
    
    /* Network'ü başlat */
    uefi_call_wrapper(g_net->Start, 1, g_net);
    uefi_call_wrapper(g_net->Initialize, 3, g_net, 0, 0);
}

int network_available(void) {
    return g_net != (void*)0;
}

/* MAC adresini al */
void network_get_mac(unsigned char *mac) {
    if (!g_net) return;
    EFI_SIMPLE_NETWORK_MODE *mode = g_net->Mode;
    for (int i = 0; i < 6; i++) {
        mac[i] = mode->CurrentAddress.Addr[i];
    }
}

/* Basit DHCP Discover */
static unsigned char g_ip[4] = {0,0,0,0};
static unsigned char g_gateway[4] = {0,0,0,0};

/* DHCP için EFI UDP4 protokolü lazım */
/* Şimdilik statik IP kullanalım */
void network_set_ip(unsigned char a, unsigned char b, unsigned char c, unsigned char d) {
    g_ip[0] = a; g_ip[1] = b; g_ip[2] = c; g_ip[3] = d;
}

void network_get_ip(unsigned char *ip) {
    ip[0] = g_ip[0]; ip[1] = g_ip[1];
    ip[2] = g_ip[2]; ip[3] = g_ip[3];
}

int network_has_ip(void) {
    return g_ip[0] != 0;
}
void network_dhcp(void) {
    if (!g_st || !g_net) return;
    /* QEMU user network default IP */
    g_ip[0] = 10; g_ip[1] = 0; g_ip[2] = 2; g_ip[3] = 15;
    g_gateway[0] = 10; g_gateway[1] = 0; g_gateway[2] = 2; g_gateway[3] = 2;
}typedef struct {
    unsigned char  type;
    unsigned char  code;
    unsigned short checksum;
    unsigned short id;
    unsigned short seq;
    unsigned char  data[32];
} icmp_t;

typedef struct {
    unsigned char  ver_ihl;
    unsigned char  tos;
    unsigned short total_len;
    unsigned short id;
    unsigned short flags_frag;
    unsigned char  ttl;
    unsigned char  protocol;
    unsigned short checksum;
    unsigned char  src[4];
    unsigned char  dst[4];
} ip4_t;

typedef struct {
    unsigned char dst_mac[6];
    unsigned char src_mac[6];
    unsigned short ethertype;
} eth_t;

static unsigned short checksum(void *data, int len) {
    unsigned short *p = data;
    unsigned int sum = 0;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len) sum += *(unsigned char*)p;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

int network_ping(unsigned char *dst_ip) {
    if (!g_net) return -1;
    static unsigned char pkt[1500];
    unsigned char *mac = g_net->Mode->CurrentAddress.Addr;
    eth_t  *eth  = (eth_t*)pkt;
    ip4_t  *ip   = (ip4_t*)(pkt + 14);
    icmp_t *icmp = (icmp_t*)(pkt + 14 + 20);
    for (int i = 0; i < 6; i++) eth->dst_mac[i] = 0xFF;
    for (int i = 0; i < 6; i++) eth->src_mac[i] = mac[i];
    eth->ethertype = 0x0008;
    ip->ver_ihl    = 0x45;
    ip->tos        = 0;
    ip->total_len  = __builtin_bswap16(20 + 8 + 32);
    ip->id         = 0x0100;
    ip->flags_frag = 0;
    ip->ttl        = 64;
    ip->protocol   = 1;
    ip->checksum   = 0;
    for (int i = 0; i < 4; i++) ip->src[i] = g_ip[i];
    for (int i = 0; i < 4; i++) ip->dst[i] = dst_ip[i];
    ip->checksum   = checksum(ip, 20);
    icmp->type     = 8;
    icmp->code     = 0;
    icmp->checksum = 0;
    icmp->id       = 0x0100;
    icmp->seq      = 0x0100;
    for (int i = 0; i < 32; i++) icmp->data[i] = i;
    icmp->checksum = checksum(icmp, 8 + 32);
    UINTN pkt_size = 14 + 20 + 8 + 32;
    EFI_STATUS st = uefi_call_wrapper(g_net->Transmit, 6, g_net, 0, pkt_size, pkt, NULL, NULL, NULL);
    return EFI_ERROR(st) ? -1 : 0;
}