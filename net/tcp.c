#include "tcp.h"
#include "ipv4.h"
#include "eth.h"
#include "../kernel/kernel.h"
#include "../kernel/kprintf.h"
#include <stdint.h>
#include <stddef.h>

#define TCP_FLAG_FIN 0x01
#define TCP_FLAG_SYN 0x02
#define TCP_FLAG_RST 0x04
#define TCP_FLAG_ACK 0x10

#define MAX_TCP_SOCKETS 4
#define TCP_MAX_PAYLOAD 1440   /* max data bytes per segment */
#define TCP_HDR_LEN     20

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t  data_off;
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent;
} tcp_hdr_t;

static tcp_socket_t sockets[MAX_TCP_SOCKETS];
static uint16_t     next_port = 49152;

static uint16_t tcp_checksum(ipv4_addr_t src, ipv4_addr_t dst, uint8_t *seg, uint16_t len) {
    uint32_t sum = 0;
    sum += (uint16_t)((src.bytes[0] << 8) | src.bytes[1]);
    sum += (uint16_t)((src.bytes[2] << 8) | src.bytes[3]);
    sum += (uint16_t)((dst.bytes[0] << 8) | dst.bytes[1]);
    sum += (uint16_t)((dst.bytes[2] << 8) | dst.bytes[3]);
    sum += IP_PROTO_TCP;
    sum += len;
    uint8_t *p = seg;
    uint16_t n = len;
    while (n > 1) { sum += (uint16_t)((p[0] << 8) | p[1]); p += 2; n -= 2; }
    if (n) sum += (uint16_t)(p[0] << 8);
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

/* Fixed-size send buffer: TCP header (20) + max payload (1440) */
static void tcp_send_flags(tcp_socket_t *s, uint8_t flags, uint8_t *data, uint16_t dlen) {
    if (dlen > TCP_MAX_PAYLOAD) dlen = TCP_MAX_PAYLOAD;
    uint8_t buf[TCP_HDR_LEN + TCP_MAX_PAYLOAD];
    uint16_t total = (uint16_t)(TCP_HDR_LEN + dlen);

    tcp_hdr_t *hdr = (tcp_hdr_t *)buf;
    hdr->src_port = htons(s->local_port);
    hdr->dst_port = htons(s->remote_port);
    hdr->seq      = htonl(s->seq);
    hdr->ack      = (flags & TCP_FLAG_ACK) ? htonl(s->ack) : 0;
    hdr->data_off = (uint8_t)(5 << 4);
    hdr->flags    = flags;
    hdr->window   = htons(4096);
    hdr->checksum = 0;
    hdr->urgent   = 0;
    for (uint16_t i = 0; i < dlen; i++) buf[TCP_HDR_LEN + i] = data[i];
    hdr->checksum = htons(tcp_checksum(eth_get_ip(), s->remote_ip, buf, total));
    ipv4_send(s->remote_ip, IP_PROTO_TCP, buf, total);
}

void tcp_init(void) {
    for (int i = 0; i < MAX_TCP_SOCKETS; i++) sockets[i].state = TCP_STATE_CLOSED;
}

tcp_socket_t *tcp_open(void) {
    for (int i = 0; i < MAX_TCP_SOCKETS; i++)
        if (sockets[i].state == TCP_STATE_CLOSED) {
            sockets[i].rx_head = 0;
            sockets[i].rx_tail = 0;
            sockets[i].seq     = 0xABCD1234;
            sockets[i].ack     = 0;
            return &sockets[i];
        }
    return 0;
}

int tcp_connect(tcp_socket_t *s, ipv4_addr_t dest, uint16_t port) {
    s->remote_ip   = dest;
    s->remote_port = port;
    s->local_port  = next_port++;
    s->state       = TCP_STATE_SYN_SENT;
    tcp_send_flags(s, TCP_FLAG_SYN, 0, 0);
    s->seq++;
    uint32_t deadline = get_ticks() + 36;
    while (get_ticks() < deadline && s->state == TCP_STATE_SYN_SENT) {
        extern void eth_poll(void);
        eth_poll();
    }
    return (s->state == TCP_STATE_ESTABLISHED) ? 0 : -1;
}

int tcp_send(tcp_socket_t *s, uint8_t *data, uint16_t len) {
    if (s->state != TCP_STATE_ESTABLISHED) return -1;
    tcp_send_flags(s, TCP_FLAG_ACK, data, len);
    s->seq += len;
    return (int)len;
}

int tcp_recv(tcp_socket_t *s, uint8_t *buf, uint16_t max_len) {
    uint32_t deadline = get_ticks() + 36;
    while (get_ticks() < deadline && s->rx_head == s->rx_tail) {
        extern void eth_poll(void);
        eth_poll();
    }
    int n = 0;
    while (s->rx_head != s->rx_tail && n < max_len) {
        buf[n++] = s->rx_buf[s->rx_tail];
        s->rx_tail = (uint16_t)((s->rx_tail + 1) % sizeof(s->rx_buf));
    }
    return n;
}

void tcp_close(tcp_socket_t *s) {
    if (s->state == TCP_STATE_ESTABLISHED)
        tcp_send_flags(s, TCP_FLAG_FIN | TCP_FLAG_ACK, 0, 0);
    s->state = TCP_STATE_CLOSED;
}

void tcp_handle(ipv4_addr_t src, uint8_t *payload, uint16_t len) {
    if (len < TCP_HDR_LEN) return;
    tcp_hdr_t *hdr     = (tcp_hdr_t *)payload;
    uint16_t   dst_port = ntohs(hdr->dst_port);
    uint16_t   src_port = ntohs(hdr->src_port);
    uint8_t    flags    = hdr->flags;
    uint32_t   seq      = ntohl(hdr->seq);
    uint32_t   ack      = ntohl(hdr->ack);
    uint8_t    off      = (uint8_t)((hdr->data_off >> 4) * 4);
    if (off < TCP_HDR_LEN || off > len) return;   /* sanity-check header offset */
    uint8_t   *data     = payload + off;
    uint16_t   dlen     = (uint16_t)(len - off);

    for (int i = 0; i < MAX_TCP_SOCKETS; i++) {
        tcp_socket_t *s = &sockets[i];
        if (s->state == TCP_STATE_CLOSED) continue;
        if (s->local_port != dst_port) continue;
        if (s->state == TCP_STATE_SYN_SENT) {
            if ((flags & (TCP_FLAG_SYN | TCP_FLAG_ACK)) == (TCP_FLAG_SYN | TCP_FLAG_ACK)) {
                s->ack         = seq + 1;
                s->seq         = ack;
                s->state       = TCP_STATE_ESTABLISHED;
                s->remote_ip   = src;
                s->remote_port = src_port;
                tcp_send_flags(s, TCP_FLAG_ACK, 0, 0);
            }
            return;
        }
        if (s->state == TCP_STATE_ESTABLISHED) {
            if (flags & TCP_FLAG_RST) {
                s->state = TCP_STATE_CLOSED;
                return;
            }
            if (flags & TCP_FLAG_FIN) {
                s->ack = seq + 1;
                tcp_send_flags(s, TCP_FLAG_ACK | TCP_FLAG_FIN, 0, 0);
                s->state = TCP_STATE_CLOSED;
                return;
            }
            if (dlen > 0) {
                s->ack = seq + dlen;
                for (uint16_t j = 0; j < dlen; j++) {
                    s->rx_buf[s->rx_head] = data[j];
                    s->rx_head = (uint16_t)((s->rx_head + 1) % sizeof(s->rx_buf));
                }
                tcp_send_flags(s, TCP_FLAG_ACK, 0, 0);
            }
            return;
        }
    }
}
