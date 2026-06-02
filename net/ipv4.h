#ifndef IPV4_H
#define IPV4_H

#include "net_types.h"
#include <stdint.h>

#define IP_PROTO_ICMP  1
#define IP_PROTO_TCP   6
#define IP_PROTO_UDP   17

void     ipv4_init(void);
void     ipv4_handle(uint8_t *payload, uint16_t len);
void     ipv4_send(ipv4_addr_t dest, uint8_t proto, uint8_t *payload, uint16_t len);
uint16_t ipv4_checksum(void *data, uint16_t len);

#endif
