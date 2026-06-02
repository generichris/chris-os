#ifndef ICMP_H
#define ICMP_H

#include "net_types.h"
#include <stdint.h>

void icmp_handle(ipv4_addr_t src, uint8_t *payload, uint16_t len);
void icmp_ping(ipv4_addr_t dest);

#endif
