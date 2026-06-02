#ifndef ARP_H
#define ARP_H

#include "net_types.h"

void       arp_init(void);
void       arp_handle(uint8_t *payload, uint16_t len);
mac_addr_t arp_lookup(ipv4_addr_t ip);
void       arp_request(ipv4_addr_t target);
void       arp_set_gateway(ipv4_addr_t gw);
void       arp_set_subnet(ipv4_addr_t mask);

#endif