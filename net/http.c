#include "http.h"
#include "dns.h"
#include "tcp.h"
#include "../kernel/mm.h"
#include "../kernel/kprintf.h"
#include <stdint.h>
#include <stddef.h>

static int kstrlen(const char *s) { int n = 0; while (s[n]) n++; return n; }
static int kstrncmp(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        if (!a[i] && !b[i]) return 0;
        if (a[i] != b[i]) return a[i] - b[i];
    }
    return 0;
}

static void parse_url(const char *url, char *host, char *path, uint16_t *port) {
    *port = 80;
    const char *p = url;
    if (kstrncmp(p, "http://", 7) == 0) p += 7;
    const char *slash = p;
    while (*slash && *slash != '/') slash++;
    int hlen = (int)(slash - p);
    for (int i = 0; i < hlen; i++) host[i] = p[i];
    host[hlen] = 0;
    if (*slash) {
        int plen = kstrlen(slash);
        for (int i = 0; i <= plen; i++) path[i] = slash[i];
    } else {
        path[0] = '/'; path[1] = 0;
    }
}

static void build_request(const char *host, const char *path, uint8_t *buf, uint16_t *len) {
    const char *method  = "GET ";
    const char *ver     = " HTTP/1.0\r\nHost: ";
    const char *tail    = "\r\nConnection: close\r\n\r\n";
    uint16_t pos = 0;
    for (int i = 0; method[i]; i++) buf[pos++] = (uint8_t)method[i];
    for (int i = 0; path[i];   i++) buf[pos++] = (uint8_t)path[i];
    for (int i = 0; ver[i];    i++) buf[pos++] = (uint8_t)ver[i];
    for (int i = 0; host[i];   i++) buf[pos++] = (uint8_t)host[i];
    for (int i = 0; tail[i];   i++) buf[pos++] = (uint8_t)tail[i];
    *len = pos;
}

http_req_t *http_get(const char *url) {
    char host[128];
    char path[256];
    uint16_t port;
    parse_url(url, host, path, &port);

    ipv4_addr_t ip = dns_lookup(host);
    if (ip.bytes[0] == 0 && ip.bytes[1] == 0 && ip.bytes[2] == 0 && ip.bytes[3] == 0) {
        kprintf("http: dns failed for %s\n", host);
        return 0;
    }

    tcp_socket_t *sock = tcp_open();
    if (!sock) return 0;
    if (tcp_connect(sock, ip, port) != 0) {
        kprintf("http: tcp connect failed\n");
        return 0;
    }

    uint8_t  req_buf[512];
    uint16_t req_len = 0;
    build_request(host, path, req_buf, &req_len);
    tcp_send(sock, req_buf, req_len);

    http_req_t *req = (http_req_t *)kmalloc(sizeof(http_req_t));
    req->sock         = sock;
    req->status       = 0;
    req->headers_done = 0;
    return req;
}

int http_read(http_req_t *req, uint8_t *buf, uint16_t max) {
    if (!req || req->sock->state != TCP_STATE_ESTABLISHED) return 0;
    uint8_t tmp[512];
    int n = tcp_recv(req->sock, tmp, 512);
    if (n <= 0) return 0;

    if (!req->headers_done) {
        int i = 0;
        while (i < n - 3) {
            if (tmp[i]=='\r' && tmp[i+1]=='\n' && tmp[i+2]=='\r' && tmp[i+3]=='\n') {
                req->headers_done = 1;
                i += 4;
                int body = n - i;
                if (body > max) body = max;
                for (int j = 0; j < body; j++) buf[j] = tmp[i + j];
                return body;
            }
            i++;
        }
        return 0;
    }

    if (n > max) n = max;
    for (int i = 0; i < n; i++) buf[i] = tmp[i];
    return n;
}

void http_close(http_req_t *req) {
    if (!req) return;
    tcp_close(req->sock);
    kfree(req);
}
