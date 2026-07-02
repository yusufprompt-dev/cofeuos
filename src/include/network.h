#ifndef _NETWORK_H
#define _NETWORK_H

void network_init(void *st);
int  network_available(void);
void network_get_mac(unsigned char *mac);

#endif
void network_set_ip(unsigned char a, unsigned char b, unsigned char c, unsigned char d);
void network_get_ip(unsigned char *ip);
int  network_has_ip(void);
void network_dhcp(void);int network_ping(unsigned char *dst_ip);
