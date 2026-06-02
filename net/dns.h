#ifndef DNS_H
#define DNS_H

#include "net_types.h"

void        dns_init(void);
ipv4_addr_t dns_lookup(const char *hostname);
void        dns_set_server(ipv4_addr_t server);

#endif
