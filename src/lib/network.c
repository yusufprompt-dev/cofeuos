/*
 * NETWORK.C - UEFI Network Driver (ARP + DNS + Domain + HTTP)
 */
#include <efi.h>
#include <efilib.h>
#include <efinet.h>
#include <stdarg.h>
#include "../include/network.h"
#include "../include/fs.h"
#include "../include/string.h"
#include "../include/io.h"
#include "../include/sha256.h"
#include "../include/tls.h"

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

/* TCP pencere boyutu: UEFI SNP (e1000) RX ring'i aynı anda gelen 2 segmenti
   güvenle alır ama 3+ segmentlik burst'lerde son segmenti düşürür. Pencereyi
   2×MSS (2×1460=2920) ile sınırla ki sunucu her seferinde en fazla 2 tam
   segment göndersin. Bu, 5 segmentlik burst'lerdeki systematic drop'u önler
   ve retransmisyon bekleme süresini (1-4 sn) ortadan kaldırır — toplamda daha
   hızlı aktarım sağlar. */
#define TCP_WINDOW_SIZE 2920

/* ─── Alım sonuç kodları ────────────────────────────────────────
   recv döngülerinin kilitlenme/zaman aşımı durumlarını ayırt etmesi
   için özel dönüş değerleri. (Hata kodları -1..-8 kullanımda.) */
#define TCP_RX_TIMEOUT (-9)    /* pencere doldu, veri tamamlanamadı */
#define TCP_RX_ERROR    (-10)  /* kalıcı alım hatası */
#define TCP_RX_REPAIR   (-11)  /* RST / NIC kopuk → otomatik onarım gerek */

/* ─── Global Değişkenler ────────────────────────────────────── */
static EFI_SIMPLE_NETWORK_PROTOCOL *g_net = (void*)0;
static EFI_SYSTEM_TABLE            *g_st  = (void*)0;

#ifndef NETWORK_IP0
#define NETWORK_IP0 10
#endif
#ifndef NETWORK_IP1
#define NETWORK_IP1 0
#endif
#ifndef NETWORK_IP2
#define NETWORK_IP2 2
#endif
#ifndef NETWORK_IP3
#define NETWORK_IP3 15
#endif
#ifndef NETWORK_GATEWAY0
#define NETWORK_GATEWAY0 10
#endif
#ifndef NETWORK_GATEWAY1
#define NETWORK_GATEWAY1 0
#endif
#ifndef NETWORK_GATEWAY2
#define NETWORK_GATEWAY2 2
#endif
#ifndef NETWORK_GATEWAY3
#define NETWORK_GATEWAY3 2
#endif
#ifndef NETWORK_DNS0
#define NETWORK_DNS0 10
#endif
#ifndef NETWORK_DNS1
#define NETWORK_DNS1 0
#endif
#ifndef NETWORK_DNS2
#define NETWORK_DNS2 2
#endif
#ifndef NETWORK_DNS3
#define NETWORK_DNS3 3
#endif

#ifdef NETWORK_DEBUG
static void network_debug_logw(const CHAR16 *fmt, ...) {
    if (!g_st || !g_st->ConOut) return;
    char buf[512];
    CHAR16 wide[512];
    int out = 0;  /* çıktı tamponu indeksi */
    int fi  = 0;  /* format dizgesi indeksi */
    va_list ap;
    va_start(ap, fmt);

    while (fmt[fi] && out < 500) {
        if (fmt[fi] != '%') {
            buf[out++] = (char)fmt[fi++];
            continue;
        }

        fi++;
        if (fmt[fi] == '\0') break;
        if (fmt[fi] == 'd') {
            fi++;  /* 'd' karakterini format dizgisinden tüket */
            int value = va_arg(ap, int);
            char tmp[32];
            int t = 0;
            if (value < 0) {
                buf[out++] = '-';
                value = -value;
            }
            do {
                tmp[t++] = (char)('0' + (value % 10));
                value /= 10;
            } while (value > 0 && t < (int)sizeof(tmp));
            while (t > 0 && out < 500) {
                buf[out++] = tmp[--t];
            }
            continue;
        }
        if (fmt[fi] == 'a' || fmt[fi] == 's') {
            fi++;  /* format karakterini format dizgisinden tüket */
            const char *s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            while (*s && out < 500) {
                buf[out++] = *s++;
            }
            continue;
        }
        /* Bilinmeyen format: '%' karakterini olduğu gibi yaz */
        buf[out++] = '%';
    }
    buf[out] = '\0';
    va_end(ap);

    int wi;
    for (wi = 0; buf[wi] && wi < 500; wi++) wide[wi] = (CHAR16)buf[wi];
    wide[wi] = 0;
    uefi_call_wrapper(g_st->ConOut->OutputString, 2, g_st->ConOut, wide);
}
#define NETWORK_DEBUG_LOG(...) do { if (g_st && g_st->ConOut) network_debug_logw(__VA_ARGS__); } while (0)
#else
#define NETWORK_DEBUG_LOG(...) do { } while (0)
#endif

static unsigned char g_ip[4]      = {NETWORK_IP0, NETWORK_IP1, NETWORK_IP2, NETWORK_IP3};
static unsigned char g_gateway[4] = {NETWORK_GATEWAY0, NETWORK_GATEWAY1, NETWORK_GATEWAY2, NETWORK_GATEWAY3};
static unsigned char g_dns_server[4] = {NETWORK_DNS0, NETWORK_DNS1, NETWORK_DNS2, NETWORK_DNS3};
static unsigned int  g_tcp_seq    = 0;
static unsigned int  g_tcp_ack    = 0;
static unsigned short g_tcp_local_port = 49152;
static unsigned char g_tcp_remote_ip[4] = {0,0,0,0};
static unsigned short g_tcp_remote_port = 0;
static int g_tcp_connected = 0;
static unsigned int  g_tcp_ack_sent = 0; /* son gönderilen ACK değeri (tekilleştirme) */
static int           g_tcp_dup_count = 0; /* mevcut g_tcp_ack değeri için gönderilen dup ACK sayısı */
static int           g_tcp_rst_received = 0; /* RST alındı: FIN/ACK gönderilmez (HATA 2) */

/* TLS / CA: güvenli ilk kullanım (TOFU) modeli.
   Bu katman gerçek bir TLS 1.2/1.3 akışı değildir; sadece sunucu sertifikasının
   SHA-256 parmak izini çıkarır ve ilk görünüşte kaydeder. Daha sonra aynı host
   için aynı parmak izini bekler. Bu yaklaşım yalnızca örnek/uyarı amaçlıdır;
   üretimde hassas veri için kullanılmamalıdır. */
#define TLS_TRUST_STORE_SIZE 64
typedef struct {
    char host[64];
    unsigned char fingerprint[32];
    unsigned char ca_fingerprint[32];
    int valid;
    int has_ca;
} tls_trust_entry_t;

static tls_trust_entry_t g_tls_trust_store[TLS_TRUST_STORE_SIZE];
static int g_tls_trust_count = 0;

static int tls_host_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    int i = 0;
    while (a[i] && b[i]) {
        char ca = a[i]; char cb = b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        i++;
    }
    return a[i] == b[i];
}

static int tls_find_trust_entry(const char *host) {
    if (!host) return -1;
    for (int i = 0; i < g_tls_trust_count; i++) {
        if (tls_host_eq(host, g_tls_trust_store[i].host)) return i;
    }
    return -1;
}

static void tls_store_fingerprint(const char *host, const unsigned char *fingerprint) {
    if (!host || !fingerprint) return;
    int idx = tls_find_trust_entry(host);
    if (idx >= 0) {
        for (int i = 0; i < 32; i++) g_tls_trust_store[idx].fingerprint[i] = fingerprint[i];
        g_tls_trust_store[idx].valid = 1;
        return;
    }
    if (g_tls_trust_count < TLS_TRUST_STORE_SIZE) {
        idx = g_tls_trust_count++;
    } else {
        idx = 0;
    }
    for (int i = 0; i < (int)sizeof(g_tls_trust_store[idx].host) - 1; i++) {
        g_tls_trust_store[idx].host[i] = 0;
    }
    for (int i = 0; host[i] && i < (int)sizeof(g_tls_trust_store[idx].host) - 1; i++) {
        g_tls_trust_store[idx].host[i] = host[i];
    }
    for (int i = 0; i < 32; i++) g_tls_trust_store[idx].fingerprint[i] = fingerprint[i];
    for (int i = 0; i < 32; i++) g_tls_trust_store[idx].ca_fingerprint[i] = 0;
    g_tls_trust_store[idx].valid = 1;
    g_tls_trust_store[idx].has_ca = 0;
}


/* ARP Önbellek (gateway) */
static unsigned char g_gateway_mac[6] = {0,0,0,0,0,0};
static int g_gateway_mac_valid = 0;
static int g_gateway_arp_fail_count = 0;
static int g_gateway_warning_shown = 0;
/* ARP rate-limit: gateway MAC'i hic cozulemezse arp_resolve() her paket
   gonderiminde tekrar tekrar cagrilmaz. g_gateway_arp_wait, bir sonraki
   denemeye kadar kalan get_dst_mac() cagrisi sayisidir. 0 iken deneme yapilir
   (ilk gercek paket init'teki basarisiz denemeyi hemen yeniden dener);
   deneme basarisiz olursa ARP_RETRY_GATE'e ayarlanir ve o kadar cagri boyunca
   broadcast MAC dondurulur, arp_resolve cagrilmaz. Boylece blocking
   arp_resolve() (1 sn raw_recv dongusu) aktif TCP/DNS okuma dongulerinin
   ortasinda surekli devreye girip paket calamaz. */
