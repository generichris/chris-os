#include "arp.h"
#include "eth.h"
#include "net_types.h"
#include "../kernel/kprintf.h"
#include "../kernel/kernel.h"
#include <stdint.h>

#define ARP_REQUEST    1
#define ARP_REPLY      2
#define ARP_CACHE_SIZE 16

typedef struct __attribute__((packed)) {
    uint16_t    hw_type;
    uint16_t    proto_type;
    uint8_t     hw_len;
    uint8_t     proto_len;
    uint16_t    op;
    mac_addr_t  sender_mac;
    ipv4_addr_t sender_ip;
    mac_addr_t  target_mac;
    ipv4_addr_t target_ip;
} arp_packet_t;

typedef struct {
    ipv4_addr_t ip;
    mac_addr_t  mac;
    int         valid;
} arp_entry_t;

static arp_entry_t cache[ARP_CACHE_SIZE];

static ipv4_addr_t gateway = {{10, 0, 2, 2}};
static ipv4_addr_t subnet  = {{255, 255, 255, 0}};

void arp_init(void) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) cache[i].valid = 0;
}

void arp_set_gateway(ipv4_addr_t gw)   { gateway = gw; }
void arp_set_subnet(ipv4_addr_t mask)  { subnet  = mask; }

static void cache_store(ipv4_addr_t ip, mac_addr_t mac) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!cache[i].valid || ip_eq(cache[i].ip, ip)) {
            cache[i].ip    = ip;
            cache[i].mac   = mac;
            cache[i].valid = 1;
            return;
        }
    }
    cache[0].ip    = ip;
    cache[0].mac   = mac;
    cache[0].valid = 1;
}

static int cache_find(ipv4_addr_t ip, mac_addr_t *out) {
    for (int i = 0; i < ARP_CACHE_SIZE; i++)
        if (cache[i].valid && ip_eq(cache[i].ip, ip)) {
            *out = cache[i].mac;
            return 1;
        }
    return 0;
}

static int same_subnet(ipv4_addr_t a, ipv4_addr_t b) {
    for (int i = 0; i < 4; i++)
        if ((a.bytes[i] & subnet.bytes[i]) != (b.bytes[i] & subnet.bytes[i]))
            return 0;
    return 1;
}

void arp_request(ipv4_addr_t target) {
    arp_packet_t pkt;
    pkt.hw_type    = htons(1);
    pkt.proto_type = htons(ETH_TYPE_IPV4);
    pkt.hw_len     = 6;
    pkt.proto_len  = 4;
    pkt.op         = htons(ARP_REQUEST);
    pkt.sender_mac = eth_get_mac();
    pkt.sender_ip  = eth_get_ip();
    for (int i = 0; i < 6; i++) pkt.target_mac.bytes[i] = 0;
    pkt.target_ip  = target;
    eth_send(MAC_BROADCAST, ETH_TYPE_ARP, (uint8_t *)&pkt, sizeof(pkt));
}

mac_addr_t arp_lookup(ipv4_addr_t ip) {
    mac_addr_t found;

    ipv4_addr_t resolve_ip = ip;
    if (!same_subnet(ip, eth_get_ip()))
        resolve_ip = gateway;

    if (cache_find(resolve_ip, &found)) return found;

    kprintf("arp: requesting %d.%d.%d.%d\n",
            resolve_ip.bytes[0], resolve_ip.bytes[1],
            resolve_ip.bytes[2], resolve_ip.bytes[3]);

    arp_request(resolve_ip);

    uint32_t deadline = get_ticks() + 180;
    while (get_ticks() < deadline) {
        extern void eth_poll(void);
        eth_poll();
        if (cache_find(resolve_ip, &found)) {
            kprintf("arp: got MAC for %d.%d.%d.%d\n",
                    resolve_ip.bytes[0], resolve_ip.bytes[1],
                    resolve_ip.bytes[2], resolve_ip.bytes[3]);
            return found;
        }
    }

    kprintf("arp: timeout - no reply from %d.%d.%d.%d\n",
            resolve_ip.bytes[0], resolve_ip.bytes[1],
            resolve_ip.bytes[2], resolve_ip.bytes[3]);
    mac_addr_t zero = {{0,0,0,0,0,0}};
    return zero;
}

void arp_handle(uint8_t *payload, uint16_t len) {
    if (len < (uint16_t)sizeof(arp_packet_t)) return;
    arp_packet_t *pkt = (arp_packet_t *)payload;
    uint16_t op = ntohs(pkt->op);
    kprintf("arp: rx op=%d from %d.%d.%d.%d\n", op,
            pkt->sender_ip.bytes[0], pkt->sender_ip.bytes[1],
            pkt->sender_ip.bytes[2], pkt->sender_ip.bytes[3]);
    cache_store(pkt->sender_ip, pkt->sender_mac);
    if (op == ARP_REQUEST && ip_eq(pkt->target_ip, eth_get_ip())) {
        arp_packet_t reply;
        reply.hw_type    = htons(1);
        reply.proto_type = htons(ETH_TYPE_IPV4);
        reply.hw_len     = 6;
        reply.proto_len  = 4;
        reply.op         = htons(ARP_REPLY);
        reply.sender_mac = eth_get_mac();
        reply.sender_ip  = eth_get_ip();
        reply.target_mac = pkt->sender_mac;
        reply.target_ip  = pkt->sender_ip;
        eth_send(pkt->sender_mac, ETH_TYPE_ARP, (uint8_t *)&reply, sizeof(reply));
    }
}