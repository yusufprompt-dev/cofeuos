/*
 * NETWORK.C - UEFI Network Driver (ARP + DNS + Domain + HTTP)
 */
#include <efi.h>
#include <efilib.h>
#include <efinet.h>
#include "../include/network.h"
#include "../include/fs.h"
#include "../include/string.h"
#include "../include/io.h"

extern fs_control_block g_fs;

/* ─── Yapılar ──────────────────────────────────────────────── */
typedef struct { unsigned char dst[6]; unsigned char src[6]; unsigned short type; } __attribute__((packed)) eth_hdr_t;
typedef struct {
    unsigned char  ver_ihl, tos;
    unsigned short len, id, flags;
    unsigned char  ttl, proto;
    unsigned short csum;
    unsigned char  src[4], dst[4];
} __attribute__((packed)) ip4_hdr_t;
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
typedef struct {
    unsigned char  type, code;
    unsigned short csum, id, seq;
    unsigned char  data[32];
} __attribute__((packed)) icmp_t;
typedef struct {
    unsigned short src_port;
    unsigned short dst_port;
    unsigned short len;
    unsigned short csum;
} __attribute__((packed)) udp_hdr_t;
typedef struct {
    unsigned short id;
    unsigned short flags;
    unsigned short qdcount;
    unsigned short ancount;
    unsigned short nscount;
    unsigned short arcount;
} __attribute__((packed)) dns_hdr_t;

/* ARP Başlığı */
typedef struct {
    unsigned short hw_type;
    unsigned short proto_type;
    unsigned char  hw_len;
    unsigned char  proto_len;
    unsigned short opcode;
    unsigned char  sender_mac[6];
    unsigned char  sender_ip[4];
    unsigned char  target_mac[6];
    unsigned char  target_ip[4];
} __attribute__((packed)) arp_hdr_t;

#define ARP_OP_REQUEST 0x0100
#define ARP_OP_REPLY   0x0200

#define TCP_FIN 0x01
#define TCP_SYN 0x02
#define TCP_RST 0x04
#define TCP_PSH 0x08
#define TCP_ACK 0x10

/* ─── Global Değişkenler ────────────────────────────────────── */
static EFI_SIMPLE_NETWORK_PROTOCOL *g_net = (void*)0;
static EFI_SYSTEM_TABLE            *g_st  = (void*)0;
static unsigned char g_ip[4]      = {10,0,2,15};
static unsigned char g_gateway[4] = {10,0,2,2};
static unsigned char g_dns_server[4] = {10,0,2,3};
static unsigned int  g_tcp_seq    = 0;
static unsigned int  g_tcp_ack    = 0;
static unsigned short g_tcp_local_port = 49152;
static unsigned char g_tcp_remote_ip[4] = {0,0,0,0};
static unsigned short g_tcp_remote_port = 0;
static int g_tcp_connected = 0;

/* ARP Önbellek (gateway) */
static unsigned char g_gateway_mac[6] = {0,0,0,0,0,0};
static int g_gateway_mac_valid = 0;

/* DNS Önbellek */
#define DNS_CACHE_SIZE 16
typedef struct {
    char domain[64];
    unsigned char ip[4];
} dns_cache_entry_t;
static dns_cache_entry_t g_dns_cache[DNS_CACHE_SIZE];
static int g_dns_cache_count = 0;

/* ─── Yardımcı Fonksiyonlar ────────────────────────────────── */
static unsigned short csum16(void *d, int n) {
    unsigned short *p = d; unsigned int s = 0;
    while (n > 1) { s += *p++; n -= 2; }
    if (n) s += *(unsigned char*)p;
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return ~(unsigned short)s;
}

static void uefi_stall(unsigned int microseconds) {
    if (g_st && g_st->BootServices)
        g_st->BootServices->Stall(microseconds);
}

static int mac_eq(unsigned char *a, unsigned char *b) {
    for (int i = 0; i < 6; i++) if (a[i] != b[i]) return 0;
    return 1;
}

static int mac_is_zero(unsigned char *m) {
    for (int i = 0; i < 6; i++) if (m[i] != 0) return 0;
    return 1;
}

static void mac_copy(unsigned char *dst, unsigned char *src) {
    for (int i = 0; i < 6; i++) dst[i] = src[i];
}

