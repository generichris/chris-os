#ifndef NET_TYPES_H
#define NET_TYPES_H

#include <stdint.h>

typedef struct { uint8_t bytes[6]; } mac_addr_t;
typedef struct { uint8_t bytes[4]; } ipv4_addr_t;

static inline uint16_t htons(uint16_t v) { return (uint16_t)((v >> 8) | (v << 8)); }
static inline uint16_t ntohs(uint16_t v) { return htons(v); }
static inline uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) | (((v >> 8) & 0xFF) << 16) |
           (((v >> 16) & 0xFF) << 8) | ((v >> 24) & 0xFF);
}
static inline uint32_t ntohl(uint32_t v) { return htonl(v); }

static inline int ip_eq(ipv4_addr_t a, ipv4_addr_t b) {
    return a.bytes[0]==b.bytes[0] && a.bytes[1]==b.bytes[1] &&
           a.bytes[2]==b.bytes[2] && a.bytes[3]==b.bytes[3];
}

static inline int mac_eq(mac_addr_t a, mac_addr_t b) {
    for (int i = 0; i < 6; i++) if (a.bytes[i] != b.bytes[i]) return 0;
    return 1;
}

#define MAC_BROADCAST  ((mac_addr_t){{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}})
#define IP_ANY         ((ipv4_addr_t){{0,0,0,0}})

#endif
