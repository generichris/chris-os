#include "udp.h"
#include "ipv4.h"
#include <stdint.h>
#include <stddef.h>

#define MAX_UDP_SOCKETS 8

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
} udp_hdr_t;

typedef struct {
    uint16_t port;
    udp_cb_t cb;
    int      used;
} udp_socket_t;

static udp_socket_t sockets[MAX_UDP_SOCKETS];

void udp_init(void) {
    for (int i = 0; i < MAX_UDP_SOCKETS; i++) sockets[i].used = 0;
}

int udp_bind(uint16_t port, udp_cb_t cb) {
    for (int i = 0; i < MAX_UDP_SOCKETS; i++) {
        if (!sockets[i].used) {
            sockets[i].port = port;
            sockets[i].cb   = cb;
            sockets[i].used = 1;
            return 0;
        }
    }
    return -1;
}

void udp_unbind(uint16_t port) {
    for (int i = 0; i < MAX_UDP_SOCKETS; i++)
        if (sockets[i].used && sockets[i].port == port)
            sockets[i].used = 0;
}

void udp_handle(ipv4_addr_t src, uint8_t *payload, uint16_t len) {
    if (len < sizeof(udp_hdr_t)) return;
    udp_hdr_t *hdr  = (udp_hdr_t *)payload;
    uint16_t   port = ntohs(hdr->dst_port);
    uint8_t   *data = payload + sizeof(udp_hdr_t);
    uint16_t   dlen = (uint16_t)(ntohs(hdr->length) - sizeof(udp_hdr_t));
    for (int i = 0; i < MAX_UDP_SOCKETS; i++)
        if (sockets[i].used && sockets[i].port == port)
            sockets[i].cb(src, ntohs(hdr->src_port), data, dlen);
}

void udp_send(ipv4_addr_t dest, uint16_t dst_port, uint16_t src_port, uint8_t *data, uint16_t len) {
    uint8_t buf[1472];
    if (len + sizeof(udp_hdr_t) > 1472) return;
    udp_hdr_t *hdr = (udp_hdr_t *)buf;
    hdr->src_port  = htons(src_port);
    hdr->dst_port  = htons(dst_port);
    hdr->length    = htons((uint16_t)(sizeof(udp_hdr_t) + len));
    hdr->checksum  = 0;
    for (uint16_t i = 0; i < len; i++) buf[sizeof(udp_hdr_t) + i] = data[i];
    ipv4_send(dest, IP_PROTO_UDP, buf, (uint16_t)(sizeof(udp_hdr_t) + len));
}
