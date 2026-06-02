#ifndef ETH_H
#define ETH_H

#include <stdint.h>
#include "net_types.h"

#define ETH_TYPE_IPV4  0x0800
#define ETH_TYPE_ARP   0x0806
#define ETH_MAX_FRAME  1514
#define ETH_HEADER_LEN 14

typedef struct __attribute__((packed)) {
    mac_addr_t dest;
    mac_addr_t src;
    uint16_t   type;
} eth_header_t;

void       eth_init(void);
void       eth_send(mac_addr_t dest, uint16_t type, uint8_t *payload, uint16_t len);
void       eth_handle_interrupt(void);
void       eth_poll(void);
mac_addr_t eth_get_mac(void);
ipv4_addr_t eth_get_ip(void);
void       eth_set_ip(ipv4_addr_t ip);
int        eth_ready(void);
uint8_t    eth_get_irq(void);   /* Returns PCI-assigned IRQ number */

#endif
