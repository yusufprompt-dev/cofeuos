/*
 * NETWORK.C - UEFI Network Driver
 */
#include <efi.h>
#include <efilib.h>
#include <efinet.h>
#include "../include/network.h"
#include "../include/fs.h"
#include "../include/string.h"

extern fs_control_block g_fs;

/* ─── TCP Yapıları ─────────────────────────────────────────────── */
typedef struct {
    unsigned short src_port;
    unsigned short dst_port;
    unsigned int   seq;
    unsigned int   ack;
    unsigned char  data_offset;
    unsigned char  flags;
    unsigned short window;
    unsigned short csum;
    unsigned short urg_ptr;
} __attribute__((packed)) tcp_hdr_t;

typedef struct {
    unsigned char  src[4];
    unsigned char  dst[4];
    unsigned char  zero;
    unsigned char  proto;
    unsigned short tcp_len;
} __attribute__((packed)) tcp_pseudo_hdr_t;

typedef struct { unsigned char dst[6]; unsigned char src[6]; unsigned short type; } __attribute__((packed)) eth_hdr_t;
typedef struct {
    unsigned char  ver_ihl, tos;
    unsigned short len, id, flags;
    unsigned char  ttl, proto;
    unsigned short csum;
    unsigned char  src[4], dst[4];
} __attribute__((packed)) ip4_hdr_t;
typedef struct {
    unsigned char  type, code;
    unsigned short csum, id, seq;
    unsigned char  data[32];
} __attribute__((packed)) icmp_t;

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10
// network.c'nin en başına ekle
extern EFI_FILE_HANDLE root_dir; // Eğer sisteminde kök dizin tutamacın böyle tanımlıysa

/* ─── Global Değişkenler ────────────────────────────────────────── */
static EFI_SIMPLE_NETWORK_PROTOCOL *g_net = (void*)0;
static EFI_SYSTEM_TABLE            *g_st  = (void*)0;
static unsigned char g_ip[4]      = {0,0,0,0};
static unsigned int  g_tcp_seq    = 0;
static unsigned int  g_tcp_ack    = 0;
static unsigned short g_tcp_local_port = 49152;

/* ─── Yardımcı Fonksiyonlar ────────────────────────────────────── */
static unsigned short csum16(void *d, int n) {
    unsigned short *p = d; unsigned int s = 0;
    while (n > 1) { s += *p++; n -= 2; }
    if (n) s += *(unsigned char*)p;
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return ~(unsigned short)s;
}

/* ─── TCP Paket Motoru ─────────────────────────────────────────── */
static int send_tcp_packet(unsigned char *dst_ip, unsigned short dst_port, 
                           unsigned char flags, const char *data, int data_len) {
    if (!g_net) return -1;
    static unsigned char pkt[1500];
    unsigned char *mac = g_net->Mode->CurrentAddress.Addr;

    eth_hdr_t *eth  = (eth_hdr_t*)pkt;
    ip4_hdr_t *ip   = (ip4_hdr_t*)(pkt + 14);
    tcp_hdr_t *tcp  = (tcp_hdr_t*)(pkt + 14 + 20);
    unsigned char *payload = pkt + 14 + 20 + 20;

    for (int i=0; i<6; i++) { eth->dst[i] = 0xFF; eth->src[i] = mac[i]; }
    eth->type = __builtin_bswap16(0x0800);

    ip->ver_ihl = 0x45; ip->tos = 0;
    ip->len     = __builtin_bswap16(20 + 20 + data_len);
    ip->id      = __builtin_bswap16(0x0200); ip->flags = 0;
    ip->ttl     = 64; ip->proto = 6; ip->csum = 0;
    for (int i=0; i<4; i++) { ip->src[i] = g_ip[i]; ip->dst[i] = dst_ip[i]; }
    ip->csum = csum16(ip, 20);

    tcp->src_port = __builtin_bswap16(g_tcp_local_port);
    tcp->dst_port = __builtin_bswap16(dst_port);
    tcp->seq      = __builtin_bswap32(g_tcp_seq);
    tcp->ack      = __builtin_bswap32(g_tcp_ack);
    tcp->data_offset = 0x50;
    tcp->flags    = flags;
    tcp->window   = __builtin_bswap16(8192);
    tcp->csum     = 0;
    tcp->urg_ptr  = 0;

    if (data_len > 0 && data != NULL) {
        for (int i = 0; i < data_len; i++) payload[i] = data[i];
    }

    static unsigned char pseudo_buf[1500];
    tcp_pseudo_hdr_t *phdr = (tcp_pseudo_hdr_t*)pseudo_buf;
    for (int i=0; i<4; i++) { phdr->src[i] = g_ip[i]; phdr->dst[i] = dst_ip[i]; }
    phdr->zero = 0; phdr->proto = 6;
    phdr->tcp_len = __builtin_bswap16(20 + data_len);
    for (int i=0; i < 20 + data_len; i++) { pseudo_buf[12 + i] = pkt[34 + i]; }
    tcp->csum = csum16(pseudo_buf, 12 + 20 + data_len);

    UINTN sz = 14 + 20 + 20 + data_len;
    EFI_STATUS st = uefi_call_wrapper(g_net->Transmit, 6, g_net, 0, sz, pkt, NULL, NULL, NULL);
    return EFI_ERROR(st) ? -1 : 0;
}