/* ─── Ham Paket Gönder/Al ──────────────────────────────────── */
static int raw_send(unsigned char *pkt, int len) {
    if (!g_net || !pkt || len <= 0 || len > 1500) return -1;

    /* Simple Network Protocol gönderimi asenkrondur: Transmit'e verilen
       tampon, GetStatus aynı tamponu geri verene kadar geçerliliğini korumalı.
       Özellikle DNS'in yığın üzerindeki paketi aksi halde TCP isteği tarafından
       eziliyor ve bağlantı zaman aşımına uğruyordu. */
    for (int attempt = 0; attempt < 100; attempt++) {
        EFI_STATUS st = uefi_call_wrapper(g_net->Transmit, 6, g_net, 0,
                                          (UINTN)len, pkt, NULL, NULL, NULL);
        if (st == EFI_SUCCESS) {
            for (int wait = 0; wait < 100; wait++) {
                VOID *completed = NULL;
                EFI_STATUS status = uefi_call_wrapper(g_net->GetStatus, 3,
                                                       g_net, NULL, &completed);
                if (status == EFI_SUCCESS && completed == pkt) return 0;
                uefi_stall(1000);
            }
            return -1;
        }
        if (st != EFI_NOT_READY) return -1;

        VOID *completed = NULL;
        uefi_call_wrapper(g_net->GetStatus, 3, g_net, NULL, &completed);
        uefi_stall(1000);
    }
    return -1;
}

static int raw_recv(unsigned char *buf, unsigned int *len) {
    if (!g_net) return -1;
    UINTN sz = 1500;
    EFI_STATUS st = uefi_call_wrapper(g_net->Receive, 7, g_net, NULL, &sz, buf, NULL, NULL, NULL);
    if (st == EFI_SUCCESS) { *len = (unsigned int)sz; return 0; }
    return (st == EFI_NOT_READY) ? -2 : -1;
}

/* ─── ARP Çözümleme ────────────────────────────────────────── */
static int arp_resolve(unsigned char *target_ip, unsigned char *out_mac) {
    if (!g_net) return -1;

    unsigned char *my_mac = g_net->Mode->CurrentAddress.Addr;

    /* ARP Request paketi oluştur */
    static unsigned char pkt[64];
    eth_hdr_t *eth = (eth_hdr_t*)pkt;
    arp_hdr_t *arp = (arp_hdr_t*)(pkt + 14);

    /* Ethernet: broadcast */
    for (int i = 0; i < 6; i++) { eth->dst[i] = 0xFF; eth->src[i] = my_mac[i]; }
    eth->type = __builtin_bswap16(0x0806);

    /* ARP */
    arp->hw_type = __builtin_bswap16(1);    /* Ethernet */
    arp->proto_type = __builtin_bswap16(0x0800); /* IPv4 */
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = ARP_OP_REQUEST;
    for (int i = 0; i < 6; i++) arp->sender_mac[i] = my_mac[i];
    for (int i = 0; i < 4; i++) arp->sender_ip[i] = g_ip[i];
    for (int i = 0; i < 6; i++) arp->target_mac[i] = 0x00;
    for (int i = 0; i < 4; i++) arp->target_ip[i] = target_ip[i];

    raw_send(pkt, 42);

    /* Yanıt bekle */
    static unsigned char rx[1500];
    unsigned int rx_len = 0;
    for (int attempt = 0; attempt < 100; attempt++) {
        uefi_stall(10000); /* 10ms */
        if (raw_recv(rx, &rx_len) == 0 && rx_len >= 42) {
            unsigned short ether_type = __builtin_bswap16(*(unsigned short*)(rx + 12));
            if (ether_type == 0x0806) {
                arp_hdr_t *reply = (arp_hdr_t*)(rx + 14);
                if (reply->opcode == ARP_OP_REPLY) {
                    if (reply->sender_ip[0] == target_ip[0] &&
                        reply->sender_ip[1] == target_ip[1] &&
                        reply->sender_ip[2] == target_ip[2] &&
                        reply->sender_ip[3] == target_ip[3]) {
                        mac_copy(out_mac, reply->sender_mac);
                        return 0;
                    }
                }
            }
        }
    }
    return -1; /* Timeout */
}