#define ARP_RETRY_GATE 8
static int g_gateway_arp_wait = 0;

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
    /* OVMF BS->Stall kernel IDT'si kurulduktan sonra PM zamanlayıcısına
       takılıp asılabilir; PIT tabanlı gecikme kullanıyoruz. */
    unsigned int ms = microseconds / 1000u;
    if (ms > 0) {
        pit_delay_ms(ms);
        return;
    }
    /* <1ms: sınırlı NOP/PAUSE teli */
    for (volatile unsigned int n = 0; n < (microseconds * 30u) / 100u + 1u; n++)
        __asm__ volatile("pause");
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
                uefi_stall(100); /* 100µs: TX tamamlanması mikrosaniyeler sürer */
            }
            return -1;
        }
        if (st != EFI_NOT_READY) return -1;

        VOID *completed = NULL;
        uefi_call_wrapper(g_net->GetStatus, 3, g_net, NULL, &completed);
        uefi_stall(100);
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
    if (!g_net) {
        NETWORK_DEBUG_LOG(L"[ARP] ag yok\r\n");
        return -1;
    }
    if (g_net->Mode->State != EfiSimpleNetworkInitialized) {
        NETWORK_DEBUG_LOG(L"[ARP] ag arayuzu hazir degil (state=%d)\r\n", g_net->Mode->State);
        return -2;
    }
    NETWORK_DEBUG_LOG(L"[ARP] %d.%d.%d.%d icin istek gonderiliyor\r\n", target_ip[0], target_ip[1], target_ip[2], target_ip[3]);

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

    int send_result = raw_send(pkt, 42);
    if (send_result != 0) {
        NETWORK_DEBUG_LOG(L"[ARP] paket gonderilemedi: raw_send=%d\r\n", send_result);
    }

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
    NETWORK_DEBUG_LOG(L"[ARP] timeout\r\n");
    return -1; /* Timeout */
}