/* ─── Temel Fonksiyonlar ───────────────────────────────────────── */
void network_init(void *st) {
    g_st = (EFI_SYSTEM_TABLE*)st;
    EFI_GUID net_guid = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
    uefi_call_wrapper(g_st->BootServices->LocateProtocol, 3, &net_guid, NULL, (VOID**)&g_net);
    if (!g_net) return;
    uefi_call_wrapper(g_net->Start, 1, g_net);
    uefi_call_wrapper(g_net->Initialize, 3, g_net, 0, 0);
    uefi_call_wrapper(g_net->ReceiveFilters, 6, g_net, 0x01 | 0x02, 0, FALSE, 0, NULL);
}
/* ─── Ping Fonksiyonu ─────────────────────────────────────────── */
int network_ping(unsigned char *dst_ip) {
    if (!g_net) return -1;
    
    // Paket için statik alan (global değil, fonksiyon içinde static)
    static unsigned char pkt[1500];
    unsigned char *mac = g_net->Mode->CurrentAddress.Addr;

    // Header yapılarını işaretçilerle paket içine yerleştiriyoruz
    eth_hdr_t *eth  = (eth_hdr_t*)pkt;
    ip4_hdr_t *ip   = (ip4_hdr_t*)(pkt + 14);
    icmp_t    *icmp = (icmp_t*)(pkt + 14 + 20);

    // Ethernet: Broadcast MAC (FF:FF:FF:FF:FF:FF)
    for (int i = 0; i < 6; i++) { eth->dst[i] = 0xFF; eth->src[i] = mac[i]; }
    eth->type = __builtin_bswap16(0x0800);

    // IP Başlığı
    ip->ver_ihl = 0x45; 
    ip->tos = 0;
    ip->len     = __builtin_bswap16(20 + 8 + 32);
    ip->id      = 0x0100; 
    ip->flags   = 0;
    ip->ttl     = 64; 
    ip->proto   = 1; // ICMP
    ip->csum    = 0;
    for (int i = 0; i < 4; i++) { ip->src[i] = g_ip[i]; ip->dst[i] = dst_ip[i]; }
    ip->csum = csum16(ip, 20);

    // ICMP Başlığı
    icmp->type = 8; // Echo Request
    icmp->code = 0; 
    icmp->csum = 0;
    icmp->id   = 0x0100; 
    icmp->seq  = 0x0100;
    for (int i = 0; i < 32; i++) icmp->data[i] = (unsigned char)i;
    icmp->csum = csum16(icmp, 8 + 32);

    // Paketi gönder
    UINTN sz = 14 + 20 + 8 + 32;
    EFI_STATUS st = uefi_call_wrapper(
        g_net->Transmit, 6, g_net, 0, sz, pkt, NULL, NULL, NULL);
        
    return EFI_ERROR(st) ? -1 : 0;
}
int network_recv(unsigned char *buf, unsigned int *len) {
    if (!g_net) return -1;
    UINTN sz = 1500;
    EFI_STATUS st = uefi_call_wrapper(g_net->Receive, 7, g_net, NULL, &sz, buf, NULL, NULL, NULL);
    if (st == EFI_SUCCESS) { *len = (unsigned int)sz; return 0; }
    return (st == EFI_NOT_READY) ? -2 : -1;
}
/* ─── Linker için eksik tamamlayıcılar ─── */
void network_dhcp(void) {
    g_ip[0]=10; g_ip[1]=0; g_ip[2]=2; g_ip[3]=15;
}

int network_available(void) { 
    return g_net != (void*)0; 
}

void network_get_mac(unsigned char *mac) {
    if (g_net) for (int i = 0; i < 6; i++) mac[i] = g_net->Mode->CurrentAddress.Addr[i];
}

void network_get_ip(unsigned char *ip) {
    for (int i = 0; i < 4; i++) ip[i] = g_ip[i];
}
static int parse_octet(const char *s, int *out) {
    if (s == NULL || out == NULL || *s == '\0') return -1;
    int value = 0;
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        s++;
    }
    if (*s != '\0') return -1;
    *out = value;
    return 0;
}