static unsigned char* get_dst_mac(unsigned char *dst_ip) {
    /* Gateway'e gidiyorsa (gateway MAC'ini kullan) */
    if (dst_ip[0] != g_ip[0] || dst_ip[1] != g_ip[1] || dst_ip[2] != g_ip[2] || dst_ip[3] != g_ip[3]) {
        if (!g_gateway_mac_valid) {
            if (arp_resolve(g_gateway, g_gateway_mac) == 0) {
                g_gateway_mac_valid = 1;
            } else {
                /* ARP başarısızsa broadcast dene */
                static unsigned char bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
                return bcast;
            }
        }
        return g_gateway_mac;
    }
    /* Aynı alt ağ */
    return g_net->Mode->CurrentAddress.Addr;
}

/* ─── Ethernet + IP + Payload Gönder ──────────────────────── */
static int send_raw_ip(unsigned char *dst_ip, unsigned char proto, unsigned char *data, int data_len) {
    if (!g_net) return -1;
    static unsigned char pkt[1500];
    unsigned char *my_mac = g_net->Mode->CurrentAddress.Addr;
    unsigned char *dst_mac = get_dst_mac(dst_ip);

    eth_hdr_t *eth = (eth_hdr_t*)pkt;
    for (int i = 0; i < 6; i++) { eth->dst[i] = dst_mac[i]; eth->src[i] = my_mac[i]; }
    eth->type = __builtin_bswap16(0x0800);

    ip4_hdr_t *ip = (ip4_hdr_t*)(pkt + 14);
    ip->ver_ihl = 0x45; ip->tos = 0;
    ip->len = __builtin_bswap16(20 + data_len);
    ip->id = 0x0100; ip->flags = 0;
    ip->ttl = 64; ip->proto = proto; ip->csum = 0;
    for (int i = 0; i < 4; i++) { ip->src[i] = g_ip[i]; ip->dst[i] = dst_ip[i]; }
    ip->csum = csum16(ip, 20);

    if (data_len > 0)
        for (int i = 0; i < data_len; i++) pkt[14 + 20 + i] = data[i];

    return raw_send(pkt, 14 + 20 + data_len);
}

