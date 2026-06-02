#ifndef TCP_H
#define TCP_H

#include "net_types.h"
#include <stdint.h>

#define TCP_STATE_CLOSED      0
#define TCP_STATE_SYN_SENT    1
#define TCP_STATE_ESTABLISHED 2
#define TCP_STATE_FIN_WAIT    3

typedef struct {
    ipv4_addr_t remote_ip;
    uint16_t    remote_port;
    uint16_t    local_port;
    uint32_t    seq;
    uint32_t    ack;
    int         state;
    uint8_t     rx_buf[4096];
    uint16_t    rx_head;
    uint16_t    rx_tail;
} tcp_socket_t;

void          tcp_init(void);
void          tcp_handle(ipv4_addr_t src, uint8_t *payload, uint16_t len);
tcp_socket_t *tcp_open(void);
int           tcp_connect(tcp_socket_t *s, ipv4_addr_t dest, uint16_t port);
int           tcp_send(tcp_socket_t *s, uint8_t *data, uint16_t len);
int           tcp_recv(tcp_socket_t *s, uint8_t *buf, uint16_t max_len);
void          tcp_close(tcp_socket_t *s);

#endif
