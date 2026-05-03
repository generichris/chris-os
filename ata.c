#include "ata.h"
#include "kernel.h"

#define ATA_PRIMARY_IO  0x1F0
#define ATA_PRIMARY_CTL 0x3F6
#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECCOUNT    0x1F2
#define ATA_LBA_LOW     0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HIGH    0x1F5
#define ATA_HDDEVSEL    0x1F6
#define ATA_COMMAND     0x1F7
#define ATA_STATUS      0x1F7

#define ATA_SR_BSY      0x80
#define ATA_SR_DRQ      0x08
#define ATA_SR_ERR      0x01
#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_TIMEOUT     100000

static int ata_present = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t r; asm volatile("inb %%dx, %%al" : "=a"(r) : "d"(port)); return r;
}
static inline void outb(uint16_t port, uint8_t v) {
    asm volatile("outb %%al, %%dx" : : "a"(v), "d"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t r; asm volatile("inw %%dx, %%ax" : "=a"(r) : "d"(port)); return r;
}
static inline void outw(uint16_t port, uint16_t v) {
    asm volatile("outw %%ax, %%dx" : : "a"(v), "d"(port));
}


static int ata_wait_bsy(void) {
    for (int i = 0; i < ATA_TIMEOUT; i++) {
        if (!(inb(ATA_STATUS) & ATA_SR_BSY)) return 1;
    }
    return 0;  
}

static int ata_wait_drq(void) {
    for (int i = 0; i < ATA_TIMEOUT; i++) {
        uint8_t s = inb(ATA_STATUS);
        if (s & ATA_SR_ERR) return 0;
        if (s & ATA_SR_DRQ) return 1;
    }
    return 0;  
}

void ata_init(void) {
    ata_present = 0;

    
    outb(ATA_PRIMARY_CTL, 0x04);
    for (int i = 0; i < 10000; i++) asm volatile("nop");
    outb(ATA_PRIMARY_CTL, 0x00);
    for (int i = 0; i < 10000; i++) asm volatile("nop");

    
    outb(ATA_HDDEVSEL, 0xA0);
    for (int i = 0; i < 10000; i++) asm volatile("nop");

    
    uint8_t status = inb(ATA_STATUS);
    if (status == 0xFF) {
        terminal_writestring("No ATA bus\n");
        return;
    }

    
    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA_LOW,  0);
    outb(ATA_LBA_MID,  0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    status = inb(ATA_STATUS);
    if (status == 0) {
        terminal_writestring("No ATA drive\n");
        return;
    }

    if (!ata_wait_bsy()) {
        terminal_writestring("ATA timeout\n");
        return;
    }

    
    if (inb(ATA_LBA_MID) || inb(ATA_LBA_HIGH)) {
        terminal_writestring("ATAPI, skipping\n");
        return;
    }

    if (!ata_wait_drq()) {
        terminal_writestring("ATA no DRQ\n");
        return;
    }

    
    for (int i = 0; i < 256; i++) inw(ATA_PRIMARY_IO);

    ata_present = 1;
    terminal_writestring("ATA drive found\n");
}

uint8_t ata_read_sector(uint32_t lba, uint8_t* buf) {
    if (!ata_present) return 0;

    if (!ata_wait_bsy()) return 0;

    outb(ATA_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
    for (int i = 0; i < 100; i++) asm volatile("nop");

    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW,  (uint8_t)(lba));
    outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND,  ATA_CMD_READ);

    if (!ata_wait_drq()) return 0;

    for (int i = 0; i < 256; i++)
        ((uint16_t*)buf)[i] = inw(ATA_PRIMARY_IO);

    return 1;
}

uint8_t ata_write_sector(uint32_t lba, uint8_t* buf) {
    if (!ata_present) return 0;

    if (!ata_wait_bsy()) return 0;

    outb(ATA_HDDEVSEL, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW,  (uint8_t)(lba));
    outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND,  ATA_CMD_WRITE);

    if (!ata_wait_drq()) return 0;

    for (int i = 0; i < 256; i++)
        outw(ATA_PRIMARY_IO, ((uint16_t*)buf)[i]);

    return 1;
}

int ata_detect(uint8_t drive) {
    if (drive > 1) return 0;
    outb(ATA_HDDEVSEL, 0xA0 | (drive << 4));
    for (int i = 0; i < 10000; i++) asm volatile("nop");
    uint8_t status = inb(ATA_STATUS);
    if (status == 0xFF || status == 0) return 0;
    
    outb(ATA_SECCOUNT, 0);
    outb(ATA_LBA_LOW,  0);
    outb(ATA_LBA_MID,  0);
    outb(ATA_LBA_HIGH, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);
    status = inb(ATA_STATUS);
    if (status == 0) return 0;
    if (!ata_wait_bsy()) return 0;
    if (inb(ATA_LBA_MID) || inb(ATA_LBA_HIGH)) return 0;
    if (!ata_wait_drq()) return 0;
    for (int i = 0; i < 256; i++) inw(ATA_PRIMARY_IO);
    return 1;
}

uint8_t ata_read_sector_ex(uint8_t drive, uint32_t lba, uint8_t* buf) {
    if (!ata_wait_bsy()) return 0;
    outb(ATA_HDDEVSEL, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    for (int i = 0; i < 100; i++) asm volatile("nop");
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW,  (uint8_t)(lba));
    outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND,  ATA_CMD_READ);
    if (!ata_wait_drq()) return 0;
    for (int i = 0; i < 256; i++) ((uint16_t*)buf)[i] = inw(ATA_PRIMARY_IO);
    return 1;
}

uint8_t ata_write_sector_ex(uint8_t drive, uint32_t lba, uint8_t* buf) {
    if (!ata_wait_bsy()) return 0;
    outb(ATA_HDDEVSEL, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F));
    for (int i = 0; i < 100; i++) asm volatile("nop");
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW,  (uint8_t)(lba));
    outb(ATA_LBA_MID,  (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND,  ATA_CMD_WRITE);
    if (!ata_wait_drq()) return 0;
    for (int i = 0; i < 256; i++) outw(ATA_PRIMARY_IO, ((uint16_t*)buf)[i]);
    return 1;
}