/* ─── DNS Çözümleme ─────────────────────────────────────── */
int dns_resolve(const char *domain, unsigned char *out_ip) {
    for (int i = 0; i < g_dns_cache_count; i++) {
        if (strcmp(g_dns_cache[i].domain, domain) == 0) {
            for (int j = 0; j < 4; j++) out_ip[j] = g_dns_cache[i].ip[j];
            return 0;
        }
    }

    if (!g_net) return -1;

    static unsigned char pkt[512];
    static unsigned char rx[1500];
    unsigned int rx_len = 0;

    /* DNS sorgusunu 3 kez dene, her denemede farkli transaction ID */
    for (int dns_try = 0; dns_try < 3; dns_try++) {
        unsigned short tx_id = (unsigned short)(0x1234 + dns_try);

        /* DNS payload olustur */
        unsigned char udp_data[512];
        int ud_len = 0;
        dns_hdr_t *dns_q = (dns_hdr_t*)udp_data;
        dns_q->id      = __builtin_bswap16(tx_id);
        dns_q->flags   = __builtin_bswap16(0x0100); /* RD=1 */
        dns_q->qdcount = __builtin_bswap16(1);
        dns_q->ancount = 0; dns_q->nscount = 0; dns_q->arcount = 0;
        ud_len = 12;

        /* Domain encode (www.example.com -> 3www7example3com0) */
        const char *d = domain;
        while (*d) {
            const char *dot = d;
            while (*dot && *dot != '.') dot++;
            int llen = (int)(dot - d);
            if (llen > 0) {
                udp_data[ud_len++] = (unsigned char)llen;
                for (int i = 0; i < llen; i++) udp_data[ud_len++] = (unsigned char)d[i];
            }
            d = dot;
            if (*d == '.') d++;
        }
        udp_data[ud_len++] = 0;            /* root label */
        udp_data[ud_len++] = 0; udp_data[ud_len++] = 1; /* QTYPE  A    */
        udp_data[ud_len++] = 0; udp_data[ud_len++] = 1; /* QCLASS IN   */

        /* Ethernet + IP + UDP cercevesi */
        unsigned char *my_mac = g_net->Mode->CurrentAddress.Addr;
        unsigned char *dst_mac = get_dst_mac(g_dns_server);

        eth_hdr_t *eth = (eth_hdr_t*)pkt;
        for (int i = 0; i < 6; i++) { eth->dst[i] = dst_mac[i]; eth->src[i] = my_mac[i]; }
        eth->type = __builtin_bswap16(0x0800);

        ip4_hdr_t *ip = (ip4_hdr_t*)(pkt + 14);
        ip->ver_ihl = 0x45; ip->tos = 0; ip->id = 0;
        ip->flags = 0; ip->ttl = 64; ip->proto = 17; ip->csum = 0;
        for (int i = 0; i < 4; i++) { ip->src[i] = g_ip[i]; ip->dst[i] = g_dns_server[i]; }

        udp_hdr_t *udp = (udp_hdr_t*)(pkt + 14 + 20);
        udp->src_port = __builtin_bswap16(53535);
        udp->dst_port = __builtin_bswap16(53);
        udp->len      = __builtin_bswap16(8 + ud_len);
        udp->csum     = 0;

        for (int i = 0; i < ud_len; i++) pkt[14 + 20 + 8 + i] = udp_data[i];

        int total_len = 14 + 20 + 8 + ud_len;
        ip->len  = __builtin_bswap16(total_len - 14);
        ip->csum = csum16(ip, 20);

        raw_send(pkt, total_len);

        /* Yanit bekle: 300x10ms = 3 saniye */
        for (int attempt = 0; attempt < 300; attempt++) {
            uefi_stall(10000);
            if (raw_recv(rx, &rx_len) != 0 || rx_len < 42) continue;

            unsigned short etype = __builtin_bswap16(*(unsigned short*)(rx + 12));
            if (etype != 0x0800) continue;          /* IPv4 degil */

            ip4_hdr_t *rip = (ip4_hdr_t*)(rx + 14);
            if (rip->proto != 17) continue;         /* UDP degil  */

            /* IP header uzunlugunu dinamik hesapla */
            int ip_hlen = (rip->ver_ihl & 0x0f) * 4;
            if (ip_hlen < 20) continue;

            int udp_off = 14 + ip_hlen;
            if ((int)rx_len < udp_off + 8 + 12) continue;

            udp_hdr_t *rudp = (udp_hdr_t*)(rx + udp_off);
            if (__builtin_bswap16(rudp->src_port) != 53)   continue; /* DNS degil */
            if (__builtin_bswap16(rudp->dst_port) != 53535) continue; /* bize degil */

            int dns_off = udp_off + 8;
            dns_hdr_t *rdns = (dns_hdr_t*)(rx + dns_off);

            /* QR=1 (yanit) ve RCODE=0 (basarili) kontrol et */
            unsigned short rflags = __builtin_bswap16(rdns->flags);
            if (!(rflags & 0x8000)) continue;       /* QR degil */
            if ((rflags & 0x000F) != 0) continue;   /* RCODE != 0 (hata) */
            if (__builtin_bswap16(rdns->ancount) == 0) continue;

            /* Soru bolumunu atla */
            int ans_off = dns_off + 12;
            /* QNAME */
            while (ans_off < (int)rx_len && rx[ans_off] != 0) {
                if ((rx[ans_off] & 0xC0) == 0xC0) { ans_off += 2; goto skip_qname; }
                ans_off += rx[ans_off] + 1;
            }
            ans_off++;          /* root null */
            skip_qname:
            ans_off += 4;       /* QTYPE + QCLASS */

            /* Cevap kayitlarini tara */
            unsigned short ancount = __builtin_bswap16(rdns->ancount);
            for (int a = 0; a < ancount && ans_off + 10 < (int)rx_len; a++) {
                /* NAME */
                if ((rx[ans_off] & 0xC0) == 0xC0) ans_off += 2;
                else { while (ans_off < (int)rx_len && rx[ans_off] != 0) ans_off++; ans_off++; }

                if (ans_off + 10 >= (int)rx_len) break;
                unsigned short rtype  = __builtin_bswap16(*(unsigned short*)(rx + ans_off));
                /* TYPE(2) CLASS(2) TTL(4) = 8 byte atla */
                ans_off += 8;
                unsigned short rdlen = __builtin_bswap16(*(unsigned short*)(rx + ans_off));
                ans_off += 2;

                if (rtype == 1 && rdlen == 4 && ans_off + 4 <= (int)rx_len) {
                    for (int j = 0; j < 4; j++) out_ip[j] = rx[ans_off + j];
                    if (g_dns_cache_count < DNS_CACHE_SIZE) {
                        strcpy(g_dns_cache[g_dns_cache_count].domain, domain);
                        for (int j = 0; j < 4; j++)
                            g_dns_cache[g_dns_cache_count].ip[j] = out_ip[j];
                        g_dns_cache_count++;
                    }
                    return 0;
                }
                if (ans_off + rdlen > (int)rx_len) break;
                ans_off += rdlen;
            }
        }
    }
    return -1;
}

