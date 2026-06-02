#include "ipv4.h"
#include "eth.h"
#include "arp.h"
#include "icmp.h"
#include "udp.h"
#include "tcp.h"
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint8_t     ver_ihl;
    uint8_t     dscp;
    uint16_t    total_len;
    uint16_t    id;
    uint16_t    flags_frag;
    uint8_t     ttl;
    uint8_t     proto;
    uint16_t    checksum;
    ipv4_addr_t src;
    ipv4_addr_t dst;
} ipv4_hdr_t;

static uint16_t ip_id = 0;

void ipv4_init(void) { ip_id = 0; }

uint16_t ipv4_checksum(void *data, uint16_t len) {
    uint8_t  *p = (uint8_t *)data;
    uint32_t  sum = 0;
    while (len > 1) { sum += (uint16_t)((p[0] << 8) | p[1]); p += 2; len -= 2; }
    if (len) sum += (uint16_t)(p[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

void ipv4_handle(uint8_t *payload, uint16_t len) {
    if (len < 20) return;
    ipv4_hdr_t *hdr  = (ipv4_hdr_t *)payload;
    uint8_t     ihl  = (uint8_t)((hdr->ver_ihl & 0x0F) * 4);
    uint16_t    data_len = (uint16_t)(ntohs(hdr->total_len) - ihl);
    uint8_t    *data = payload + ihl;

    /* Accept packets destined for us OR broadcast */
    ipv4_addr_t my_ip = eth_get_ip();
    ipv4_addr_t broadcast = {{255,255,255,255}};
    if (!ip_eq(hdr->dst, my_ip) && !ip_eq(hdr->dst, broadcast)) return;

    switch (hdr->proto) {
        case IP_PROTO_ICMP: icmp_handle(hdr->src, data, data_len); break;
        case IP_PROTO_UDP:  udp_handle(hdr->src, data, data_len);  break;
        case IP_PROTO_TCP:  tcp_handle(hdr->src, data, data_len);  break;
    }
}

void ipv4_send(ipv4_addr_t dest, uint8_t proto, uint8_t *payload, uint16_t len) {
    uint8_t buf[1500];
    if (len + 20 > 1500) return;

    ipv4_hdr_t *hdr = (ipv4_hdr_t *)buf;
    hdr->ver_ihl   = 0x45;
    hdr->dscp      = 0;
    hdr->total_len = htons((uint16_t)(20 + len));
    hdr->id        = htons(ip_id++);
    hdr->flags_frag= htons(0x4000);
    hdr->ttl       = 64;
    hdr->proto     = proto;
    hdr->checksum  = 0;
    hdr->src       = eth_get_ip();
    hdr->dst       = dest;
    hdr->checksum  = htons(ipv4_checksum(hdr, 20));

    for (uint16_t i = 0; i < len; i++) buf[20 + i] = payload[i];

    /* For broadcast/multicast destinations, use broadcast MAC directly
     * instead of going through ARP (which would fail or route to gateway). */
    ipv4_addr_t broadcast = {{255,255,255,255}};
    mac_addr_t  dst_mac;
    if (ip_eq(dest, broadcast) || dest.bytes[0] >= 224) {
        dst_mac = MAC_BROADCAST;
    } else {
        dst_mac = arp_lookup(dest);
    }

    eth_send(dst_mac, ETH_TYPE_IPV4, buf, (uint16_t)(20 + len));
}
