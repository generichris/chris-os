#include "icmp.h"
#include "ipv4.h"
#include "arp.h"
#include "../kernel/kernel.h"
#include "../kernel/kprintf.h"
#include <stdint.h>

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t id;
    uint16_t seq;
} icmp_hdr_t;

#define ICMP_HDR_LEN  8
#define ICMP_DATA_LEN 32
#define ICMP_PKT_LEN  (ICMP_HDR_LEN + ICMP_DATA_LEN)

static uint16_t ping_seq = 0;
volatile int icmp_reply_received = 0;

void icmp_handle(ipv4_addr_t src, uint8_t *payload, uint16_t len) {
    if (len < ICMP_HDR_LEN) return;
    icmp_hdr_t *hdr = (icmp_hdr_t *)payload;
    if (hdr->type == ICMP_ECHO_REQUEST) {
        uint8_t buf[ICMP_PKT_LEN];
        icmp_hdr_t *reply = (icmp_hdr_t *)buf;
        reply->type     = ICMP_ECHO_REPLY;
        reply->code     = 0;
        reply->checksum = 0;
        reply->id       = hdr->id;
        reply->seq      = hdr->seq;
        uint16_t data_len = (uint16_t)(len - ICMP_HDR_LEN);
        if (data_len > ICMP_DATA_LEN) data_len = ICMP_DATA_LEN;
        for (uint16_t i = 0; i < data_len; i++)
            buf[ICMP_HDR_LEN + i] = payload[ICMP_HDR_LEN + i];
        uint16_t total = (uint16_t)(ICMP_HDR_LEN + data_len);
        reply->checksum = htons(ipv4_checksum(buf, total));
        ipv4_send(src, IP_PROTO_ICMP, buf, total);
    } else if (hdr->type == ICMP_ECHO_REPLY) {
        icmp_reply_received = 1;
        kprintf("ping: reply from %d.%d.%d.%d seq=%d\n",
                src.bytes[0], src.bytes[1], src.bytes[2], src.bytes[3],
                ntohs(hdr->seq));
    }
}

void icmp_ping(ipv4_addr_t dest) {
    uint8_t buf[ICMP_PKT_LEN];
    icmp_hdr_t *hdr = (icmp_hdr_t *)buf;
    hdr->type     = ICMP_ECHO_REQUEST;
    hdr->code     = 0;
    hdr->checksum = 0;
    hdr->id       = htons(0x1337);
    hdr->seq      = htons(ping_seq++);
    for (int i = 0; i < ICMP_DATA_LEN; i++) buf[ICMP_HDR_LEN + i] = (uint8_t)i;
    hdr->checksum = htons(ipv4_checksum(buf, ICMP_PKT_LEN));

    kprintf("ping: sending to %d.%d.%d.%d\n",
            dest.bytes[0], dest.bytes[1], dest.bytes[2], dest.bytes[3]);

    icmp_reply_received = 0;
    ipv4_send(dest, IP_PROTO_ICMP, buf, ICMP_PKT_LEN);

    uint32_t deadline = get_ticks() + 180;
    while (get_ticks() < deadline && !icmp_reply_received) {
        extern void eth_poll(void);
        eth_poll();
    }
    if (!icmp_reply_received) kprintf("ping: no reply (timeout)\n");
}