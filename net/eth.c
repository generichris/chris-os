#include "eth.h"
#include "netstack.h"
#include "../kernel/mm.h"
#include "../kernel/kprintf.h"
#include <stdint.h>
#include <stddef.h>

#define PCI_ADDR  0xCF8
#define PCI_DATA  0xCFC

#define RTL8139_VENDOR  0x10EC
#define RTL8139_DEVICE  0x8139

#define RTL_IDR0    0x00
#define RTL_MAR0    0x08
#define RTL_TSD0    0x10
#define RTL_TSAD0   0x20
#define RTL_RBSTART 0x30
#define RTL_CMD     0x37
#define RTL_CAPR    0x38
#define RTL_CBR     0x3A
#define RTL_IMR     0x3C
#define RTL_ISR     0x3E
#define RTL_TCR     0x40
#define RTL_RCR     0x44
#define RTL_CONFIG1 0x52

/* PCI config space registers */
#define PCI_REG_COMMAND     0x04
#define PCI_REG_BAR0        0x10
#define PCI_REG_INT_LINE    0x3C   /* interrupt line (byte 0) = IRQ number */

#define RTL_CMD_RESET  0x10
#define RTL_CMD_BUFE   0x01
#define RTL_CMD_RE     0x08
#define RTL_CMD_TE     0x04

#define RTL_ISR_ROK    0x0001
#define RTL_ISR_TOK    0x0004
#define RTL_ISR_TER    0x0008
#define RTL_ISR_RER    0x0002

#define RTL_RCR_AAP        (1 << 0)
#define RTL_RCR_APM        (1 << 1)
#define RTL_RCR_AM         (1 << 2)
#define RTL_RCR_AB         (1 << 3)
#define RTL_RCR_WRAP       (1 << 7)
#define RTL_RCR_RBLEN_32K  (1 << 11)

#define RX_STAT_ROK  0x0001

#define RX_BUF_SIZE  (32768 + 16 + 1500)
#define TX_BUF_SIZE  1536
#define TX_BUF_COUNT 4

static uint32_t    nic_base  = 0;
static uint8_t     rx_buf[RX_BUF_SIZE];
static uint8_t     tx_buf[TX_BUF_COUNT][TX_BUF_SIZE];
static int         tx_cur    = 0;
static uint16_t    rx_offset = 0;
static mac_addr_t  my_mac;
static ipv4_addr_t my_ip     = {{10, 0, 2, 15}};
static int         nic_ok    = 0;
static uint8_t     nic_irq   = 11;   /* read from PCI, default 11 */

static inline void outb(uint16_t port, uint8_t v)  { asm volatile("outb %%al,%%dx"::"a"(v),"d"(port)); }
static inline void outw(uint16_t port, uint16_t v) { asm volatile("outw %%ax,%%dx"::"a"(v),"d"(port)); }
static inline void outl(uint16_t port, uint32_t v) { asm volatile("outl %%eax,%%dx"::"a"(v),"d"(port)); }
static inline uint8_t  inb(uint16_t port) { uint8_t v;  asm volatile("inb %%dx,%%al":"=a"(v):"d"(port)); return v; }
static inline uint16_t inw(uint16_t port) { uint16_t v; asm volatile("inw %%dx,%%ax":"=a"(v):"d"(port)); return v; }
static inline uint32_t inl(uint16_t port) { uint32_t v; asm volatile("inl %%dx,%%eax":"=a"(v):"d"(port)); return v; }

static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg) {
    uint32_t addr = (uint32_t)(1u << 31) | ((uint32_t)bus << 16) |
                    ((uint32_t)dev << 11) | ((uint32_t)fn << 8) | (reg & 0xFC);
    outl(PCI_ADDR, addr);
    return inl(PCI_DATA);
}

static void pci_write(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t reg, uint32_t val) {
    uint32_t addr = (uint32_t)(1u << 31) | ((uint32_t)bus << 16) |
                    ((uint32_t)dev << 11) | ((uint32_t)fn << 8) | (reg & 0xFC);
    outl(PCI_ADDR, addr);
    outl(PCI_DATA, val);
}

static int pci_find_rtl8139(uint8_t *bus_out, uint8_t *dev_out) {
    for (uint8_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read(bus, dev, 0, 0);
            uint16_t vendor = (uint16_t)(id & 0xFFFF);
            uint16_t device = (uint16_t)(id >> 16);
            if (vendor == RTL8139_VENDOR && device == RTL8139_DEVICE) {
                *bus_out = bus;
                *dev_out = dev;
                return 1;
            }
        }
    }
    return 0;
}

static void print_mac_inline(mac_addr_t m) {
    const char *h = "0123456789abcdef";
    for (int i = 0; i < 6; i++) {
        kprintf("%c%c", h[m.bytes[i] >> 4], h[m.bytes[i] & 0xF]);
        if (i < 5) kprintf(":");
    }
}