/* ─── Host Parse ─────────────────────────────────────────── */
static int parse_host(const char *host, unsigned char *out_ip) {
    if (!host) return -1;
    int is_ip = 1;
    const char *p = host;
    while (*p) {
        if ((*p < '0' || *p > '9') && *p != '.') { is_ip = 0; break; }
        p++;
    }
    if (is_ip) {
        const char *s = host;
        for (int i = 0; i < 4; i++) {
            int n = 0;
            while (*s >= '0' && *s <= '9') { n = n * 10 + (*s - '0'); s++; }
            out_ip[i] = (unsigned char)n;
            if (*s == '.') s++;
        }
        return 0;
    }
    return dns_resolve(host, out_ip);
}

/* ─── TCP ────────────────────────────────────────────────── */
static int send_tcp_packet(unsigned char *dst_ip, unsigned short dst_port,
                           unsigned char flags, const char *data, int data_len) {
    if (!g_net) return -1;
    static unsigned char pkt[1500];
    unsigned char *my_mac = g_net->Mode->CurrentAddress.Addr;
    unsigned char *dst_mac = get_dst_mac(dst_ip);

    eth_hdr_t *eth = (eth_hdr_t*)pkt;
    for (int i = 0; i < 6; i++) { eth->dst[i] = dst_mac[i]; eth->src[i] = my_mac[i]; }
    eth->type = __builtin_bswap16(0x0800);

    ip4_hdr_t *ip = (ip4_hdr_t*)(pkt + 14);
    ip->ver_ihl = 0x45; ip->tos = 0;
    ip->len = __builtin_bswap16(20 + 20 + data_len);
    ip->id = __builtin_bswap16(0x0200); ip->flags = 0;
    ip->ttl = 64; ip->proto = 6; ip->csum = 0;
    for (int i = 0; i < 4; i++) { ip->src[i] = g_ip[i]; ip->dst[i] = dst_ip[i]; }
    ip->csum = csum16(ip, 20);

    tcp_hdr_t *tcp = (tcp_hdr_t*)(pkt + 14 + 20);
    tcp->src_port = __builtin_bswap16(g_tcp_local_port);
    tcp->dst_port = __builtin_bswap16(dst_port);
    tcp->seq = __builtin_bswap32(g_tcp_seq);
    tcp->ack = __builtin_bswap32(g_tcp_ack);
    tcp->data_offset = 0x50;
    tcp->flags = flags;
    tcp->window = __builtin_bswap16(8192);
    tcp->csum = 0;
    tcp->urg_ptr = 0;

    unsigned char *payload = pkt + 14 + 20 + 20;
    if (data_len > 0 && data)
        for (int i = 0; i < data_len; i++) payload[i] = data[i];

    /* TCP checksum (pseudo-header) */
    static unsigned char pseudo_buf[1500];
    tcp_pseudo_hdr_t *phdr = (tcp_pseudo_hdr_t*)pseudo_buf;
    for (int i = 0; i < 4; i++) { phdr->src[i] = g_ip[i]; phdr->dst[i] = dst_ip[i]; }
    phdr->zero = 0; phdr->proto = 6;
    phdr->tcp_len = __builtin_bswap16(20 + data_len);
    for (int i = 0; i < 20 + data_len; i++) pseudo_buf[12 + i] = pkt[34 + i];
    tcp->csum = csum16(pseudo_buf, 12 + 20 + data_len);

    return raw_send(pkt, 14 + 20 + 20 + data_len);
}

