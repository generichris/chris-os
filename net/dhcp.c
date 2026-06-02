#include "dhcp.h"
#include "udp.h"
#include "eth.h"
#include "../kernel/kernel.h"
#include "../kernel/kprintf.h"
#include <stdint.h>

#define DHCP_SERVER_PORT  67
#define DHCP_CLIENT_PORT  68
#define DHCP_MAGIC        0x63825363

#define DHCP_DISCOVER  1
#define DHCP_OFFER     2
#define DHCP_REQUEST   3
#define DHCP_ACK       5

typedef struct __attribute__((packed)) {
    uint8_t     op, htype, hlen, hops;
    uint32_t    xid;
    uint16_t    secs, flags;
    ipv4_addr_t ciaddr, yiaddr, siaddr, giaddr;
    uint8_t     chaddr[16];
    uint8_t     sname[64];
    uint8_t     file[128];
    uint32_t    magic;
    uint8_t     options[312];  /* RFC 2131: minimum 312 bytes of options space */
} dhcp_pkt_t;

static volatile ipv4_addr_t offered_ip;
static volatile int dhcp_got_offer = 0;
static volatile int dhcp_got_ack   = 0;
static uint32_t dhcp_xid = 0xDEADBEEF;

static void dhcp_rx(ipv4_addr_t src, uint16_t src_port, uint8_t *data, uint16_t len) {
    (void)src; (void)src_port;
    if (len < sizeof(dhcp_pkt_t)) return;
    dhcp_pkt_t *pkt = (dhcp_pkt_t *)data;
    if (pkt->op != 2) return;
    if (ntohl(pkt->xid) != dhcp_xid) return;  /* ignore packets for other transactions */
    uint8_t msg_type = 0;
    int i = 0;
    /* Safe options parsing: use actual received length to bound the walk */
    int opts_len = (int)(len - (int)((uint8_t *)pkt->options - (uint8_t *)pkt));
    if (opts_len > 312) opts_len = 312;   /* RFC 2131 max options field */
    while (i < opts_len) {
        uint8_t opt = pkt->options[i++];
        if (opt == 255) break;  /* end option */
        if (opt == 0)  continue; /* pad option */
        if (i >= opts_len) break;
        uint8_t olen = pkt->options[i++];
        if (i + olen > opts_len) break;  /* malformed, stop */
        if (opt == 53 && olen == 1) msg_type = pkt->options[i];
        i += olen;
    }
    if (msg_type == DHCP_OFFER) { offered_ip = pkt->yiaddr; dhcp_got_offer = 1; }
    if (msg_type == DHCP_ACK)   { eth_set_ip(pkt->yiaddr);  dhcp_got_ack   = 1; }
}

static void send_discover(void) {
    dhcp_pkt_t pkt;
    for (int i = 0; i < (int)sizeof(pkt); i++) ((uint8_t *)&pkt)[i] = 0;
    pkt.op    = 1;
    pkt.htype = 1;
    pkt.hlen  = 6;
    pkt.xid   = htonl(dhcp_xid);
    pkt.flags = htons(0x8000);
    mac_addr_t mac = eth_get_mac();
    for (int i = 0; i < 6; i++) pkt.chaddr[i] = mac.bytes[i];
    pkt.magic = htonl(DHCP_MAGIC);
    pkt.options[0] = 53; pkt.options[1] = 1; pkt.options[2] = DHCP_DISCOVER;
    pkt.options[3] = 255;
    ipv4_addr_t broadcast = {{255,255,255,255}};
    udp_send(broadcast, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, (uint8_t *)&pkt, sizeof(pkt));
}

static void send_request(ipv4_addr_t ip) {
    dhcp_pkt_t pkt;
    for (int i = 0; i < (int)sizeof(pkt); i++) ((uint8_t *)&pkt)[i] = 0;
    pkt.op    = 1;
    pkt.htype = 1;
    pkt.hlen  = 6;
    pkt.xid   = htonl(dhcp_xid);
    pkt.flags = htons(0x8000);
    mac_addr_t mac = eth_get_mac();
    for (int i = 0; i < 6; i++) pkt.chaddr[i] = mac.bytes[i];
    pkt.magic = htonl(DHCP_MAGIC);
    int i = 0;
    pkt.options[i++] = 53; pkt.options[i++] = 1; pkt.options[i++] = DHCP_REQUEST;
    pkt.options[i++] = 50; pkt.options[i++] = 4;
    pkt.options[i++] = ip.bytes[0]; pkt.options[i++] = ip.bytes[1];
    pkt.options[i++] = ip.bytes[2]; pkt.options[i++] = ip.bytes[3];
    pkt.options[i++] = 255;
    ipv4_addr_t broadcast = {{255,255,255,255}};
    udp_send(broadcast, DHCP_SERVER_PORT, DHCP_CLIENT_PORT, (uint8_t *)&pkt, sizeof(pkt));
}

void dhcp_init(void) {
    udp_bind(DHCP_CLIENT_PORT, dhcp_rx);
}

int dhcp_request(void) {
    dhcp_got_offer = 0;
    dhcp_got_ack   = 0;
    send_discover();
    /* 5 seconds at ~18Hz = 90 ticks; give the server enough time to respond */
    uint32_t deadline = get_ticks() + 90;
    while (get_ticks() < deadline && !dhcp_got_offer) {
        extern void eth_poll(void);
        eth_poll();
    }
    if (!dhcp_got_offer) { kprintf("dhcp: no offer\n"); return -1; }
    ipv4_addr_t ip = {.bytes = {offered_ip.bytes[0], offered_ip.bytes[1],
                                offered_ip.bytes[2], offered_ip.bytes[3]}};
    send_request(ip);
    deadline = get_ticks() + 90;
    while (get_ticks() < deadline && !dhcp_got_ack) {
        extern void eth_poll(void);
        eth_poll();
    }
    if (!dhcp_got_ack) { kprintf("dhcp: no ack\n"); return -1; }
    ipv4_addr_t got = eth_get_ip();
    kprintf("dhcp: got %d.%d.%d.%d\n", got.bytes[0], got.bytes[1], got.bytes[2], got.bytes[3]);
    return 0;
}