static int parse_ipv4_host(const char *host, unsigned char *out) {
    if (host == NULL || out == NULL) return -1;

    char tmp[64];
    const char *src = host;
    if (strncmp(src, "http://", 7) == 0) src += 7;
    else if (strncmp(src, "https://", 8) == 0) src += 8;

    const char *slash = strchr(src, '/');
    const char *colon = strchr(src, ':');
    const char *end = slash ? slash : (colon ? colon : src + strlen(src));
    int len = (int)(end - src);
    if (len <= 0 || len >= (int)sizeof(tmp)) return -1;

    memcpy(tmp, src, len);
    tmp[len] = '\0';

    char copy[64];
    strcpy(copy, tmp);
    char *token = strtok(copy, ".");
    int octets[4] = {0, 0, 0, 0};
    int count = 0;
    while (token != NULL && count < 4) {
        int value = 0;
        if (parse_octet(token, &value) != 0) return -1;
        octets[count++] = value;
        token = strtok(NULL, ".");
    }

    if (count != 4) return -1;
    for (int i = 0; i < 4; i++) {
        if (octets[i] < 0 || octets[i] > 255) return -1;
        out[i] = (unsigned char)octets[i];
    }
    return 0;
}

/* ─── TCP / HTTP Mantığı ───────────────────────────────────────── */
int tcp_connect(unsigned char *dst_ip, unsigned short port) {
    g_tcp_seq = 1000; g_tcp_ack = 0; g_tcp_local_port++;
    send_tcp_packet(dst_ip, port, TCP_SYN, NULL, 0);
    
    unsigned char rx[1500]; unsigned int rx_len = 0;
    for(int i=0; i<500000; i++) {
        if(network_recv(rx, &rx_len) == 0) {
            tcp_hdr_t *tcp = (tcp_hdr_t*)(rx + 34);
            if((tcp->flags & (TCP_SYN|TCP_ACK)) == (TCP_SYN|TCP_ACK)) {
                g_tcp_seq++; g_tcp_ack = __builtin_bswap32(tcp->seq) + 1;
                send_tcp_packet(dst_ip, port, TCP_ACK, NULL, 0);
                return 0;
            }
        }
    }
    return -1;
}

/* ─── TAM WGET FONKSİYONU ────────────────────────────────────────── */
int wget(const char *host, const char *http_path, const char *save_path, char *buf, int maxlen) {
    unsigned char dest[4] = {10, 0, 2, 2};
    int i = 0;

    if (host != NULL && parse_ipv4_host(host, dest) != 0) {
        dest[0] = 10; dest[1] = 0; dest[2] = 2; dest[3] = 2;
    }

    if (tcp_connect(dest, 80) != 0) return -1;

    char req[256];
    char *p = req;
    char *cmd = "GET ";
    char *h = " HTTP/1.1\r\nHost: 10.0.2.2\r\nConnection: close\r\n\r\n";

    while (*cmd) *p++ = *cmd++;
    const char *path_ptr = http_path;
    while (*path_ptr) *p++ = *path_ptr++;
    while (*h) *p++ = *h++;
    *p = '\0';
    int req_len = (int)(p - req);

    send_tcp_packet(dest, 80, TCP_PSH | TCP_ACK, req, req_len);
    g_tcp_seq += req_len;

    unsigned char rx_pkt[1500];
    unsigned int rx_len = 0;
    int total_received = 0;
    int header_done = 0;

    for (i = 0; i < 1000000; i++) {
        if (network_recv(rx_pkt, &rx_len) == 0) {
            if (rx_len > 54) {
                /* HTTP başlığını bul ve atla */
                if (!header_done) {
                    int header_found = 0;
                    for (int k = 54; k < (int)rx_len - 3; k++) {
                        if (rx_pkt[k] == '\r' && rx_pkt[k+1] == '\n' && 
                            rx_pkt[k+2] == '\r' && rx_pkt[k+3] == '\n') {
                            /* Header bulundu! */
                            int start = k + 4;
                            int data_len = rx_len - start;
                            if (data_len > 0 && total_received + data_len < maxlen) {
                                for (int j = 0; j < data_len; j++) {
                                    buf[total_received++] = rx_pkt[start + j];
                                }
                            }
                            header_done = 1;
                            header_found = 1;
                            break;
                        }
                    }
                    /* Header bulunmadıysa bu paket tamamı header, skip et */
                    if (!header_found) {
                        continue;
                    }
                } else {
                    /* Header zaten atlandı, tüm paket body */
                    int data_len = rx_len - 54;
                    if (data_len > 0 && total_received + data_len < maxlen) {
                        for (int j = 0; j < data_len; j++) {
                            buf[total_received++] = rx_pkt[54 + j];
                        }
                    }
                }
            }
        }
    }

    if (total_received > 0) {
        int write_result = fs_write_file(&g_fs, save_path, buf, (size_t)total_received);
        if (write_result < 0) {
            return -3; /* Write error */
        }
    }

    if (maxlen > 0) {
        if (total_received < maxlen) {
            buf[total_received] = '\0';
        } else {
            buf[maxlen - 1] = '\0';
        }
    }
    return total_received;
}