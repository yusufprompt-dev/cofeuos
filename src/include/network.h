#ifndef NETWORK_H
#define NETWORK_H

/* Bir TLS saglayicisi baglanmadiginda HTTPS istekleri bu kodu dondurur. */
#define NETWORK_ERR_TLS_UNAVAILABLE (-7)
#define NETWORK_ERR_TLS_VERIFICATION_FAILED (-8)

void network_init(void *st);
int  network_available(void);
void network_get_mac(unsigned char *mac);
void network_get_ip(unsigned char *ip);
void network_set_dns(unsigned char *dns_ip);
void network_dhcp(void);
int  network_ping(unsigned char *dst_ip);
int  network_recv(unsigned char *buf, unsigned int *len);

/* DNS */
int dns_resolve(const char *domain, unsigned char *out_ip);

/* ARP - teshis icin */
int arp_test(unsigned char *target_ip, unsigned char *out_mac);

/* TCP */
int tcp_connect(unsigned char *dst_ip, unsigned short port);
int tcp_disconnect(unsigned char *dst_ip, unsigned short port);

/* HTTP */
int http_get(const char *host, const char *path, char *buf, int maxlen);
int http_get_port(const char *host, const char *path, char *buf, int maxlen, unsigned short port);
int http_post(const char *host, const char *path, const char *data, int data_len, char *buf, int maxlen);
int https_get(const char *host, const char *path, char *buf, int maxlen);
int https_get_port(const char *host, const char *path, char *buf, int maxlen, unsigned short port);

/* WGET */
int wget(const char *host, const char *http_path, const char *save_path, char *buf, int maxlen);

/* ─── Otomatik Ağ Sağlık İzleme / Onarım ─────────────────────── */
/* Ana döngüde (arka planda) periyodik olarak çağrılır. Ucuzdur;
   yalnızca belirli aralıklarda denetim yapar ve kopma/paket kaybı
   tespit ederse NIC'i kullanıcıya hissettirmeden onarır. */
void network_monitor_tick(void);

/* Anında onarım dener; 0 dönerse başarılı, -1 ağ erişimi yok. */
int  network_force_repair(void);

/* Sağlık durumu: 1 sağlıklı, 0 sorunlu/ağ yok. */
int  network_health_status(void);

/* Otomatik onarım sayacı (teşhis için). */
int  network_repair_count(void);

#endif
