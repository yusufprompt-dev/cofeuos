#ifndef NETWORK_H
#define NETWORK_H

void network_init(void *st);
int  network_available(void);
void network_get_mac(unsigned char *mac);
void network_get_ip(unsigned char *ip);
void network_dhcp(void);
int  network_ping(unsigned char *dst_ip);
int  wget(const char *host, const char *http_path, const char *save_path, char *buf, int maxlen);

#endif