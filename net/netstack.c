#include "netstack.h"
#include "eth.h"
#include "arp.h"
#include "ipv4.h"
#include "udp.h"
#include "tcp.h"
#include "dns.h"
#include "dhcp.h"
#include <stdint.h>

void netstack_init(void) {
    arp_init();
    ipv4_init();
    udp_init();
    tcp_init();
    dns_init();
    dhcp_init();
}

void netstack_rx(uint8_t *frame, uint16_t len) {
    if (len < 14) return;
    uint16_t type = (uint16_t)((frame[12] << 8) | frame[13]);
    uint8_t *payload = frame + 14;
    uint16_t plen    = (uint16_t)(len - 14);
    switch (type) {
        case ETH_TYPE_ARP:  arp_handle(payload, plen);  break;
        case ETH_TYPE_IPV4: ipv4_handle(payload, plen); break;
    }
}
