#include "ata.h"
#include "kernel.h"

#define ATA_PRIMARY_IO  0x1F0
#define ATA_STATUS      0x1F7
#define ATA_HDDEVSEL    0x1F6
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_SECCOUNT    0x1F2
#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30

#define ATA_SR_BSY      0x80
#define ATA_SR_DRQ      0x08
#define ATA_SR_ERR      0x01

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %%al, %%dx" : : "a"(value), "d"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t result;
    asm volatile("inw %%dx, %%ax" : "=a"(result) : "d"(port));
    return result;
}

static inline void outw(uint16_t port, uint16_t value) {
    asm volatile("outw %%ax, %%dx" : : "a"(value), "d"(port));
}

static void ata_wait() {
    while (inb(ATA_STATUS) & ATA_SR_BSY);
}

void ata_init() {
    // Software reset
    outb(0x3F6, 0x04);
    for (int i = 0; i < 10000; i++) asm volatile("nop");
    outb(0x3F6, 0x00);
    for (int i = 0; i < 10000; i++) asm volatile("nop");

    // Select master drive
    outb(ATA_HDDEVSEL, 0xA0);
    for (int i = 0; i < 10000; i++) asm volatile("nop");

    // Send IDENTIFY
    outb(ATA_STATUS, 0xEC);
    ata_wait();

    uint8_t status = inb(ATA_STATUS);
    if (status == 0) {
        terminal_writestring("No ATA drive found\n");
        return;
    }

    // Drain the IDENTIFY data so DRQ is cleared before future reads
    if (status & ATA_SR_DRQ) {
        for (int i = 0; i < 256; i++)
            inw(ATA_PRIMARY_IO);
    }

    terminal_writestring("ATA drive found\n");
}

uint8_t ata_read_sector(uint32_t lba, uint8_t* buf) {
    ata_wait();

    outb(ATA_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
    for (int i = 0; i < 100; i++) asm volatile("nop");

    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW,  (uint8_t)(lba));
    outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_STATUS,   ATA_CMD_READ);

    uint8_t s;
    do {
        s = inb(ATA_STATUS);
    } while ((s & ATA_SR_BSY) || !(s & ATA_SR_DRQ));

    for (int i = 0; i < 256; i++)
        ((uint16_t*)buf)[i] = inw(ATA_PRIMARY_IO);

    return 1;
}

uint8_t ata_write_sector(uint32_t lba, uint8_t* buf) {
    ata_wait();

    outb(ATA_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW,  (uint8_t)(lba));
    outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_STATUS,   ATA_CMD_WRITE);

    while (!(inb(ATA_STATUS) & ATA_SR_DRQ));

    for (int i = 0; i < 256; i++)
        outw(ATA_PRIMARY_IO, ((uint16_t*)buf)[i]);

    return 1;
}