/* ─── Temel Fonksiyonlar ──────────────────────────────────── */
void network_init(void *st) {
    g_st = (EFI_SYSTEM_TABLE*)st;
    EFI_GUID net_guid = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
    uefi_call_wrapper(g_st->BootServices->LocateProtocol, 3, &net_guid, NULL, (VOID**)&g_net);
    if (!g_net) return;
    uefi_call_wrapper(g_net->Start, 1, g_net);
    uefi_call_wrapper(g_net->Initialize, 3, g_net, 0, 0);
    uefi_call_wrapper(g_net->ReceiveFilters, 6, g_net, 0x01 | 0x02, 0, FALSE, 0, NULL);
    network_dhcp();
}

int network_ping(unsigned char *dst_ip) {
    if (!g_net) return -1;

    static unsigned char pkt[1500];
    unsigned char *my_mac = g_net->Mode->CurrentAddress.Addr;
    unsigned char *dst_mac = get_dst_mac(dst_ip);

    eth_hdr_t *eth = (eth_hdr_t*)pkt;
    for (int i = 0; i < 6; i++) { eth->dst[i] = dst_mac[i]; eth->src[i] = my_mac[i]; }
    eth->type = __builtin_bswap16(0x0800);

    ip4_hdr_t *ip = (ip4_hdr_t*)(pkt + 14);
    ip->ver_ihl = 0x45; ip->tos = 0;
    ip->len = __builtin_bswap16(20 + 8 + 32);
    ip->id = 0x0100; ip->flags = 0;
    ip->ttl = 64; ip->proto = 1; ip->csum = 0;
    for (int i = 0; i < 4; i++) { ip->src[i] = g_ip[i]; ip->dst[i] = dst_ip[i]; }
    ip->csum = csum16(ip, 20);

    icmp_t *icmp = (icmp_t*)(pkt + 14 + 20);
    icmp->type = 8; icmp->code = 0;
    icmp->csum = 0; icmp->id = 0x0100; icmp->seq = 0x0100;
    for (int i = 0; i < 32; i++) icmp->data[i] = (unsigned char)i;
    icmp->csum = csum16(icmp, 8 + 32);

    return raw_send(pkt, 14 + 20 + 8 + 32);
}

int network_recv(unsigned char *buf, unsigned int *len) {
    return raw_recv(buf, len);
}

void network_dhcp(void) {
    g_ip[0] = 10; g_ip[1] = 0; g_ip[2] = 2; g_ip[3] = 15;
    g_gateway_mac_valid = 0; /* Yeniden ARP gerekli */
}

int network_available(void) { return g_net != (void*)0; }

void network_get_mac(unsigned char *mac) {
    if (g_net) for (int i = 0; i < 6; i++) mac[i] = g_net->Mode->CurrentAddress.Addr[i];
}

void network_get_ip(unsigned char *ip) {
    for (int i = 0; i < 4; i++) ip[i] = g_ip[i];
}

void network_set_dns(unsigned char *dns_ip) {
    for (int i = 0; i < 4; i++) g_dns_server[i] = dns_ip[i];
}

/* Teshis icin ARP sonucunu disariya ac */
int arp_test(unsigned char *target_ip, unsigned char *out_mac) {
    return arp_resolve(target_ip, out_mac);
}

