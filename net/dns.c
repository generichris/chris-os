#include "dns.h"
#include "udp.h"
#include "eth.h"
#include "../kernel/kernel.h"
#include "../kernel/kprintf.h"
#include <stdint.h>
#include <stddef.h>

#define DNS_PORT    53
#define DNS_SRC     1053

static ipv4_addr_t dns_server = {{10, 0, 2, 3}};
static volatile ipv4_addr_t dns_result;
static volatile int dns_done = 0;
static uint16_t dns_tid = 0;

static void kstrncpy(char *dst, const char *src, int n) {
    int i = 0;
    while (i < n - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

static int kstrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

typedef struct {
    ipv4_addr_t ip;
    char        name[64];
    int         valid;
} dns_cache_t;

#define DNS_CACHE_SIZE 8
static dns_cache_t dns_cache[DNS_CACHE_SIZE];

static void dns_cache_store(const char *name, ipv4_addr_t ip) {
    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (!dns_cache[i].valid || kstrcmp(dns_cache[i].name, name) == 0) {
            kstrncpy(dns_cache[i].name, name, 64);
            dns_cache[i].ip    = ip;
            dns_cache[i].valid = 1;
            return;
        }
    }
    kstrncpy(dns_cache[0].name, name, 64);
    dns_cache[0].ip    = ip;
    dns_cache[0].valid = 1;
}

static int dns_cache_lookup(const char *name, ipv4_addr_t *out) {
    for (int i = 0; i < DNS_CACHE_SIZE; i++)
        if (dns_cache[i].valid && kstrcmp(dns_cache[i].name, name) == 0) {
            *out = dns_cache[i].ip;
            return 1;
        }
    return 0;
}

static void dns_rx(ipv4_addr_t src, uint16_t src_port, uint8_t *data, uint16_t len) {
    (void)src; (void)src_port;
    if (len < 12) return;
    uint16_t tid = (uint16_t)((data[0] << 8) | data[1]);
    if (tid != dns_tid) return;
    uint16_t ancount = (uint16_t)((data[6] << 8) | data[7]);
    if (ancount == 0) { dns_done = 1; return; }
    int pos = 12;
    while (pos < (int)len && data[pos]) pos += data[pos] + 1;
    pos += 5;
    for (int i = 0; i < (int)ancount && pos + 12 <= (int)len; i++) {
        if ((data[pos] & 0xC0) == 0xC0) pos += 2;
        else { while (pos < (int)len && data[pos]) pos += data[pos] + 1; pos++; }
        uint16_t rtype = (uint16_t)((data[pos] << 8) | data[pos+1]);
        uint16_t rdlen = (uint16_t)((data[pos+8] << 8) | data[pos+9]);
        pos += 10;
        if (rtype == 1 && rdlen == 4 && pos + 4 <= (int)len) {
            dns_result.bytes[0] = data[pos];
            dns_result.bytes[1] = data[pos+1];
            dns_result.bytes[2] = data[pos+2];
            dns_result.bytes[3] = data[pos+3];
            dns_done = 1;
            return;
        }
        pos += rdlen;
    }
    dns_done = 1;
}

void dns_init(void) {
    for (int i = 0; i < DNS_CACHE_SIZE; i++) dns_cache[i].valid = 0;
    udp_bind(DNS_SRC, dns_rx);
}

void dns_set_server(ipv4_addr_t server) { dns_server = server; }

ipv4_addr_t dns_lookup(const char *hostname) {
    ipv4_addr_t cached;
    if (dns_cache_lookup(hostname, &cached)) return cached;

    uint8_t pkt[512];
    int pos = 0;
    pkt[pos++] = (uint8_t)(++dns_tid >> 8);
    pkt[pos++] = (uint8_t)(dns_tid & 0xFF);
    pkt[pos++] = 0x01; pkt[pos++] = 0x00;
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;
    pkt[pos++] = 0x00; pkt[pos++] = 0x00;
    pkt[pos++] = 0x00; pkt[pos++] = 0x00;
    pkt[pos++] = 0x00; pkt[pos++] = 0x00;

    const char *p = hostname;
    while (*p) {
        const char *dot = p;
        while (*dot && *dot != '.') dot++;
        int plen = (int)(dot - p);
        pkt[pos++] = (uint8_t)plen;
        for (int i = 0; i < plen; i++) pkt[pos++] = (uint8_t)p[i];
        p = (*dot == '.') ? dot + 1 : dot;
    }
    pkt[pos++] = 0x00;
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;
    pkt[pos++] = 0x00; pkt[pos++] = 0x01;

    dns_done = 0;
    dns_result.bytes[0] = dns_result.bytes[1] = 0;
    dns_result.bytes[2] = dns_result.bytes[3] = 0;

    kprintf("dns: querying %d.%d.%d.%d for %s\n",
            dns_server.bytes[0], dns_server.bytes[1],
            dns_server.bytes[2], dns_server.bytes[3], hostname);

    udp_send(dns_server, DNS_PORT, DNS_SRC, pkt, (uint16_t)pos);

    uint32_t deadline = get_ticks() + 72;
    while (get_ticks() < deadline && !dns_done) {
        extern void eth_poll(void);
        eth_poll();
    }

    if (!dns_done) {
        kprintf("dns: timeout\n");
        ipv4_addr_t zero = {{0,0,0,0}};
        return zero;
    }

    if (dns_result.bytes[0] || dns_result.bytes[1] ||
        dns_result.bytes[2] || dns_result.bytes[3]) {
        ipv4_addr_t result = {{dns_result.bytes[0], dns_result.bytes[1],
                               dns_result.bytes[2], dns_result.bytes[3]}};
        dns_cache_store(hostname, result);
        kprintf("dns: %s -> %d.%d.%d.%d\n", hostname,
                result.bytes[0], result.bytes[1],
                result.bytes[2], result.bytes[3]);
        return result;
    }

    kprintf("dns: no A record\n");
    ipv4_addr_t zero = {{0,0,0,0}};
    return zero;
}