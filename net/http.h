#ifndef HTTP_H
#define HTTP_H

#include "tcp.h"
#include "net_types.h"
#include <stdint.h>

typedef struct {
    tcp_socket_t *sock;
    int           status;
    int           headers_done;
} http_req_t;

http_req_t *http_get(const char *url);
int         http_read(http_req_t *req, uint8_t *buf, uint16_t max);
void        http_close(http_req_t *req);

#endif