/* ─── TCP Bağlantı ───────────────────────────────────────── */
int tcp_connect(unsigned char *dst_ip, unsigned short port) {
    if (!dst_ip || !g_net) return -1;
    g_tcp_seq = 1000; g_tcp_ack = 0; g_tcp_local_port++;
    g_tcp_connected = 0;

    /* Bekleyen/eski paketleri temizle (stale RX flush) */
    {
        static unsigned char _flush[1500]; unsigned int _flen = 0;
        for (int _f = 0; _f < 20; _f++) {
            if (raw_recv(_flush, &_flen) != 0) break;
        }
    }

    /* SYN gönder, SYN-ACK bekle — timeout olursa SYN'i tekrarla (3 deneme) */
    static unsigned char rx[1500]; unsigned int rx_len = 0;
    for (int attempt = 0; attempt < 3; attempt++) {
        if (send_tcp_packet(dst_ip, port, TCP_SYN, NULL, 0) != 0) return -1;

        /* Her denemede 200×10ms = 2 saniye bekle */
        for (int i = 0; i < 200; i++) {
            uefi_stall(10000); /* 10ms */
            if (raw_recv(rx, &rx_len) != 0 || rx_len < 54) continue;

            ip4_hdr_t *rip = (ip4_hdr_t*)(rx + 14);
            if (rip->proto != 6) continue;

            tcp_hdr_t *tcp = (tcp_hdr_t*)(rx + 34);

            /* Bu cevap bizim bağlantımıza mı ait? */
            if (__builtin_bswap16(tcp->dst_port) != g_tcp_local_port) continue;
            if (__builtin_bswap16(tcp->src_port) != port) continue;

            if (tcp->flags & TCP_RST) return -3;

            if ((tcp->flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                g_tcp_seq++;
                g_tcp_ack = __builtin_bswap32(tcp->seq) + 1;
                send_tcp_packet(dst_ip, port, TCP_ACK, NULL, 0);
                for (int j = 0; j < 4; j++) g_tcp_remote_ip[j] = dst_ip[j];
                g_tcp_remote_port = port;
                g_tcp_connected = 1;
                return 0;
            }
        }
        /* Bu denemede SYN-ACK gelmedi, seq/port değiştir ve tekrar dene */
        g_tcp_local_port++;
    }
    return -1;
}

int tcp_disconnect(unsigned char *dst_ip, unsigned short port) {
    int result = send_tcp_packet(dst_ip, port, TCP_FIN | TCP_ACK, NULL, 0);
    g_tcp_connected = 0;
    return result;
}

/* ─── HTTP ───────────────────────────────────────────────── */
static int tcp_recv_response(char *buf, int maxlen) {
    static unsigned char rx[1500]; unsigned int rx_len = 0;
    char header[2048];
    int header_len = 0;
    int total = 0; int header_done = 0;

    if (!buf || maxlen < 2 || !g_tcp_connected) return -1;

    for (int i = 0; i < 500; i++) {
        uefi_stall(10000);
        if (raw_recv(rx, &rx_len) == 0 && rx_len > 54) {
            ip4_hdr_t *rip = (ip4_hdr_t*)(rx + 14);
            if (rip->proto != 6) continue;
            tcp_hdr_t *tcp = (tcp_hdr_t*)(rx + 34);
            if (rip->src[0] != g_tcp_remote_ip[0] || rip->src[1] != g_tcp_remote_ip[1] ||
                rip->src[2] != g_tcp_remote_ip[2] || rip->src[3] != g_tcp_remote_ip[3] ||
                __builtin_bswap16(tcp->src_port) != g_tcp_remote_port ||
                __builtin_bswap16(tcp->dst_port) != g_tcp_local_port) continue;
            if (tcp->flags & TCP_RST) return -3;
            int ip_len = (rip->ver_ihl & 0x0f) * 4;
            int tcp_len = ((tcp->data_offset >> 4) & 0x0f) * 4;
            int data_start = 14 + ip_len + tcp_len;
            if (ip_len < 20 || tcp_len < 20 || data_start > (int)rx_len) continue;
            int data_len = rx_len - data_start;

            if (!header_done) {
                for (int k = 0; k < data_len; k++) {
                    if (header_len >= (int)sizeof(header)) return -4;
                    header[header_len++] = (char)rx[data_start + k];
                    if (header_len >= 4 && header[header_len - 4] == '\r' &&
                        header[header_len - 3] == '\n' && header[header_len - 2] == '\r' &&
                        header[header_len - 1] == '\n') {
                        int body_start = k + 1;
                        int dlen = data_len - body_start;
                        if (total + dlen >= maxlen) return -4;
                        for (int j = 0; j < dlen; j++) {
                            buf[total++] = rx[data_start + body_start + j];
                        }
                        header_done = 1;
                        break;
                    }
                }
            } else {
                if (total + data_len >= maxlen) return -4;
                for (int j = 0; j < data_len; j++) buf[total++] = rx[data_start + j];
            }
            /* ACK gönder */
            if (data_len > 0) {
                g_tcp_ack += data_len;
                if (send_tcp_packet(g_tcp_remote_ip, g_tcp_remote_port, TCP_ACK, NULL, 0) != 0) return -1;
            }

            if (tcp->flags & TCP_FIN) break;
        }
    }
    if (total > 0 && total < maxlen) buf[total] = '\0';
    return total;
}

int http_get(const char *host, const char *path, char *buf, int maxlen) {
    unsigned char dest[4];
    if (parse_host(host, dest) != 0) return -1;
    if (tcp_connect(dest, 80) != 0) return -2;

    char req[512]; int rl = 0;
    const char *m = "GET ", *pr = " HTTP/1.1\r\nHost: ", *en = "\r\nConnection: close\r\n\r\n";
    if (!host || !path || strlen(m) + strlen(path) + strlen(pr) + strlen(host) + strlen(en) >= sizeof(req)) return -4;
    for (const char *p = m; *p; p++) req[rl++] = *p;
    for (const char *p = path; *p; p++) req[rl++] = *p;
    for (const char *p = pr; *p; p++) req[rl++] = *p;
    for (const char *p = host; *p; p++) req[rl++] = *p;
    for (const char *p = en; *p; p++) req[rl++] = *p;
    req[rl] = '\0';

    if (send_tcp_packet(dest, 80, TCP_PSH | TCP_ACK, req, rl) != 0) return -1;
    g_tcp_seq += rl;

    int len = tcp_recv_response(buf, maxlen);
    tcp_disconnect(dest, 80);
    return len;
}

int http_post(const char *host, const char *path, const char *data, int data_len, char *buf, int maxlen) {
    unsigned char dest[4];
    if (parse_host(host, dest) != 0) return -1;
    if (tcp_connect(dest, 80) != 0) return -2;

    if (!host || !path || !data || data_len < 0) return -4;
    char req[1024]; int rl = 0;
    const char *m = "POST ", *pr = " HTTP/1.1\r\nHost: ";
    const char *ct = "\r\nContent-Type: application/x-www-form-urlencoded\r\nContent-Length: ";
    const char *en = "\r\nConnection: close\r\n\r\n";
    if (strlen(m) + strlen(path) + strlen(pr) + strlen(host) + strlen(ct) + strlen(en) + data_len + 16 >= sizeof(req)) return -4;
    for (const char *p = m; *p; p++) req[rl++] = *p;
    for (const char *p = path; *p; p++) req[rl++] = *p;
    for (const char *p = pr; *p; p++) req[rl++] = *p;
    for (const char *p = host; *p; p++) req[rl++] = *p;
    for (const char *p = ct; *p; p++) req[rl++] = *p;

    char num[16]; int np = 0;
    if (data_len == 0) { num[np++] = '0'; }
    else { char tmp[16]; int t = 0; int n = data_len; while (n > 0) { tmp[t++] = '0' + (n % 10); n /= 10; } for (int i = t-1; i >= 0; i--) num[np++] = tmp[i]; }
    for (int i = 0; i < np; i++) req[rl++] = num[i];

    for (const char *p = en; *p; p++) req[rl++] = *p;
    for (int i = 0; i < data_len; i++) req[rl++] = data[i];
    req[rl] = '\0';

    if (send_tcp_packet(dest, 80, TCP_PSH | TCP_ACK, req, rl) != 0) return -1;
    g_tcp_seq += rl;

    int len = tcp_recv_response(buf, maxlen);
    tcp_disconnect(dest, 80);
    return len;
}

int wget(const char *host, const char *http_path, const char *save_path, char *buf, int maxlen) {
    int len = http_get(host, http_path, buf, maxlen);
    if (len <= 0) return len;
    if (fs_write_file(&g_fs, save_path, buf, (size_t)len) < 0) return -4;
    return len;
}

/* Bu hedefte TLS sağlayıcısı ve güvenilir kök CA deposu henüz yoktur.
   HTTPS'i HTTP/80'e düşürmek güvenli değildir; çağırana açık hata verilir. */
int https_get(const char *host, const char *path, char *buf, int maxlen) {
    (void)host; (void)path; (void)buf; (void)maxlen;
    return NETWORK_ERR_TLS_UNAVAILABLE;
}
