#include "shell.h"
#include "../kernel/kernel.h"
#include "../kernel/mm.h"
#include "../kernel/kprintf.h"
#include "../drivers/ata.h"
#include "../drivers/vesa.h"
#include "../ui/draw.h"
#include "../ui/gterm.h"
#include "../fs/fat32.h"
#include "../fs/installer.h"
#include "../fs/elf.h"
#include "../net/eth.h"
#include "../net/icmp.h"
#include "../net/dns.h"
#include "../net/http.h"
#include "../net/dhcp.h"
#include "../net/net_types.h"

#define INPUT_SIZE 256

static char input[INPUT_SIZE];
static int  input_len = 0;

static char history[INPUT_SIZE];
static int  history_len = 0;
static int  cursor_pos  = 0;

static void set_fg(uint32_t gfx_color, uint8_t vga_attr) {
    if (fb_active) {
        gterm_cur_fg = gfx_color;
        (void)vga_attr;
    } else {
        terminal_setcolor(vga_attr);
    }
}

static void print_ip(ipv4_addr_t ip) {
    char buf[4];
    for (int i = 0; i < 4; i++) {
        itoa(ip.bytes[i], buf);
        terminal_writestring(buf);
        if (i < 3) terminal_putchar('.');
    }
}

static void print_mac(mac_addr_t mac) {
    const char *hex = "0123456789ABCDEF";
    for (int i = 0; i < 6; i++) {
        terminal_putchar(hex[mac.bytes[i] >> 4]);
        terminal_putchar(hex[mac.bytes[i] & 0xF]);
        if (i < 5) terminal_putchar(':');
    }
}

static int is_digit(char c) { return c >= '0' && c <= '9'; }

static ipv4_addr_t parse_ip(const char *s) {
    ipv4_addr_t ip = {{0,0,0,0}};
    int octet = 0, val = 0;
    while (*s && octet < 4) {
        if (is_digit(*s)) {
            val = val * 10 + (*s - '0');
        } else if (*s == '.' || *s == 0) {
            ip.bytes[octet++] = (uint8_t)val;
            val = 0;
        }
        s++;
    }
    if (octet < 4) ip.bytes[octet] = (uint8_t)val;
    return ip;
}

static int is_ip(const char *s) {
    int dots = 0;
    while (*s) { if (*s == '.') dots++; s++; }
    return dots == 3;
}