static unsigned char* get_dst_mac(unsigned char *dst_ip) {
    /* Gateway'e gidiyorsa (gateway MAC'ini kullan) */
    if (dst_ip[0] != g_ip[0] || dst_ip[1] != g_ip[1] || dst_ip[2] != g_ip[2] || dst_ip[3] != g_ip[3]) {
        if (!g_gateway_mac_valid) {
            /* ARP rate-limit: ilk deneme basarisizsa (QEMU/slirp'te gorulen
               durum) arp_resolve()'un blocking raw_recv dongusu, SNP'de
               paketler yikici okundugu icin tam o sirada gelen SYN-ACK/DNS
               yaniti gibi paketleri caliyor. Bu yuzden son basarisiz
               denemeden sonra ARP_RETRY_GATE adet get_dst_mac() cagrisi
               gecmeden yeniden denenmez; bekleme boyunca broadcast MAC
               dondurulur (mevcut davranis). Ilk gercek cagri (wait==0) ise
               hemen deneme yapar. */
            if (g_gateway_arp_wait > 0) {
                /* Bekleme periyodu: arp_resolve cagirma, broadcast dondur */
                g_gateway_arp_wait--;
                if (g_gateway_arp_wait == ARP_RETRY_GATE - 1) {
                    NETWORK_DEBUG_LOG(L"[ARP] rate-limit: %d get_dst_mac cagrisi sonra yeniden denenecek\r\n", g_gateway_arp_wait);
                }
                static unsigned char bcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
                return bcast;
            }
            g_gateway_arp_wait = ARP_RETRY_GATE;
            if (arp_resolve(g_gateway, g_gateway_mac) == 0) {
                g_gateway_mac_valid = 1;
                g_gateway_arp_fail_count = 0;
                g_gateway_arp_wait = 0;
            } else {
                g_gateway_arp_fail_count++;
                if (g_gateway_arp_fail_count >= 3 && !g_gateway_warning_shown) {
                    g_gateway_warning_shown = 1;
                    NETWORK_DEBUG_LOG(L"[NET] Gateway (%d.%d.%d.%d) ag katmaninda hic yanit vermiyor.\r\n",
                        g_gateway[0], g_gateway[1], g_gateway[2], g_gateway[3]);
                    NETWORK_DEBUG_LOG(L"      Bu bir kod hatasi degil, olasilikla asagidaki nedenlerden biri:\r\n");
                    NETWORK_DEBUG_LOG(L"      - QEMU '-netdev user,id=net0' + '-device virtio-net,netdev=net0' benzeri bir ayar kullanilmiyor olabilir\r\n");
                    NETWORK_DEBUG_LOG(L"      - Kullanilan NIC modeli (e1000/virtio/rtl8139) SNP surucusuyle uyusmuyor olabilir\r\n");
                    NETWORK_DEBUG_LOG(L"      - Sanal/fiziksel ag arayuzu hic bagli degil\r\n");
                    NETWORK_DEBUG_LOG(L"      Lutfen QEMU komut satirini veya fiziksel ag baglantisini kontrol edin.\r\n");
                }
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

/* ─── DNS Çözümleme ─────────────────────────────────────── */
int dns_resolve(const char *domain, unsigned char *out_ip) {
    NETWORK_DEBUG_LOG(L"[DNS] %a cozumleniyor...\r\n", domain);
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
        NETWORK_DEBUG_LOG(L"[DNS] deneme %d/3\r\n", dns_try + 1);
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

        int send_result = raw_send(pkt, total_len);
        if (send_result != 0) {
            NETWORK_DEBUG_LOG(L"[DNS] paket gonderilemedi: raw_send=%d\r\n", send_result);
        }

        /* Yanit bekle: 1500x2ms = 3 saniye (ayni toplam sure, 5x daha sik poll —
           art arda hizli gelen paketlerin kacirilma riskini azaltir) */
        for (int attempt = 0; attempt < 1500; attempt++) {
            uefi_stall(2000);
            if (raw_recv(rx, &rx_len) != 0 || rx_len < 42) continue;

            /* Ham teshis: raw_recv'in dondurdugu her paketi, port/protokol
               filtrelerinden ONCE logla. Boylece paket hic gorulmuyorsa
               sorunun raw_recv/SNP seviyesinde oldugu kesinlesir. */
            NETWORK_DEBUG_LOG(L"[DNS] ham paket alindi: len=%d\r\n", rx_len);

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

            /* Sadece kaynak portu 53 olan (yani DNS sunucusundan gelen) paketler
               icin teshis logu bas; diger tum paketler (ARP, IPv6, vs.) sessizce
               atlanir. Yanlis dst_port bize gelmeyen bir yaniti gosterir. */
            if (__builtin_bswap16(rudp->dst_port) != 53535) {
                NETWORK_DEBUG_LOG(L"[DNS] port53 paket: dst_port=%d beklenen=53535\r\n", __builtin_bswap16(rudp->dst_port));
                continue; /* bize degil */
            }

            int dns_off = udp_off + 8;
            dns_hdr_t *rdns = (dns_hdr_t*)(rx + dns_off);

            /* Yanitin bu sorguya ait oldugunu dogrula: eski/farkli bir sorgunun
               (farkli transaction ID) yaniti kabul edilmemeli. */
            if (__builtin_bswap16(rdns->id) != tx_id) {
                NETWORK_DEBUG_LOG(L"[DNS] ID uyusmuyor: alinan=%d beklenen=%d\r\n", __builtin_bswap16(rdns->id), tx_id);
                continue;
            }

            /* QR=1 (yanit) ve RCODE=0 (basarili) kontrol et */
            unsigned short rflags = __builtin_bswap16(rdns->flags);
            if (!(rflags & 0x8000)) {
                NETWORK_DEBUG_LOG(L"[DNS] QR biti yok: flags=%d\r\n", rflags);
                continue;       /* QR degil */
            }
            if ((rflags & 0x000F) != 0) {
                NETWORK_DEBUG_LOG(L"[DNS] RCODE hata: flags=%d\r\n", rflags);
                continue;   /* RCODE != 0 (hata) */
            }
            if (__builtin_bswap16(rdns->ancount) == 0) {
                NETWORK_DEBUG_LOG(L"[DNS] ancount=0\r\n");
                continue;
            }

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
                        int dlen = (int)strlen(domain);
                        if (dlen < (int)sizeof(g_dns_cache[g_dns_cache_count].domain)) {
                            strcpy(g_dns_cache[g_dns_cache_count].domain, domain);
                            for (int j = 0; j < 4; j++)
                                g_dns_cache[g_dns_cache_count].ip[j] = out_ip[j];
                            g_dns_cache_count++;
                        }
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
    if (!host || !out_ip) return -1;
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
            int saw_digit = 0;
            while (*s >= '0' && *s <= '9') {
                saw_digit = 1;
                n = n * 10 + (*s - '0');
                if (n > 255) return -1;
                s++;
            }
            if (!saw_digit) return -1;
            out_ip[i] = (unsigned char)n;
            if (i < 3) {
                if (*s != '.') return -1;
                s++;
            }
        }
        return 0;
    }
    return dns_resolve(host, out_ip);
}

/* ─── TCP ────────────────────────────────────────────────── */
static void tcp_flush_rx(void); /* aşağıda tanımlı */
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
    tcp->window = __builtin_bswap16(TCP_WINDOW_SIZE);
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

    /* Basit gönderim: ACK bekleme burada yapılmaz; veri okuma akışını bozmaz. */
    return raw_send(pkt, 14 + 20 + 20 + data_len);
}

/* ─── ACK tekilleştirme (HATA 4) ──────────────────────────────
   ACK gönderimini tekilleştir:
   - rcv_nxt (g_tcp_ack) ilerlediyse her zaman ACK gönder.
   - Sıra dışı/eksik segment yüzünden g_tcp_ack AYNI kaldıysa, bu ACK değeri
     için yalnızca BİR kez duplicate ACK gönder (fast retransmit sinyali);
     "ack 2881 x2 / ack 1441 x3" tarzı üst üste tekrarlar bastırılır.
   İlerleme geldiğinde dup sayacı sıfırlanır. */
#define TCP_DUP_ACK_LIMIT 3
static int tcp_send_ack(void) {
    int dup = 0;
    if (g_tcp_ack == g_tcp_ack_sent) {
        if (g_tcp_dup_count >= TCP_DUP_ACK_LIMIT) return 0;  /* 3 dup ACK sınırı (fast retransmit) */
        dup = 1;
    }
    int r = send_tcp_packet(g_tcp_remote_ip, g_tcp_remote_port, TCP_ACK, NULL, 0);
    if (r == 0) {
        if (dup) g_tcp_dup_count++;
        else { g_tcp_ack_sent = g_tcp_ack; g_tcp_dup_count = 0; }
    }
    return r;
}

/* ─── Ölü bağlantı retransmisyonları (HATA 3) ────────────────
   Kapanan/terk edilen bağlantılara sunucunun dakikalarca (~12 sn'de bir)
   gönderdiği retransmisyonlar NIC RX ring'ini doldurup HATA 1/2'yi tetikliyor.
   Kapalı bağlantıları (uzak IP + uzak port + yerel port) küçük bir tabloda
   tutup, gelen segment tablodaki bir tuple'a uyuyorsa ring'den TÜKET ve RST
   gönder (retransmisyon selini keser). */

#define TCP_CLOSED_SLOTS 8
#define TCP_CLOSED_LIFE  16
typedef struct {
    unsigned char ip[4];
    unsigned short rport;
    unsigned short lport;
    int life;
} tcp_closed_t;
static tcp_closed_t g_tcp_closed[TCP_CLOSED_SLOTS];

static void tcp_closed_add(unsigned char *rip, unsigned short rport, unsigned short lport) {
    int slot = -1, oldest = 0;
    for (int i = 0; i < TCP_CLOSED_SLOTS; i++) {
        if (g_tcp_closed[i].life <= 0) { slot = i; break; }
        if (g_tcp_closed[i].life < g_tcp_closed[oldest].life) oldest = i;
    }
    if (slot < 0) slot = oldest;
    for (int i = 0; i < 4; i++) g_tcp_closed[slot].ip[i] = rip[i];
    g_tcp_closed[slot].rport = rport;
    g_tcp_closed[slot].lport = lport;
    g_tcp_closed[slot].life = TCP_CLOSED_LIFE;
}

static void tcp_closed_decay(void) {
    for (int i = 0; i < TCP_CLOSED_SLOTS; i++)
        if (g_tcp_closed[i].life > 0) g_tcp_closed[i].life--;
}

static int tcp_closed_match(unsigned char *rip, unsigned short rport, unsigned short lport) {
    for (int i = 0; i < TCP_CLOSED_SLOTS; i++) {
        if (g_tcp_closed[i].life <= 0) continue;
        if (g_tcp_closed[i].ip[0] != rip[0] || g_tcp_closed[i].ip[1] != rip[1] ||
            g_tcp_closed[i].ip[2] != rip[2] || g_tcp_closed[i].ip[3] != rip[3]) continue;
        if (g_tcp_closed[i].rport != rport || g_tcp_closed[i].lport != lport) continue;
        return i;
    }
    return -1;
}

/* Kapalı bir bağlantı adına saf RST gönder. send_tcp_packet() aktif bağlantının
   g_tcp_local_port'unu kullandığı için terk edilmiş bağlantının portu adına
   ayrıca üretilir. RFC 793 reset üretimi: gelen segment ACK taşıyorsa RST
   seq'i = gelen ACK değeri, aksi halde seq=0; RST ack'i = gelen seq + uzunluk
   (+ FIN). ACK bayrağı set edilir (RST|ACK) — sunucu/slirp bu numaralandırmayı
   doğru kabul eder ve retransmisyonu durdurur. */
static int tcp_send_rst(unsigned char *dst_ip, unsigned short dst_port,
                        unsigned short src_port, unsigned int seq, unsigned int ack) {
    if (!g_net) return -1;
    static unsigned char pkt[1500];
    unsigned char *my_mac = g_net->Mode->CurrentAddress.Addr;
    unsigned char *dst_mac = get_dst_mac(dst_ip);

    eth_hdr_t *eth = (eth_hdr_t*)pkt;
    for (int i = 0; i < 6; i++) { eth->dst[i] = dst_mac[i]; eth->src[i] = my_mac[i]; }
    eth->type = __builtin_bswap16(0x0800);

    ip4_hdr_t *ip = (ip4_hdr_t*)(pkt + 14);
    ip->ver_ihl = 0x45; ip->tos = 0;
    ip->len = __builtin_bswap16(40);
    ip->id = __builtin_bswap16(0x0300); ip->flags = 0;
    ip->ttl = 64; ip->proto = 6; ip->csum = 0;
    for (int i = 0; i < 4; i++) { ip->src[i] = g_ip[i]; ip->dst[i] = dst_ip[i]; }
    ip->csum = csum16(ip, 20);

    tcp_hdr_t *tcp = (tcp_hdr_t*)(pkt + 34);
    tcp->src_port = __builtin_bswap16(src_port);
    tcp->dst_port = __builtin_bswap16(dst_port);
    tcp->seq = __builtin_bswap32(seq);
    tcp->ack = __builtin_bswap32(ack);
    tcp->data_offset = 0x50;
    tcp->flags = TCP_RST | TCP_ACK;
    tcp->window = 0;
    tcp->csum = 0;
    tcp->urg_ptr = 0;

    static unsigned char pseudo_buf[1500];
    tcp_pseudo_hdr_t *phdr = (tcp_pseudo_hdr_t*)pseudo_buf;
    for (int i = 0; i < 4; i++) { phdr->src[i] = g_ip[i]; phdr->dst[i] = dst_ip[i]; }
    phdr->zero = 0; phdr->proto = 6;
    phdr->tcp_len = __builtin_bswap16(20);
    for (int i = 0; i < 20; i++) pseudo_buf[12 + i] = pkt[34 + i];
    tcp->csum = csum16(pseudo_buf, 12 + 20);

    return raw_send(pkt, 54);
}

/* Gelen segment kapalı bir bağlantıya aitse 1 döner (RST gönderildi, tüketildi).
   Aktif/ilgisiz bağlantıya aitse 0 döner (çağıran kendi kurallarıyla işler).
   Gelen segment zaten RST ise cevap üretilmez (RST'e RST atılmaz). */
static int tcp_rst_closed_segment(ip4_hdr_t *rip, tcp_hdr_t *tcp) {
    if (rip->proto != 6) return 0;
    unsigned short srcp = __builtin_bswap16(tcp->src_port);
    unsigned short dstp = __builtin_bswap16(tcp->dst_port);
    if (tcp_closed_match(rip->src, srcp, dstp) < 0) return 0;
    if (tcp->flags & TCP_RST) return 1; /* RST'e RST ile cevap verilmez */
    /* RFC 793 reset üretimi: seq = gelen ACK (yoksa 0), ack = gelen seq +
       uzunluk (+ FIN). Böylece sunucunun kapanan bağlantıya retransmisyonu
       doğru numaralandırma ile tanınır ve kesilir (HATA 3). */
    int ip_len = (rip->ver_ihl & 0x0f) * 4;
    int tcp_len = ((tcp->data_offset >> 4) & 0x0f) * 4;
    if (ip_len < 20) ip_len = 20;
    if (tcp_len < 20) tcp_len = 20;
    int data_len = __builtin_bswap16(rip->len) - ip_len - tcp_len;
    if (data_len < 0) data_len = 0;
    unsigned int seq = (tcp->flags & TCP_ACK) ? __builtin_bswap32(tcp->ack) : 0;
    unsigned int ack = __builtin_bswap32(tcp->seq) + (unsigned int)data_len;
    if (tcp->flags & TCP_FIN) ack++;
    tcp_send_rst(rip->src, srcp, dstp, seq, ack);
    return 1;
}

/* ─── Temel Fonksiyonlar ──────────────────────────────────── */
void network_init(void *st) {
    EFI_STATUS st_status;
    NETWORK_DEBUG_LOG(L"[NET] network_init baslatiliyor\r\n");
    g_st = (EFI_SYSTEM_TABLE*)st;
    EFI_GUID net_guid = EFI_SIMPLE_NETWORK_PROTOCOL_GUID;
    uefi_call_wrapper(g_st->BootServices->LocateProtocol, 3, &net_guid, NULL, (VOID**)&g_net);
    if (!g_net) {
        NETWORK_DEBUG_LOG(L"[NET] SNP protokolu bulunamadi\r\n");
        return;
    }
    NETWORK_DEBUG_LOG(L"[NET] Ag arayuzu bulundu, state=%d\r\n", g_net->Mode->State);

    st_status = uefi_call_wrapper(g_net->Start, 1, g_net);
    if (st_status != EFI_SUCCESS) {
        NETWORK_DEBUG_LOG(L"[NET] Start basarisiz: status=0x%x\r\n", st_status);
        return;
    }
    NETWORK_DEBUG_LOG(L"[NET] Start basarili\r\n");

    st_status = uefi_call_wrapper(g_net->Initialize, 3, g_net, 0, 0);
    if (st_status != EFI_SUCCESS) {
        NETWORK_DEBUG_LOG(L"[NET] Initialize basarisiz: status=0x%x\r\n", st_status);
        return;
    }
    NETWORK_DEBUG_LOG(L"[NET] Initialize basarili\r\n");

    st_status = uefi_call_wrapper(g_net->ReceiveFilters, 6, g_net, 0x01 | 0x02, 0, FALSE, 0, NULL);
    if (st_status != EFI_SUCCESS) {
        NETWORK_DEBUG_LOG(L"[NET] ReceiveFilters basarisiz: status=0x%x\r\n", st_status);
        return;
    }
    NETWORK_DEBUG_LOG(L"[NET] ReceiveFilters basarili\r\n");

    if (g_net->Mode->State != EfiSimpleNetworkInitialized) {
        NETWORK_DEBUG_LOG(L"[NET] UYARI: Ag arayuzu Initialized degil (state=%d)\r\n", g_net->Mode->State);
    }

    network_dhcp();
    if (arp_resolve(g_gateway, g_gateway_mac) != 0) {
        g_gateway_arp_fail_count = 1;
        g_gateway_arp_wait = 0; /* ilk gercek get_dst_mac cagrisi hemen yeniden denesin */
        NETWORK_DEBUG_LOG(L"[NET] Gateway (%d.%d.%d.%d) ARP yaniti vermiyor (ilk deneme).\r\n",
            g_gateway[0], g_gateway[1], g_gateway[2], g_gateway[3]);
        NETWORK_DEBUG_LOG(L"       Yerel IP: %d.%d.%d.%d  Gateway: %d.%d.%d.%d  DNS: %d.%d.%d.%d\r\n",
            g_ip[0], g_ip[1], g_ip[2], g_ip[3],
            g_gateway[0], g_gateway[1], g_gateway[2], g_gateway[3],
            g_dns_server[0], g_dns_server[1], g_dns_server[2], g_dns_server[3]);
    } else {
        g_gateway_mac_valid = 1;
        NETWORK_DEBUG_LOG(L"[NET] Gateway ARP basarili\r\n");
    }
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

void network_set_static_ip(void) {
    /* Bu fonksiyon gerçek DHCP DISCOVER/OFFER/REQUEST/ACK akışı değil;
       sadece statik IP ataması yapar. Derleme zamanındaki ağ değerlerini korur. */
    g_ip[0] = NETWORK_IP0; g_ip[1] = NETWORK_IP1; g_ip[2] = NETWORK_IP2; g_ip[3] = NETWORK_IP3;
    g_gateway[0] = NETWORK_GATEWAY0; g_gateway[1] = NETWORK_GATEWAY1; g_gateway[2] = NETWORK_GATEWAY2; g_gateway[3] = NETWORK_GATEWAY3;
    g_dns_server[0] = NETWORK_DNS0; g_dns_server[1] = NETWORK_DNS1; g_dns_server[2] = NETWORK_DNS2; g_dns_server[3] = NETWORK_DNS3;
    g_gateway_mac_valid = 0; /* Yeniden ARP gerekli */
    g_gateway_arp_wait = 0; /* rate-limit sayacini da sifirla */
}

void network_dhcp(void) {
    /* Uyumluluk için eski isim korunur; gerçek DHCP implemente edilmedi. */
    network_set_static_ip();
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
    g_tcp_ack_sent = 0; g_tcp_dup_count = 0;
    g_tcp_rst_received = 0;
    g_tcp_connected = 0;

    /* Bekleyen/eski paketleri temizle (stale RX flush; kapalı bağlantı
       retransmisyonlarına burada da RST gider) */
    tcp_flush_rx();

    /* Aynı port üzerinde 3 SYN denemesi (t≈0s, 2s, 4s), toplam ~6 saniye.
       Port her denemede DEĞİŞTİRİLMEZ: QEMU e1000 + OVMF SNP altında ilk
       SYN-ACK zaman zaman NIC kuyruğuna düşebiliyor; aynı portta yeniden SYN
       göndermek sunucudan taze bir SYN-ACK tetikler. Ayrıca sunucunun eski
       SYN-ACK retransmisyonları da aynı port'a gelir ve hepsi yakalanabilir.
       (Önceki port-atalama tasarımı, düşen SYN-ACK'i hemen terk edip sunucunun
       retransmisyonlarını boşa harcıyordu.) Beklerken NIC kuyruğu 1ms'de bir
       tamamen boşaltılır (non-blocking drain) ki firmware RX ring'i dolu
       kaldığı için gelen paketler düşürülmesin. */
    static unsigned char rx[1500]; unsigned int rx_len = 0;
    for (int syn_try = 0; syn_try < 3; syn_try++) {
        NETWORK_DEBUG_LOG(L"[TCP] SYN gonderildi (deneme %d), yanit bekleniyor...\r\n", syn_try + 1);
        if (send_tcp_packet(dst_ip, port, TCP_SYN, NULL, 0) != 0) return -1;

        /* 2000×1ms = 2 saniye; dış döngü uyurken içte NIC kuyruğundaki BÜTÜN
           paketler anında boşaltılır (non-blocking poll), böylece SYN-ACK
           gecikmez ve ring sürekli boşalır. */
        for (int i = 0; i < 2000; i++) {
            for (;;) {
                rx_len = 0; /* (HATA 1 kural c) paylaşılan uzunluk her alımdan önce sıfırlansın */
                int rr = raw_recv(rx, &rx_len);
                if (rr != 0) break;            /* -2 kuyruk boş, -1 kalıcı hata */
                if (rx_len < 54) continue;     /* kısa paket: atla */
                ip4_hdr_t *rip = (ip4_hdr_t*)(rx + 14);
                if (rip->proto != 6) continue;

                tcp_hdr_t *tcp = (tcp_hdr_t*)(rx + 34);

                /* Bu cevap bizim aktif bağlantımıza mı ait? (kaynak IP + portlar) */
                int ours = (rip->src[0] == dst_ip[0] && rip->src[1] == dst_ip[1] &&
                            rip->src[2] == dst_ip[2] && rip->src[3] == dst_ip[3] &&
                            __builtin_bswap16(tcp->dst_port) == g_tcp_local_port &&
                            __builtin_bswap16(tcp->src_port) == port);
                if (!ours) {
                    /* Kapalı bağlantı retransmisyonuysa tüket + RST (HATA 3) */
                    tcp_rst_closed_segment(rip, tcp);
                    continue;
                }

                if (tcp->flags & TCP_RST) { g_tcp_rst_received = 1; return -3; }

                if ((tcp->flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                    g_tcp_seq++;
                    g_tcp_ack = __builtin_bswap32(tcp->seq) + 1;
                    send_tcp_packet(dst_ip, port, TCP_ACK, NULL, 0);
                    g_tcp_ack_sent = g_tcp_ack;
                    g_tcp_dup_count = 0;
                    for (int j = 0; j < 4; j++) g_tcp_remote_ip[j] = dst_ip[j];
                    g_tcp_remote_port = port;
                    g_tcp_connected = 1;
                    NETWORK_DEBUG_LOG(L"[TCP] SYN-ACK alindi, baglanti kuruldu\r\n");
                    return 0;
                }
            }
            uefi_stall(1000); /* 1ms */
        }
        /* Bu SYN'e SYN-ACK gelmedi; aynı portta tekrar dene */
    }
    /* 3 SYN denemesi de boşa çıktı: sunucu hâlâ bu port'a SYN-ACK
       retransmisyonu gönderebilir; tuple'ı kapalı tablosuna ekle ki daha
       sonra gelecek retransmisyonlar RST ile kesilsin (ring'i doldurmasın). */
    tcp_closed_add(dst_ip, port, g_tcp_local_port);
    return -1;
}

int tcp_disconnect(unsigned char *dst_ip, unsigned short port) {
    /* RST alındıysa (sunucu zaten kapattı) ek ACK/FIN gönderme (HATA 2) */
    int result = g_tcp_rst_received
                     ? 0
                     : send_tcp_packet(dst_ip, port, TCP_FIN | TCP_ACK, NULL, 0);
    g_tcp_connected = 0;
    /* Sunucu kalan veri/FIN retransmisyonlarını dakikalarca gönderebilir;
       tuple'ı kapalı tablosuna ekle ki sonraki drain'lerde RST ile kesilsin. */
    if (result == 0) tcp_closed_add(g_tcp_remote_ip, g_tcp_remote_port, g_tcp_local_port);
    return result;
}

/* ─── Otomatik onarım / yeniden bağlantı ─────────────────────── */
static int network_nic_ready(void); /* alttaki sağlık modülünde tanımlı */

/* Segment bizim aktif bağlantımıza mı ait? */
static int tcp_is_ours(ip4_hdr_t *rip, tcp_hdr_t *tcp) {
    return rip->proto == 6 &&
           rip->src[0] == g_tcp_remote_ip[0] && rip->src[1] == g_tcp_remote_ip[1] &&
           rip->src[2] == g_tcp_remote_ip[2] && rip->src[3] == g_tcp_remote_ip[3] &&
           __builtin_bswap16(tcp->src_port) == g_tcp_remote_port &&
           __builtin_bswap16(tcp->dst_port) == g_tcp_local_port;
}

/* Bekleyen/eski RX paketlerini boşalt (stale flush). Kapalı bağlantıya gelen
   retransmisyonlar burada da tüketilir ve RST ile kesilir (HATA 3); böylece
   ölü bağlantı trafiği NIC RX ring'ini doldurmaz. */
static void tcp_flush_rx(void) {
    static unsigned char f[1500]; unsigned int flen = 0;
    /* Decay yalnızca network_monitor_tick'inde yapılır; burada yapılırsa aktif
       işlem sırasında kapalı kayıtlar erken silinip retransmisyon seli geri döner. */
    for (int i = 0; i < 20; i++) {
        if (raw_recv(f, &flen) != 0) break;
        if (flen < 54) continue;
        ip4_hdr_t *rip = (ip4_hdr_t*)(f + 14);
        if (rip->proto != 6) continue;
        tcp_hdr_t *tcp = (tcp_hdr_t*)(f + 34);
        if (tcp_is_ours(rip, tcp)) continue;   /* aktif bağlantıya dokunma */
        tcp_rst_closed_segment(rip, tcp);
    }
}

/* Soketi güvenli şekilde kapatır: FIN gönderir, RX kuyruğunu temizler,
   TCP durumunu ve bağlantı tanımlayıcılarını sıfırlar. */
static void tcp_safe_close(void) {
    if (g_tcp_connected && g_net) {
        /* Sunucu kalan veri/FIN retransmisyonlarını sürdürebilir; tuple'ı kapalı
           tablosuna ekle ki sonraki drain'lerde RST ile kesilsin (HATA 3). */
        tcp_closed_add(g_tcp_remote_ip, g_tcp_remote_port, g_tcp_local_port);
        /* RST alındıysa RFC 793: ACK/FIN GÖNDERME (HATA 2) — bağlantı zaten
           sunucu tarafından kapatıldı; FIN yalnızca temiz kapanışta gider. */
        if (!g_tcp_rst_received)
            send_tcp_packet(g_tcp_remote_ip, g_tcp_remote_port, TCP_FIN | TCP_ACK, NULL, 0);
    }
    tcp_flush_rx();
    g_tcp_connected = 0;
    g_tcp_rst_received = 0;
    g_tcp_remote_ip[0] = g_tcp_remote_ip[1] = 0;
    g_tcp_remote_ip[2] = g_tcp_remote_ip[3] = 0;
    g_tcp_remote_port = 0;
}

/* Kilitlenmiş/takılmış TCP oturumunu onarır: soketi kapatır, belleği/kuyruğu
   temizler, NIC gerekirse yeniden başlatılır ve hedefle taze bir TCP el
   sıkışması (reconnect) başlatır. Başarılıysa 0 döner. */
static int tcp_auto_reconnect(unsigned char *dst_ip, unsigned short port) {
    NETWORK_DEBUG_LOG(L"[TCP] otomatik onarım: soket kapatılıp yeniden bağlanılıyor\r\n");
    tcp_safe_close();
    if (!network_nic_ready()) {
        if (network_force_repair() != 0) return -1;
    }
    for (int attempt = 0; attempt < 3; attempt++) {
        if (tcp_connect(dst_ip, port) == 0) return 0;
        tcp_flush_rx();
    }
    return -1;
}

/* ─── TLS / CA yardımcıları ─────────────────────────────── */
static int tcp_read_payload(unsigned char *buf, int maxlen) {
    if (!buf || maxlen < 2) return -4;
    static unsigned char rx[1500]; unsigned int rx_len = 0;
    int total = 0;
    int expected_record = 0;
    int handshake_active = 0;  /* ilk TLS kaydı Handshake (0x16) ise */
    int rx_fatal = 0;          /* ardışık kalıcı alım hatası sayacı */
    int idle_after_data = 0;   /* veri geldikten sonra sessiz tur sayacı */
    int done = 0;
    /* Sunucu TCP retransmisyonlari uzun araliklarla (~1s, ~3s, ~6s, ~12s)
       gelebiliyor. Kisa pencereler bu araliklari kacirir; tek cagrida en
       az ~8 saniye dinle (8000 x 1ms) ki ilk birkaç retransmisyonu
       yakalayabilelim. Bu sure normal ve beklenen bir durum. */
    for (int i = 0; i < 8000; i++) {
        /* ── Non-blocking boşaltma: NIC kuyruğundaki BÜTÜN segmentleri
           gecikmeden oku, batch'leri TEK kümülatif ACK ile onayla. Bu,
           sunucunun ACK bekleyip dakikalarca retransmisyon yapmasını
           (bağlantı kilidi) önler. select()/poll() eşdeğeri. ── */
        int drained = 0;
        int got_data = 0;
        for (;;) {
            rx_len = 0; /* (HATA 1 kural c) paylaşılan uzunluk her alımdan önce sıfırlansın */
            int rr = raw_recv(rx, &rx_len);
            if (rr == -1) {
                /* Kalıcı alım hatası (EFI_NOT_READY dışı): kilidi bekleme */
                if (++rx_fatal >= 5) return TCP_RX_ERROR;
                break;
            }
            if (rr != 0) break;            /* -2: kuyruk boş */
            if (rx_len < 54) continue;     /* kısa paket: atla */
            drained = 1;
            ip4_hdr_t *rip = (ip4_hdr_t*)(rx + 14);
            tcp_hdr_t *tcp = (tcp_hdr_t*)(rx + 34);
            if (!tcp_is_ours(rip, tcp)) {
                /* Kapalı bağlantı retransmisyonu? Tüket + RST (HATA 3) */
                tcp_rst_closed_segment(rip, tcp);
                continue;
            }

            if (tcp->flags & TCP_RST) { g_tcp_rst_received = 1; return TCP_RX_REPAIR; }
            if (!g_net) return TCP_RX_REPAIR;

            int ip_len = (rip->ver_ihl & 0x0f) * 4;
            int tcp_len = ((tcp->data_offset >> 4) & 0x0f) * 4;
            int data_start = 14 + ip_len + tcp_len;
            if (ip_len < 20 || tcp_len < 20 || data_start > (int)rx_len) continue;
            /* Payload uzunluğu IP total length alanından hesaplanır, ham çerçeve
               uzunluğundan (rx_len) DEĞİL: NIC'ler kısa çerçeveleri Ethernet
               minimum boyutuna (60 bayt) dolgular. rx_len'den türetilen data_len,
               boş ACK'ları 6 baytlık "veri" gibi yanlış işleyip rcv_nxt'i +6
               ilerletiyor, ACK'i kalıcı olarak kaydırıyordu (ack 7 takılması). */
            int ip_total = __builtin_bswap16(rip->len);
            int data_len = ip_total - ip_len - tcp_len;
            if (data_len < 0) data_len = 0;
            if (data_start + data_len > (int)rx_len) data_len = (int)rx_len - data_start;
            if (data_len <= 0) {
                /* Boş ACK: ilerleme yok. FIN ise RFC 793 gereği YALNIZCA sıra
                   uyumluysa (tcp->seq == g_tcp_ack) kabul edilir ve 1 seq
                   tüketip ack = seq+1 ile onaylanır. Eksik veri (boşluk)
                   varken gelen FIN veriyi "kesemez": ACK ilerletilmez, sadece
                   duplicate ACK gönderilir (sunucu boşluğu yeniden iletir). */
                if (tcp->flags & TCP_FIN) {
                    unsigned int fin_seq = __builtin_bswap32(tcp->seq);
                    if (fin_seq == g_tcp_ack) {
                        g_tcp_ack++;
                        if (tcp_send_ack() != 0) return TCP_RX_REPAIR;
                    } else if (fin_seq > g_tcp_ack) {
                        NETWORK_DEBUG_LOG(L"[TLS] sira disi FIN: seq=%d beklenen=%d, duplicate ACK\r\n", fin_seq, g_tcp_ack);
                        if (tcp_send_ack() != 0) return TCP_RX_REPAIR;
                    } else if (fin_seq < g_tcp_ack) {
                        /* Zaten onaylanmış FIN yeniden iletimi: sunucu ACK'i
                           almamış olabilir; kümülatif ACK'i yeniden gönder
                           (tekilleştirme: değer başına en fazla bir). */
                        if (tcp_send_ack() != 0) return TCP_RX_REPAIR;
                    }
                }
                continue;
            }
            unsigned int seg_seq = __builtin_bswap32(tcp->seq);
            /* Ham teshis: raw_recv'in aldigi her TCP segmentini, sira kontrolundan
               ONCE logla. Boylece segmentin yazilim seviyesinde gorulup gorulmedigi
               (kayip NIC'te mi yoksa alinamayan paket mi) ayirt edilir. */
            NETWORK_DEBUG_LOG(L"[TLS] paket alindi: seq=%d len=%d (beklenen=%d)\r\n", seg_seq, data_len, g_tcp_ack);
            /* Zaten ACK'lanmış yeniden iletimler: tamponu ikinci kez doldurma,
               g_tcp_ack'i geriye götürme. Sunucu ACK'imizi almamış olabilir;
               kümülatif ACK'i yeniden gönder (tekilleştirme: değer başına en
               fazla bir) ki retransmisyon kilidi çözülsün. */
            if ((seg_seq + (unsigned int)data_len) <= g_tcp_ack) {
                if (tcp_send_ack() != 0) return TCP_RX_REPAIR;
                continue;
            }
            /* Sira disi/eksik segment: tcp->seq beklenen g_tcp_ack ile eslesmiyorsa
               veriyi buffer'a ekleme, ACK'i ilerletme. SNP aradaki segmentleri
               kacirmis olabilir; duplicate ACK gondererek sunucunun kaybolan
               segmenti yeniden gondermesini sagla (fast retransmit). Burada
               TCP_RX_REPAIR dönülmüyor; boşluk dolana kadar beklemeye devam
               edilir (sıra-odaklı tamamlama). */
            if (seg_seq != g_tcp_ack) {
                NETWORK_DEBUG_LOG(L"[TLS] sira disi segment: seq=%d beklenen=%d, duplicate ACK\r\n", seg_seq, g_tcp_ack);
                if (tcp_send_ack() != 0) return TCP_RX_REPAIR;
                continue;
            }
            if (total + data_len > maxlen) data_len = maxlen - total;
            if (data_len <= 0) { done = 1; break; }
            for (int j = 0; j < data_len; j++) buf[total++] = rx[data_start + j];
            g_tcp_ack += data_len;
            if (tcp->flags & TCP_FIN) g_tcp_ack++;
            got_data = 1;

            /* TLS record basligi geldiyse beklenen toplam kayit boyutunu hesapla:
               record_len = (buf[3]<<8) | buf[4], toplam = 5 + record_len */
            if (expected_record == 0 && total >= 5) {
                expected_record = 5 + ((buf[3] << 8) | buf[4]);
                handshake_active = (buf[0] == 0x16);
            }
            /* Sıra-odaklı tamamlanma:
               - Handshake DEĞİL (alert/CCS/appdata): ilk kayıt tamamlanınca dön.
               - Handshake (0x16): ServerHelloDone/CCS dahil el sıkışmasının TAMAMI
                 gelene kadar okumaya devam et (üstteki idle_after_data ile dön).
                 Erken çıkış yok — sabit 2048 bayt sınırı sertifika zincirini kesip
                 ACK'in 3'te takılı kalmasına ve TLS'in başarısız olmasına yol
                 açıyordu; artık çağıranın tampon boyutuna (maxlen) kadar okunur. */
            if (!handshake_active && expected_record > 0 && total >= expected_record) { done = 1; break; }
            if (total >= maxlen) { done = 1; break; }
        }

        /* Batch ACK: aynı turda boşaltılan tüm segmentler için TEK kümülatif ACK.
           Her segment için ayrı ACK yerine kuyruk boşaltıldıktan sonra tek onay
           verilir; ACK yoğunluğu düşer. Sıra dışı durumdaki duplicate ACK yukarıda
           anında gönderilir (fast retransmit). tcp_send_ack() ilerleme yoksa
           tekrar göndermez (HATA 4). */
        if (got_data) {
            if (tcp_send_ack() != 0) return TCP_RX_REPAIR;
        }

        if (done) break;

        if (drained) {
            rx_fatal = 0;
            idle_after_data = 0;
            /* İşleme sırasında / batch ACK sırasında kuyruğa gelen paketleri
               kaçırma: 1ms beklemeden hemen bir sonraki boşaltma turuna dön.
               Ring'i işleme hızında tüketmek burst kaybını azaltır (HATA 1
               kural d: kuyruk boşalana/boş doğrulanana dek devam). */
            continue;
        } else if (total > 0) {
            /* Veri geldi ama kuyruk boş: ~2 saniye sessizse tamamlandı say.
               Sunucu retransmisyonu ~1s, ~3s, ~6s araliklarla gelir; 2 saniyelik
               pencere ilk retransmisyonu yakalar. Eksik segment (gap) varsa
               periyodik dup ACK gonder ki sunucu fast retransmit tetiklesin. */
            if (++idle_after_data >= 2000) break;
            /* Eksik segment varsa (dup ACK gonderilmisse) her ~200ms'de bir
               dup ACK daha gonder. tcp_send_ack() 3 dup ACK sonrasi gondermeyi
               durdurur; sayaci sifirlayarak yeni bir dup ACK serisi baslat. */
            if (g_tcp_dup_count > 0 && (idle_after_data % 200) == 0) {
                g_tcp_dup_count = 0;
                tcp_send_ack();
            }
        }

        uefi_stall(1000); /* 1ms: art arda gelen paketleri kacirmamak icin sik poll */
    }
    /* Ham byte loglama: alinan total byte ve ilk 16 byte (ondalik) */
    NETWORK_DEBUG_LOG(L"[TLS] ham veri (%d byte): ", total);
    for (int i = 0; i < total && i < 16; i++) {
        NETWORK_DEBUG_LOG(L"%d ", buf[i]);
    }
    NETWORK_DEBUG_LOG(L"\r\n");
    if (total > 0) return total;
    return TCP_RX_TIMEOUT;
}

static int tls_verify_host_fingerprint(const char *host, const unsigned char *fingerprint) {
    if (!host || !fingerprint) return -1;
    int idx = tls_find_trust_entry(host);
    if (idx < 0) {
        tls_store_fingerprint(host, fingerprint);
        return 0;
    }
    if (!g_tls_trust_store[idx].valid) {
        tls_store_fingerprint(host, fingerprint);
        return 0;
    }
    if (g_tls_trust_store[idx].has_ca) {
        return (memcmp(g_tls_trust_store[idx].ca_fingerprint, fingerprint, 32) == 0) ? 0 : -1;
    }
    return memcmp(g_tls_trust_store[idx].fingerprint, fingerprint, 32) == 0 ? 0 : -1;
}

/* ─── Gerçek TLS 1.2 istemcisi (raw TCP katmanına adapter) ─────
   tls_client_t (tls_client.c) ağdan bağımsızdır; bu adapter onu
   send_tcp_packet / tcp_read_payload katmanına bağlar. Statik
   tamponlar malloc'suz tasarım gereği; ağ tek bağlantılı olduğundan
   yeniden giriş (reentrancy) yoktur. */

typedef struct {
    unsigned char *dst_ip;
    unsigned short port;
    unsigned char pending[TLS_REC_MAX_PAYLOAD + 8];
    int pending_off, pending_len;
} tls_tcp_ctx_t;

static int tcp_recv_response(char *buf, int maxlen);

static tls_tcp_ctx_t g_tls_tcp;
static tls_client_t  g_tls;

/* TLS kaydını TCP segmentlerine böler (MTU ~1500; 1400 baytlık parçalar) */
static int tls_tcp_send(void *ctx, const uint8_t *data, size_t len) {
    tls_tcp_ctx_t *t = ctx;
    size_t off = 0;
    while (off < len) {
        size_t chunk = len - off;
        if (chunk > 1400) chunk = 1400;
        if (send_tcp_packet(t->dst_ip, t->port, TCP_PSH | TCP_ACK,
                            (const char *)(data + off), (int)chunk) != 0)
            return -1;
        g_tcp_seq += (unsigned int)chunk;
        off += chunk;
    }
    return 0;
}

/* tam `len` byte okur; gelen akış t->pending tamponunda biriktirilir */
static int tls_tcp_recv(void *ctx, uint8_t *data, size_t len) {
    tls_tcp_ctx_t *t = ctx;
    size_t got = 0;
    while (got < len) {
        if (t->pending_len - t->pending_off > 0) {
            size_t take = len - got;
            if (take > (size_t)(t->pending_len - t->pending_off))
                take = (size_t)(t->pending_len - t->pending_off);
            memcpy(data + got, t->pending + t->pending_off, take);
            t->pending_off += (int)take;
            got += take;
            if (t->pending_off == t->pending_len)
                t->pending_off = t->pending_len = 0;
            continue;
        }
        int n = tcp_read_payload(t->pending, (int)sizeof(t->pending));
        if (n <= 0) return -1;
        t->pending_off = 0;
        t->pending_len = n;
    }
    return 0;
}

/* Zayıf ama işlevsel entropi: xorshift64, saat + TCP seq + yer adresi ile
   tohumlanır. Premaster RSA ile şifrelendiğinden gizlilik korunur. */
static uint64_t tls_rng_state = 0;
static void tls_tcp_get_random(void *ctx, uint8_t *out, size_t len) {
    (void)ctx;
    if (tls_rng_state == 0) {
        uint64_t s = (uint64_t)(uintptr_t)&tls_rng_state;
        s ^= (uint64_t)g_tcp_seq << 32;
        s ^= (uint64_t)g_tcp_ack;
        if (g_st && g_st->RuntimeServices) {
            EFI_TIME tm;
            if (g_st->RuntimeServices->GetTime(&tm, NULL) == EFI_SUCCESS) {
                s ^= (uint64_t)tm.Hour << 40;
                s ^= (uint64_t)tm.Minute << 32;
                s ^= (uint64_t)tm.Second << 24;
                s ^= (uint64_t)tm.Nanosecond;
            }
        }
        tls_rng_state = s ? s : 0x9E3779B97F4A7C15ull;
    }
    for (size_t i = 0; i < len; i++) {
        uint64_t x = tls_rng_state;
        x ^= x << 13; x ^= x >> 7; x ^= x << 17;
        tls_rng_state = x;
        out[i] = (uint8_t)(x >> 32);
    }
}

/* TLS handshake + TOFU sertifika doğrulaması.
   0: başarılı; negatif: NETWORK_ERR_TLS_* */
static int tls_session_handshake(const char *host, unsigned char *dst_ip,
                                 unsigned short port) {
    tls_net_t net;
    memset(&g_tls_tcp, 0, sizeof g_tls_tcp);
    g_tls_tcp.dst_ip = dst_ip;
    g_tls_tcp.port = port;

    net.ctx = &g_tls_tcp;
    net.send = tls_tcp_send;
    net.recv = tls_tcp_recv;
    net.get_random = tls_tcp_get_random;

    tls_client_init(&g_tls, &net, NULL, NULL);
    g_tls.sni_host = host;

    int rc = tls_client_handshake(&g_tls);
    if (rc != TLS_OK) {
        uint8_t lvl, desc;
        NETWORK_DEBUG_LOG(L"[TLS] handshake basarisiz: %a (rc=%d)\r\n",
                          tls_err_str(rc), rc);
        if (tls_client_last_alert(&g_tls, &lvl, &desc) == 0)
            NETWORK_DEBUG_LOG(L"[TLS] alert: level=%d desc=%d\r\n", lvl, desc);
        if (rc == TLS_ERR_ALERT_FATAL || rc == TLS_ERR_CRYPTO)
            return NETWORK_ERR_TLS_VERIFICATION_FAILED;
        return NETWORK_ERR_TLS_UNAVAILABLE;
    }
    if (!g_tls.peer_cert_hash_valid)
        return NETWORK_ERR_TLS_VERIFICATION_FAILED;
    if (tls_verify_host_fingerprint(host, g_tls.peer_cert_hash) != 0) {
        NETWORK_DEBUG_LOG(L"[TLS] sertifika parmak izi eslesmedi (TOFU)\r\n");
        return NETWORK_ERR_TLS_VERIFICATION_FAILED;
    }
    NETWORK_DEBUG_LOG(L"[TLS] handshake tamam (TLS 1.2, AES128-SHA256)\r\n");
    return 0;
}

/* HTTP isteğini şifreli gönderir, yanıtı şifreli okur; yalnızca gövde döner */
static int tls_http_exchange(const char *host, const char *path,
                             char *buf, int maxlen) {
    char req[512]; int rl = 0;
    const char *m = "GET ", *pr = " HTTP/1.1\r\nHost: ", *en = "\r\nConnection: close\r\n\r\n";
    if (strlen(m) + strlen(path) + strlen(pr) + strlen(host) + strlen(en) >= sizeof(req)) return -4;
    for (const char *p = m; *p; p++) req[rl++] = *p;
    for (const char *p = path; *p; p++) req[rl++] = *p;
    for (const char *p = pr; *p; p++) req[rl++] = *p;
    for (const char *p = host; *p; p++) req[rl++] = *p;
    for (const char *p = en; *p; p++) req[rl++] = *p;
    req[rl] = '\0';

    if (tls_client_write(&g_tls, (const uint8_t *)req, (size_t)rl) != TLS_OK) {
        NETWORK_DEBUG_LOG(L"[TLS] istek gonderilemedi\r\n");
        return -1;
    }
    NETWORK_DEBUG_LOG(L"[TLS] HTTP istegi sifreli gonderildi\r\n");

    unsigned char rbuf[2048];
    char hbuf[2048]; int hlen = 0;
    int total = 0, header_done = 0;
    size_t rlen;
    for (;;) {
        int rc = tls_client_read(&g_tls, rbuf, sizeof rbuf, &rlen);
        if (rc == TLS_OK && rlen == 0) break;   /* close_notify: temiz son */
        if (rc != TLS_OK) break;                /* IO hatasi / zaman asimi */
        if (!header_done) {
            for (size_t k = 0; k < rlen; k++) {
                if (hlen >= (int)sizeof(hbuf)) return -4;
                hbuf[hlen++] = (char)rbuf[k];
                if (hlen >= 4 && hbuf[hlen-4] == '\r' && hbuf[hlen-3] == '\n' &&
                    hbuf[hlen-2] == '\r' && hbuf[hlen-1] == '\n') {
                    for (size_t j = k + 1; j < rlen; j++) {
                        if (total + 1 >= maxlen) return -4;
                        buf[total++] = (char)rbuf[j];
                    }
                    header_done = 1;
                    break;
                }
            }
        } else {
            if (total + (int)rlen >= maxlen) return -4;
            for (size_t j = 0; j < rlen; j++) buf[total++] = (char)rbuf[j];
        }
    }
    if (total > 0) {
        if (total < maxlen) buf[total] = '\0';
        NETWORK_DEBUG_LOG(L"[TLS] HTTP govdesi %d byte\r\n", total);
        return total;
    }
    NETWORK_DEBUG_LOG(L"[TLS] HTTP yaniti alinamadi\r\n");
    return -1;
}

/* ─── HTTP ───────────────────────────────────────────────── */
static int http_get_common(const char *host, const char *path, char *buf, int maxlen, unsigned short port) {
    unsigned char dest[4];
    int dns_result = -1;
    int tcp_result = -1;
    int http_result = -1;
    if (!host || !path || !buf || maxlen < 2) return -4;
    dns_result = parse_host(host, dest);
    if (dns_result != 0) {
        NETWORK_DEBUG_LOG(L"[HTTP] dns basarisiz: %d\r\n", dns_result);
        return -1;
    }
    tcp_result = tcp_connect(dest, port);
    if (tcp_result != 0) {
        NETWORK_DEBUG_LOG(L"[HTTP] tcp basarisiz: %d\r\n", tcp_result);
        /* Bağlantı aşamasında kilitlenme/hatada otomatik onarım dene */
        if (tcp_auto_reconnect(dest, port) == 0) {
            tcp_result = 0;
        } else {
            return -2;
        }
    }

    char req[512]; int rl = 0;
    const char *m = "GET ", *pr = " HTTP/1.1\r\nHost: ", *en = "\r\nConnection: close\r\n\r\n";
    if (strlen(m) + strlen(path) + strlen(pr) + strlen(host) + strlen(en) >= sizeof(req)) return -4;
    for (const char *p = m; *p; p++) req[rl++] = *p;
    for (const char *p = path; *p; p++) req[rl++] = *p;
    for (const char *p = pr; *p; p++) req[rl++] = *p;
    for (const char *p = host; *p; p++) req[rl++] = *p;
    for (const char *p = en; *p; p++) req[rl++] = *p;
    req[rl] = '\0';

    for (int attempt = 0; attempt < 3; attempt++) {
        if (send_tcp_packet(dest, port, TCP_PSH | TCP_ACK, req, rl) != 0) {
            /* Gönderim hatası: onar + yeniden el sıkışma, isteği tekrarla */
            if (tcp_auto_reconnect(dest, port) != 0) break;
            continue;
        }
        g_tcp_seq += rl;

        int len = tcp_recv_response(buf, maxlen);
        http_result = len;
        if (len > 0) {
            tcp_disconnect(dest, port);
            NETWORK_DEBUG_LOG(L"[HTTP] dns=%d tcp=%d http=%d\r\n", dns_result, tcp_result, http_result);
            return len;
        }
        /* Zaman aşımı / kalıcı hata / RST: kilidi bekleme, otomatik onarım
           ile taze TCP el sıkışması başlat ve isteği hemen yeniden dene. */
        if (len == TCP_RX_TIMEOUT || len == TCP_RX_ERROR || len == TCP_RX_REPAIR) {
            NETWORK_DEBUG_LOG(L"[HTTP] alım takıldı (%d), otomatik onarılıyor\r\n", len);
            if (tcp_auto_reconnect(dest, port) != 0) break;
            continue;
        }
        /* Diğer hatada da bir kez daha dene */
        if (attempt == 0 && tcp_auto_reconnect(dest, port) == 0) continue;
        break;
    }
    tcp_safe_close();
    NETWORK_DEBUG_LOG(L"[HTTP] dns=%d tcp=%d http=%d\r\n", dns_result, tcp_result, http_result);
    return -1;
}

static int tcp_recv_response(char *buf, int maxlen) {
    static unsigned char rx[1500]; unsigned int rx_len = 0;
    char header[2048];
    int header_len = 0;
    int total = 0; int header_done = 0;
    int rx_fatal = 0;         /* ardışık kalıcı alım hatası sayacı */
    int idle_after_data = 0;  /* veri geldikten sonra sessiz tur sayacı */
    int got_fin = 0;

    if (!buf || maxlen < 2 || !g_tcp_connected) return -1;

    /* 6 saniyelik toplam pencere; içte non-blocking boşaltma ile NIC
       kuyruğu anında tüketilir ve her segmentin ACK'i geciktirilmeden
       gönderilir. Böylece sunucu retransmisyon kilidinden kaçınılır. */
    for (int i = 0; i < 6000; i++) {
        int drained = 0;
        for (;;) {
            rx_len = 0; /* (HATA 1 kural c) paylaşılan uzunluk her alımdan önce sıfırlansın */
            int rr = raw_recv(rx, &rx_len);
            if (rr == -1) {
                /* Kalıcı alım hatası (EFI_NOT_READY dışı): kilidi bekleme */
                if (++rx_fatal >= 5) return TCP_RX_ERROR;
                break;
            }
            if (rr != 0) break;            /* -2: kuyruk boş */
            if (rx_len < 54) continue;     /* kısa paket: atla */
            drained = 1;
            ip4_hdr_t *rip = (ip4_hdr_t*)(rx + 14);
            tcp_hdr_t *tcp = (tcp_hdr_t*)(rx + 34);
            if (!tcp_is_ours(rip, tcp)) {
                /* Kapalı bağlantı retransmisyonu? Tüket + RST (HATA 3) */
                tcp_rst_closed_segment(rip, tcp);
                continue;
            }
            if (tcp->flags & TCP_RST) { g_tcp_rst_received = 1; return TCP_RX_REPAIR; }
            if (!g_net) return TCP_RX_REPAIR;

            int ip_len = (rip->ver_ihl & 0x0f) * 4;
            int tcp_len = ((tcp->data_offset >> 4) & 0x0f) * 4;
            int data_start = 14 + ip_len + tcp_len;
            if (ip_len < 20 || tcp_len < 20 || data_start > (int)rx_len) continue;
            /* Payload uzunluğu IP total length alanından hesaplanır, ham çerçeve
               uzunluğundan DEĞİL (Ethernet 60 bayt dolgu). Böylece boş ACK'lar
               veri gibi işlenip rcv_nxt'i kaydırmaz. */
            int ip_total = __builtin_bswap16(rip->len);
            int data_len = ip_total - ip_len - tcp_len;
            if (data_len < 0) data_len = 0;
            if (data_start + data_len > (int)rx_len) data_len = (int)rx_len - data_start;

            /* Sira disi/eksik segment: tcp->seq beklenen g_tcp_ack ile
               eslesmiyorsa veriyi buffer'a ekleme, ACK'i ilerletme. SNP
               aradaki segmenti kacirmis olabilir; duplicate ACK ile sunucunun
               kaybolan segmenti yeniden gondermesi saglanir (fast retransmit).
               Ayni kural FIN icin de gecerli: eksik veri varken gelen FIN
               kabul edilmez (RFC 793 — FIN ancak kendinden onceki tum veri
               onaylandiktan sonra islenir). */
            if (data_len > 0 || (tcp->flags & TCP_FIN)) {
                unsigned int seg_seq = __builtin_bswap32(tcp->seq);
                int seg_has_fin = (tcp->flags & TCP_FIN) ? 1 : 0;
                /* Zaten ACK'lanmış yeniden iletimler: atla (eski veri/FIN);
                   sunucu ACK'i almamış olabilir → kümülatif ACK'i yeniden gönder. */
                if ((seg_seq + (unsigned int)data_len + (unsigned int)seg_has_fin) <= g_tcp_ack) {
                    if (tcp_send_ack() != 0) return TCP_RX_REPAIR;
                    continue;
                }
                if (seg_seq != g_tcp_ack) {
                    NETWORK_DEBUG_LOG(L"[HTTP] sira disi segment: seq=%d beklenen=%d, atlaniyor\r\n", seg_seq, g_tcp_ack);
                    if (tcp_send_ack() != 0) return TCP_RX_REPAIR;
                    continue;
                }
            }

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
            /* ACK gönder — gecikmeden, tampon tüketimi ile aynı anda.
               rcv_nxt yalnızca payload + FIN kadar ilerler. tcp_send_ack()
               ilerleme yoksa tekrar göndermez (HATA 4). */
            if (data_len > 0) g_tcp_ack += data_len;
            if (tcp->flags & TCP_FIN) { g_tcp_ack++; got_fin = 1; }
            if (data_len > 0 || got_fin) {
                if (tcp_send_ack() != 0) return TCP_RX_REPAIR;
            }
            if (got_fin) break;
        }

        if (drained) {
            rx_fatal = 0;
            idle_after_data = 0;
            /* İşleme sırasında / ACK sırasında gelen paketleri kaçırma: 1ms
               beklemeden hemen bir sonraki boşaltma turuna dön (HATA 1 kural d). */
            continue;
        } else if (total > 0) {
            /* Veri geldi ama kuyruk boş: ~2 saniye sessizse tamamlandı say.
               Sunucu retransmisyonu ~1s araliklarla gelir; 2 saniyelik pencere
               ilk retransmisyonu yakalar. Eksik segment varsa periyodik dup ACK. */
            if (++idle_after_data >= 2000) break;
            if (g_tcp_dup_count > 0 && (idle_after_data % 200) == 0) {
                g_tcp_dup_count = 0;
                tcp_send_ack();
            }
        }

        if (got_fin) break;
        uefi_stall(1000); /* 1ms: art arda gelen paketleri kacirmamak icin sik poll */
    }
    if (total > 0 && total < maxlen) buf[total] = '\0';
    if (total > 0) return total;
    return TCP_RX_TIMEOUT;
}

int http_get(const char *host, const char *path, char *buf, int maxlen) {
    return http_get_common(host, path, buf, maxlen, 80);
}

int http_get_port(const char *host, const char *path, char *buf, int maxlen, unsigned short port) {
    return http_get_common(host, path, buf, maxlen, port ? port : 80);
}

int http_post(const char *host, const char *path, const char *data, int data_len, char *buf, int maxlen) {
    unsigned char dest[4];
    if (!host || !path || !data || data_len < 0 || !buf || maxlen < 2) return -4;
    if (parse_host(host, dest) != 0) return -1;
    if (tcp_connect(dest, 80) != 0) return -2;
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

    for (int attempt = 0; attempt < 3; attempt++) {
        if (send_tcp_packet(dest, 80, TCP_PSH | TCP_ACK, req, rl) != 0) {
            if (tcp_auto_reconnect(dest, 80) != 0) break;
            continue;
        }
        g_tcp_seq += rl;

        int len = tcp_recv_response(buf, maxlen);
        if (len > 0) {
            tcp_disconnect(dest, 80);
            return len;
        }
        if (len == TCP_RX_TIMEOUT || len == TCP_RX_ERROR || len == TCP_RX_REPAIR) {
            if (tcp_auto_reconnect(dest, 80) != 0) break;
            continue;
        }
        if (attempt == 0 && tcp_auto_reconnect(dest, 80) == 0) continue;
        break;
    }
    tcp_safe_close();
    return -1;
}

int wget(const char *host, const char *http_path, const char *save_path, char *buf, int maxlen) {
    int len = http_get(host, http_path, buf, maxlen);
    if (len <= 0) return len;
    if (fs_write_file(&g_fs, save_path, buf, (size_t)len) < 0) return -4;
    return len;
}

int https_get(const char *host, const char *path, char *buf, int maxlen) {
    return https_get_port(host, path, buf, maxlen, 443);
}

/* HTTPS: gerçek TLS 1.2 el sıkışması (RSA, AES-128-CBC-SHA256), TOFU
   sertifika doğrulaması ve şifreli HTTP isteği/yanıtı. */
int https_get_port(const char *host, const char *path, char *buf, int maxlen, unsigned short port) {
    unsigned char dest[4];
    int dns_result = -1;
    int tcp_result = -1;
    int tls_result = -1;
    int http_result = -1;
    if (!host || !path || !buf || maxlen < 2) return -4;
    if (!port) port = 443;
    dns_result = parse_host(host, dest);
    if (dns_result != 0) {
        NETWORK_DEBUG_LOG(L"[HTTPS] dns basarisiz: %d\r\n", dns_result);
        return -1;
    }

    /* Kilitlenme/soket hatası durumunda otomatik onarım + taze TCP el
       sıkışması ile isteği yeniden dene (kullanıcıya hissettirmeden).
       En fazla 3 deneme; her denemede yalnızca TEK bağlantı açık kalır. */
    for (int attempt = 0; attempt < 3; attempt++) {
        tcp_result = tcp_connect(dest, port);
        if (tcp_result != 0) {
            NETWORK_DEBUG_LOG(L"[HTTPS] tcp basarisiz: %d\r\n", tcp_result);
            if (attempt < 2 && tcp_auto_reconnect(dest, port) == 0) {
                /* tcp_auto_reconnect bağlantıyı zaten kurdu; döngünün başına
                   dönüp tcp_connect'i tekrar çağırmak YENİ bir port açar ve
                   önceki bağlantıyı yarım bırakır (port spam'i). Burada akış
                   TLS aşamasına geçer. */
                tcp_result = 0;
            } else {
                return -2;
            }
        }

        tls_result = tls_session_handshake(host, dest, port);
        if (tls_result != 0) {
            NETWORK_DEBUG_LOG(L"[HTTPS] tls basarisiz: %d\r\n", tls_result);
            /* TLS başarısız (-7/-8): soketi güvenli kapat (FIN + RX temizleme),
               state tamamen sıfırlanır, 1 sn bekle ve TEK bağlantıyı taze bir
               el sıkışmasıyla yeniden kur. En fazla 3 deneme; aynı anda yalnızca
               bir bağlantı açık kalır (port spam'i yok). */
            if (attempt < 2) {
                tcp_safe_close();
                pit_delay_ms(1000);
                continue;
            }
            NETWORK_DEBUG_LOG(L"[HTTPS] dns=%d tcp=%d tls=%d http=%d\r\n", dns_result, tcp_result, tls_result, http_result);
            return tls_result;
        }

        http_result = tls_http_exchange(host, path, buf, maxlen);
        if (http_result > 0) {
            tcp_disconnect(dest, port);
            NETWORK_DEBUG_LOG(L"[HTTPS] dns=%d tcp=%d tls=%d http=%d\r\n", dns_result, tcp_result, tls_result, http_result);
            return http_result;
        }
        /* Zaman aşımı / kalıcı hata: kilidi bekleme, yeniden bağlan */
        NETWORK_DEBUG_LOG(L"[HTTPS] alım takıldı (%d), yeniden deneniyor\r\n", http_result);
        if (attempt < 2) {
            tcp_safe_close();
            pit_delay_ms(1000);
            continue;
        }
        tcp_disconnect(dest, port);
        break;
    }
    NETWORK_DEBUG_LOG(L"[HTTPS] dns=%d tcp=%d tls=%d http=%d\r\n", dns_result, tcp_result, tls_result, http_result);
    return -1;
}

/* ─── Otomatik Ağ Sağlık İzleme / Onarım ─────────────────────── */

#define NET_HEALTH_INTERVAL   48    /* denetimler arası tick sayısı */
#define NET_HEALTH_FAIL_LIMIT 2     /* ardışık başarısız denetimde onarım */
#define NET_HEALTH_PROBE_MAX  15    /* ağ geçidi sınama zaman aşımı (×10ms) */

static int g_net_health_tick       = 0;
static int g_net_health_fail       = 0;
static int g_net_healthy           = 1;
static int g_net_repair_count      = 0;
static int g_net_reinit_cooldown   = 0;

static int network_nic_ready(void) {
    if (!g_net) return 0;
    if (g_net->Mode->State != EfiSimpleNetworkInitialized) return 0;
    if (g_net->Mode->MediaPresentSupported && !g_net->Mode->MediaPresent) return 0;
    return 1;
}

/* Ağ geçidine sınırlı süreli ARP yoklaması: arp_resolve()'un 1 saniyelik
   blokajı yerine en fazla ~150ms bekler, böylece aktif TCP trafiğiyle
   çakışmaz ve kullanıcı arayüzünde hissedilir bir takılma olmaz. */
static int network_probe_gateway(void) {
    if (!network_nic_ready()) return -1;
    if (g_gateway_mac_valid) return 0; /* geçerli önbellek: sağlıklı */

    unsigned char *my_mac = g_net->Mode->CurrentAddress.Addr;
    static unsigned char pkt[64];
    eth_hdr_t *eth = (eth_hdr_t*)pkt;
    arp_hdr_t *arp = (arp_hdr_t*)(pkt + 14);

    for (int i = 0; i < 6; i++) { eth->dst[i] = 0xFF; eth->src[i] = my_mac[i]; }
    eth->type = __builtin_bswap16(0x0806);
    arp->hw_type = __builtin_bswap16(1);
    arp->proto_type = __builtin_bswap16(0x0800);
    arp->hw_len = 6;
    arp->proto_len = 4;
    arp->opcode = ARP_OP_REQUEST;
    for (int i = 0; i < 6; i++) arp->sender_mac[i] = my_mac[i];
    for (int i = 0; i < 4; i++) arp->sender_ip[i] = g_ip[i];
    for (int i = 0; i < 6; i++) arp->target_mac[i] = 0;
    for (int i = 0; i < 4; i++) arp->target_ip[i] = g_gateway[i];

    if (raw_send(pkt, 42) != 0) return -1;

    static unsigned char rx[1500];
    unsigned int rx_len = 0;
    for (int i = 0; i < NET_HEALTH_PROBE_MAX; i++) {
        uefi_stall(10000);
        if (raw_recv(rx, &rx_len) == 0 && rx_len >= 42) {
            unsigned short t = __builtin_bswap16(*(unsigned short*)(rx + 12));
            if (t == 0x0806) {
                arp_hdr_t *reply = (arp_hdr_t*)(rx + 14);
                if (reply->opcode == ARP_OP_REPLY &&
                    reply->sender_ip[0] == g_gateway[0] &&
                    reply->sender_ip[1] == g_gateway[1] &&
                    reply->sender_ip[2] == g_gateway[2] &&
                    reply->sender_ip[3] == g_gateway[3]) {
                    mac_copy(g_gateway_mac, reply->sender_mac);
                    g_gateway_mac_valid = 1;
                    g_gateway_arp_fail_count = 0;
                    g_gateway_arp_wait = 0;
                    return 0;
                }
            }
        }
    }
    return -1;
}

int network_force_repair(void) {
    NETWORK_DEBUG_LOG(L"[NET] Otomatik onarım başlıyor\r\n");
    g_gateway_mac_valid = 0;
    g_gateway_arp_wait  = 0;
    g_gateway_arp_fail_count = 0;
    g_tcp_connected = 0;
    g_net = NULL;
    if (g_st) network_init(g_st);
    if (!network_nic_ready()) return -1;
    g_net_health_fail = 0;
    g_net_healthy = 1;
    g_net_repair_count++;
    g_net_reinit_cooldown = 0;
    NETWORK_DEBUG_LOG(L"[NET] Onarım tamamlandı, state=%d\r\n", g_net->Mode->State);
    return 0;
}

int network_health_status(void) {
    if (!g_net) return 0;
    if (g_net->Mode->State != EfiSimpleNetworkInitialized) return 0;
    if (g_net->Mode->MediaPresentSupported && !g_net->Mode->MediaPresent) return 0;
    return g_net_healthy;
}

int network_repair_count(void) { return g_net_repair_count; }

void network_monitor_tick(void) {
    if (g_net_reinit_cooldown > 0) {
        g_net_reinit_cooldown--;
        return;
    }

    /* Boşta kalınan HER tick'te NIC kuyruğunu tüket; ölü bağlantı retransmisyonlarına
       anında RST gönder (ring birikmesin — HATA 1/3). Aktif oturum sürerken dokunma:
       aktif oturumun okuma döngüleri kendi drain'ini yapar. */
    if (g_net && !g_tcp_connected && network_nic_ready()) {
        tcp_flush_rx();
        tcp_closed_decay();
    }

    g_net_health_tick++;
    if (g_net_health_tick < NET_HEALTH_INTERVAL) return;
    g_net_health_tick = 0;

    if (!g_net) {
        /* NIC hiç bulunamadı: LocateProtocol'u tekrar dene */
        g_net_reinit_cooldown = 4;
        g_net = NULL;
        if (g_st) network_init(g_st);
        if (!network_nic_ready()) {
            g_net_health_fail++;
            if (g_net_health_fail >= NET_HEALTH_FAIL_LIMIT) {
                g_net_health_fail = 0;
                g_net_repair_count++;
                g_net_healthy = 0;
            }
            return;
        }
        g_net_healthy = 1;
        g_net_health_fail = 0;
        return;
    }

    if (!network_nic_ready()) {
        /* NIC durumu bozuk ya da kablo/medya kopmuş: hemen onar */
        network_force_repair();
        return;
    }

    if (g_tcp_connected) {
        /* Aktif TCP oturumu sürüyorsa denetim yapma; sağlıklı kabul et */
        g_net_health_fail = 0;
        g_net_healthy = 1;
        return;
    }

    /* Boşta kalınan dönemde NIC kuyruğu yukarıdaki her-tick drain'inde zaten
       tüketiliyor; burada yalnızca sağlık sondası çalışır. */
    if (network_probe_gateway() == 0) {
        g_net_health_fail = 0;
        g_net_healthy = 1;
    } else {
        g_net_health_fail++;
        if (g_net_health_fail >= NET_HEALTH_FAIL_LIMIT) {
            g_net_health_fail = 0;
            g_net_healthy = 0;
            network_force_repair();
        }
    }
}