void eth_init(void) {
    uint8_t bus, dev;
    if (!pci_find_rtl8139(&bus, &dev)) {
        kprintf("eth: RTL8139 not found\n");
        return;
    }

    uint32_t bar0 = pci_read(bus, dev, 0, PCI_REG_BAR0);
    nic_base = (uint16_t)(bar0 & 0xFFFC);

    /* Enable PCI bus mastering + I/O space */
    uint32_t cmd = pci_read(bus, dev, 0, PCI_REG_COMMAND);
    pci_write(bus, dev, 0, PCI_REG_COMMAND, cmd | 0x5);

    /* Read IRQ line from PCI config space (byte at offset 0x3C) */
    uint32_t int_reg = pci_read(bus, dev, 0, PCI_REG_INT_LINE);
    nic_irq = (uint8_t)(int_reg & 0xFF);
    kprintf("eth: PCI IRQ=%d\n", nic_irq);

    outb(nic_base + RTL_CONFIG1, 0x00);

    outb(nic_base + RTL_CMD, RTL_CMD_RESET);
    int timeout = 100000;
    while ((inb(nic_base + RTL_CMD) & RTL_CMD_RESET) && timeout-- > 0);

    for (int i = 0; i < 6; i++)
        my_mac.bytes[i] = inb(nic_base + RTL_IDR0 + i);

    outl(nic_base + RTL_RBSTART, (uint32_t)(uintptr_t)rx_buf);

    outw(nic_base + RTL_IMR, RTL_ISR_ROK | RTL_ISR_RER | RTL_ISR_TOK | RTL_ISR_TER);

    outl(nic_base + RTL_RCR, RTL_RCR_AAP | RTL_RCR_APM | RTL_RCR_AM |
                              RTL_RCR_AB  | RTL_RCR_WRAP | RTL_RCR_RBLEN_32K);

    outl(nic_base + RTL_TCR, 0x00000600);

    outb(nic_base + RTL_CMD, RTL_CMD_TE | RTL_CMD_RE);

    nic_ok = 1;
    kprintf("eth: RTL8139 I/O=0x%x MAC=", nic_base);
    print_mac_inline(my_mac);
    kprintf(" IP=%d.%d.%d.%d\n",
            my_ip.bytes[0], my_ip.bytes[1], my_ip.bytes[2], my_ip.bytes[3]);
}

void eth_send(mac_addr_t dest, uint16_t type, uint8_t *payload, uint16_t len) {
    if (!nic_ok) return;
    uint16_t frame_len = (uint16_t)(ETH_HEADER_LEN + len);
    if (frame_len > TX_BUF_SIZE) return;

    eth_header_t *hdr = (eth_header_t *)tx_buf[tx_cur];
    hdr->dest = dest;
    hdr->src  = my_mac;
    hdr->type = htons(type);

    for (uint16_t i = 0; i < len; i++)
        tx_buf[tx_cur][ETH_HEADER_LEN + i] = payload[i];

    outl(nic_base + RTL_TSAD0 + tx_cur * 4, (uint32_t)(uintptr_t)tx_buf[tx_cur]);
    outl(nic_base + RTL_TSD0  + tx_cur * 4, frame_len);

    tx_cur = (tx_cur + 1) % TX_BUF_COUNT;
}

void eth_poll(void) {
    if (!nic_ok) return;
    int budget = 32;
    while (budget-- > 0) {
        uint16_t isr = inw(nic_base + RTL_ISR);
        if (!(isr & (RTL_ISR_ROK | RTL_ISR_RER))) break;
        outw(nic_base + RTL_ISR, isr);

        if (inb(nic_base + RTL_CMD) & RTL_CMD_BUFE) break;

        uint16_t pkt_status = *(volatile uint16_t *)(rx_buf + rx_offset);
        uint16_t pkt_len    = *(volatile uint16_t *)(rx_buf + rx_offset + 2);

        if (!(pkt_status & RX_STAT_ROK) || pkt_len == 0 || pkt_len > 1514) {
            /* Bad packet: resync to CBR */
            rx_offset = (uint16_t)(inw(nic_base + RTL_CBR) % 32768);
            outw(nic_base + RTL_CAPR, (uint16_t)((rx_offset - 16 + 32768) % 32768));
            break;
        }

        netstack_rx(rx_buf + rx_offset + 4, (uint16_t)(pkt_len - 4));

        /* Advance rx_offset: align to 4 bytes, wrap within 32K ring */
        rx_offset = (uint16_t)((rx_offset + pkt_len + 4 + 3) & ~3u);
        rx_offset %= 32768;
        /* CAPR must be written as (rx_offset - 16), wrapping correctly */
        outw(nic_base + RTL_CAPR, (uint16_t)((rx_offset - 16 + 32768) % 32768));
    }
}

void eth_handle_interrupt(void) {
    if (!nic_ok) return;
    eth_poll();
}

mac_addr_t  eth_get_mac(void)           { return my_mac; }
ipv4_addr_t eth_get_ip(void)            { return my_ip;  }
void        eth_set_ip(ipv4_addr_t ip)  { my_ip = ip;    }
int         eth_ready(void)             { return nic_ok;  }
uint8_t     eth_get_irq(void)           { return nic_irq; }