void shell_init(void) {
    set_fg(COLOR_PROMPT, make_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("chris os> ");
    set_fg(COLOR_TERM_FG, make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void shell_process_char(char c) {
    if (installer_is_active()) {
        installer_handle_char(c);
        return;
    }
    if (c == '\n') {
        terminal_putchar('\n');
        shell_execute(input, input_len);
        input_len = 0;
        input[0]  = 0;
        cursor_pos = 0;
        set_fg(COLOR_PROMPT, make_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("chris os> ");
        set_fg(COLOR_TERM_FG, make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    } else if (c == '\b') {
        if (input_len > 0) {
            input_len--;
            input[input_len] = 0;
            if (fb_active) {
                gterm_putchar('\b');
            } else {
                if (terminal_column > 0) {
                    terminal_column--;
                    terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
                    move_cursor(terminal_column, terminal_row);
                }
            }
        }
    } else {
        if (input_len < INPUT_SIZE - 1) {
            input[input_len++] = c;
            input[input_len]   = 0;
            terminal_putchar(c);
        }
    }
}

void shell_execute(char* cmd, int len) {
    if (len == 0) return;

    for (int i = 0; i < len; i++) history[i] = cmd[i];
    history_len = len;
    history[len] = 0;

    if (strcmp(cmd, "help") == 0) {
        terminal_writestring("commands:\n");
        terminal_writestring("  help, clear, echo, version, meminfo, uptime, reboot\n");
        terminal_writestring("  ls, cat, touch, rm, write, exec\n");
        terminal_writestring("  disk, ui, install\n");
        terminal_writestring("  ping <host|ip>, ifconfig, nslookup <host>, wget <url>, dhcp\n");

    } else if (strcmp(cmd, "install") == 0) {
        installer_start();
        return;

    } else if (strcmp(cmd, "clear") == 0) {
        terminal_initialize();
        return;

    } else if (strcmp(cmd, "version") == 0) {
        terminal_writestring("ChrisOS v0.7 (net build)\n");

    } else if (strncmp(cmd, "echo ", 5) == 0) {
        terminal_writestring(cmd + 5);
        terminal_putchar('\n');

    } else if (strcmp(cmd, "meminfo") == 0) {
        char buf[16];
        terminal_writestring("used: "); itoa(mm_used(), buf); terminal_writestring(buf);
        terminal_writestring(" bytes\nfree: "); itoa(mm_free(), buf); terminal_writestring(buf);
        terminal_writestring(" bytes\n");

    } else if (strcmp(cmd, "uptime") == 0) {
        char buf[16];
        terminal_writestring("uptime: ");
        itoa(get_ticks() / 18, buf);
        terminal_writestring(buf);
        terminal_writestring(" seconds\n");

    } else if (strcmp(cmd, "reboot") == 0) {
        terminal_writestring("rebooting...\n");
        asm volatile("int $0x19");

    } else if (strcmp(cmd, "ls") == 0) {
        fat32_ls();

    } else if (strncmp(cmd, "cat ", 4) == 0) {
        uint8_t* buf = kmalloc(4096);
        int size = fat32_read_file(cmd + 4, buf);
        if (size) {
            for (int i = 0; i < size; i++) terminal_putchar(buf[i]);
            terminal_putchar('\n');
        } else {
            terminal_writestring("file not found\n");
        }
        kfree(buf);

    } else if (strcmp(cmd, "disk") == 0) {
        uint8_t* buf = kmalloc(512);
        if (ata_read_sector(0, buf)) {
            terminal_writestring("boot sig: ");
            char hex[3]; hex[2] = 0;
            hex[0] = "0123456789ABCDEF"[buf[510] >> 4];
            hex[1] = "0123456789ABCDEF"[buf[510] & 0xF];
            terminal_writestring(hex); terminal_putchar(' ');
            hex[0] = "0123456789ABCDEF"[buf[511] >> 4];
            hex[1] = "0123456789ABCDEF"[buf[511] & 0xF];
            terminal_writestring(hex); terminal_putchar('\n');
        } else {
            terminal_writestring("disk read failed\n");
        }
        kfree(buf);

    } else if (strcmp(cmd, "ui") == 0) {
        if (fb_active) {
            terminal_writestring("ui: already in graphical mode\n");
        } else {
            vesa_init();
            if (fb_active) {
                draw_desktop();
                draw_taskbar();
                draw_window(100, 60, 400, 200, "About ChrisOS");
                draw_string(108, 96,  "ChrisOS v0.7 - VESA Graphical Mode", COLOR_WIN_TEXT, COLOR_WIN_BG);
                draw_string(108, 114, "1024 x 768 x 32bpp linear framebuffer", COLOR_WIN_TEXT, COLOR_WIN_BG);
                draw_string(108, 132, "RTL8139 network stack", COLOR_WIN_TEXT, COLOR_WIN_BG);
                gterm_init();
                shell_init();
            } else {
                terminal_writestring("ui: VESA not available\n");
            }
        }

    } else if (strncmp(cmd, "exec ", 5) == 0) {
        elf_exec(cmd + 5);

    } else if (strcmp(cmd, "ifconfig") == 0) {
        if (!eth_ready()) {
            terminal_writestring("eth0: no NIC\n");
        } else {
            terminal_writestring("eth0  MAC: ");
            print_mac(eth_get_mac());
            terminal_writestring("\n      IP:  ");
            print_ip(eth_get_ip());
            terminal_putchar('\n');
        }

    } else if (strcmp(cmd, "dhcp") == 0) {
        if (!eth_ready()) {
            terminal_writestring("dhcp: no NIC\n");
        } else {
            terminal_writestring("dhcp: sending discover...\n");
            dhcp_request();
        }

    } else if (strncmp(cmd, "ping ", 5) == 0) {
        if (!eth_ready()) {
            terminal_writestring("ping: no NIC\n");
        } else {
            const char *target = cmd + 5;
            ipv4_addr_t ip;
            if (is_ip(target)) {
                ip = parse_ip(target);
            } else {
                terminal_writestring("resolving ");
                terminal_writestring(target);
                terminal_writestring("...\n");
                ip = dns_lookup(target);
                if (ip.bytes[0] == 0 && ip.bytes[1] == 0 && ip.bytes[2] == 0 && ip.bytes[3] == 0) {
                    terminal_writestring("ping: host not found\n");
                    return;
                }
            }
            terminal_writestring("PING ");
            print_ip(ip);
            terminal_writestring(" 56 bytes\n");
            icmp_ping(ip);
        }

    } else if (strncmp(cmd, "nslookup ", 9) == 0) {
        if (!eth_ready()) {
            terminal_writestring("nslookup: no NIC\n");
        } else {
            const char *host = cmd + 9;
            terminal_writestring("resolving ");
            terminal_writestring(host);
            terminal_writestring("...\n");
            ipv4_addr_t ip = dns_lookup(host);
            terminal_writestring(host);
            terminal_writestring(" -> ");
            if (ip.bytes[0] == 0 && ip.bytes[1] == 0 && ip.bytes[2] == 0 && ip.bytes[3] == 0) {
                terminal_writestring("not found\n");
            } else {
                print_ip(ip);
                terminal_putchar('\n');
            }
        }

    } else if (strncmp(cmd, "wget ", 5) == 0) {
        if (!eth_ready()) {
            terminal_writestring("wget: no NIC\n");
        } else {
            const char *url = cmd + 5;
            terminal_writestring("fetching ");
            terminal_writestring(url);
            terminal_putchar('\n');
            http_req_t *req = http_get(url);
            if (!req) {
                terminal_writestring("wget: failed\n");
            } else {
                uint8_t *buf = kmalloc(512);
                int n;
                int total = 0;
                while ((n = http_read(req, buf, 512)) > 0) {
                    for (int i = 0; i < n; i++) terminal_putchar(buf[i]);
                    total += n;
                }
                kfree(buf);
                http_close(req);
                terminal_putchar('\n');
                char tbuf[16];
                itoa(total, tbuf);
                terminal_writestring(tbuf);
                terminal_writestring(" bytes received\n");
            }
        }

    } else {
        terminal_writestring("unknown command: ");
        terminal_writestring(cmd);
        terminal_putchar('\n');
    }
}

void shell_cursor_left(void)  { if (cursor_pos > 0) cursor_pos--; }
void shell_cursor_right(void) { if (cursor_pos < input_len) cursor_pos++; }

void shell_history_up(void) {
    if (history_len == 0) return;
    for (int i = 0; i < history_len; i++) input[i] = history[i];
    input_len  = history_len;
    input[input_len] = 0;
    cursor_pos = input_len;
}

void shell_history_down(void) {
    input_len  = 0;
    input[0]   = 0;
    cursor_pos = 0;
}
