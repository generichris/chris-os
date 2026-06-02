#include "syscall.h"
#include "idt.h"
#include "kernel.h"
#include "kprintf.h"
#include "sched.h"
#include "isr.h"
#include "../net/tcp.h"
#include "../net/dns.h"
#include <stdint.h>

extern void syscall_entry(void);

static tcp_socket_t *sock_table[8];

static tcp_socket_t *get_sock(uint32_t id) {
    if (id >= 8) return 0;
    return sock_table[id];
}

static int alloc_sock(tcp_socket_t *s) {
    for (int i = 0; i < 8; i++) {
        if (!sock_table[i]) { sock_table[i] = s; return i; }
    }
    return -1;
}

void syscall_handler(struct registers* regs) {
    uint32_t num = regs->eax;

    switch (num) {

        case SYS_EXIT:
            sched_exit();
            break;

        case SYS_WRITE: {
            const char* buf = (const char*)regs->ebx;
            uint32_t    len = regs->ecx;
            for (uint32_t i = 0; i < len; i++)
                terminal_putchar(buf[i]);
            regs->eax = len;
            break;
        }

        case SYS_READ: {
            regs->eax = 0;
            break;
        }

        case SYS_GETPID: {
            regs->eax = 0;
            break;
        }

        case SYS_YIELD:
            sched_yield();
            break;

        case SYS_NET_SOCKET: {
            tcp_socket_t *s = tcp_open();
            if (!s) { regs->eax = (uint32_t)-1; break; }
            int id = alloc_sock(s);
            regs->eax = (id >= 0) ? (uint32_t)id : (uint32_t)-1;
            break;
        }

        case SYS_NET_CONNECT: {
            tcp_socket_t *s = get_sock(regs->ebx);
            if (!s) { regs->eax = (uint32_t)-1; break; }
            uint8_t *ip_bytes = (uint8_t *)regs->ecx;
            ipv4_addr_t ip = {{ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3]}};
            regs->eax = (uint32_t)tcp_connect(s, ip, (uint16_t)regs->edx);
            break;
        }

        case SYS_NET_SEND: {
            tcp_socket_t *s = get_sock(regs->ebx);
            if (!s) { regs->eax = (uint32_t)-1; break; }
            regs->eax = (uint32_t)tcp_send(s, (uint8_t *)regs->ecx, (uint16_t)regs->edx);
            break;
        }

        case SYS_NET_RECV: {
            tcp_socket_t *s = get_sock(regs->ebx);
            if (!s) { regs->eax = (uint32_t)-1; break; }
            regs->eax = (uint32_t)tcp_recv(s, (uint8_t *)regs->ecx, (uint16_t)regs->edx);
            break;
        }

        case SYS_NET_CLOSE: {
            tcp_socket_t *s = get_sock(regs->ebx);
            if (s) { tcp_close(s); sock_table[regs->ebx] = 0; }
            regs->eax = 0;
            break;
        }

        case SYS_NET_RESOLVE: {
            const char  *hostname = (const char *)regs->ebx;
            ipv4_addr_t *out      = (ipv4_addr_t *)regs->ecx;
            *out = dns_lookup(hostname);
            regs->eax = 0;
            break;
        }

        default:
            kprintf("syscall: unknown %u\n", num);
            regs->eax = (uint32_t)-1;
            break;
    }
}

void syscall_init(void) {
    for (int i = 0; i < 8; i++) sock_table[i] = 0;
    idt_set_entry(0x80, (uint32_t)syscall_entry, 0x08, 0xEE);
}
