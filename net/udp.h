#ifndef UDP_H
#define UDP_H

#include "net_types.h"
#include <stdint.h>

typedef void (*udp_cb_t)(ipv4_addr_t src, uint16_t src_port, uint8_t *data, uint16_t len);

void     udp_init(void);
void     udp_handle(ipv4_addr_t src, uint8_t *payload, uint16_t len);
void     udp_send(ipv4_addr_t dest, uint16_t dst_port, uint16_t src_port, uint8_t *data, uint16_t len);
int      udp_bind(uint16_t port, udp_cb_t cb);
void     udp_unbind(uint16_t port);

#endif
