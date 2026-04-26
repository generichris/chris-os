#include "fat32.h"
#include "ata.h"
#include "kernel.h"
#include "mm.h"

static fat32_bpb_t bpb;
static uint32_t fat_start;
static uint32_t data_start;
static uint32_t root_cluster;

static void read_sector(uint32_t lba, uint8_t* buf) {
    ata_read_sector(FAT32_START_LBA + lba, buf);
}

static void read_cluster(uint32_t cluster, uint8_t* buf) {
    uint32_t lba = data_start + (cluster - 2) * bpb.sectors_per_cluster;
    for (int i = 0; i < bpb.sectors_per_cluster; i++)
        read_sector(lba + i, buf + i * 512);
}

static uint32_t next_cluster(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start + fat_offset / 512;
    uint32_t offset = fat_offset % 512;

    uint8_t* buf = kmalloc(512);
    read_sector(fat_sector, buf);
    uint32_t next = *(uint32_t*)(buf + offset) & 0x0FFFFFFF;
    return next;
}

void fat32_init() {
    uint8_t* buf = kmalloc(512);
    read_sector(0, buf);
    bpb = *(fat32_bpb_t*)buf;

    if (bpb.signature != 0x28 && bpb.signature != 0x29) {
        terminal_writestring("fat32: bad signature\n");
        return;
    }

    fat_start = bpb.reserved_sectors;
    data_start = fat_start + bpb.fat_count * bpb.sectors_per_fat_32;
    root_cluster = bpb.root_cluster;

    terminal_writestring("fat32: mounted\n");
}

void fat32_ls() {
    uint32_t cluster_size = bpb.sectors_per_cluster * 512;
    uint8_t* buf = kmalloc(cluster_size);
    read_cluster(root_cluster, buf);

    uint32_t offset = 0;
    while (offset < cluster_size) {
        fat32_entry_t* entry = (fat32_entry_t*)(buf + offset);

        if (entry->name[0] == 0x00) break;
        if (entry->name[0] == 0xE5) { offset += 32; continue; }
        if (entry->attributes == 0x0F) { offset += 32; continue; }

        // print name
        for (int i = 0; i < 8; i++) {
            if (entry->name[i] == ' ') break;
            terminal_putchar(entry->name[i]);
        }
        if (entry->name[8] != ' ') {
            terminal_putchar('.');
            for (int i = 8; i < 11; i++) {
                if (entry->name[i] == ' ') break;
                terminal_putchar(entry->name[i]);
            }
        }
        terminal_putchar('\n');
        offset += 32;
    }
}

int fat32_read_file(const char* name, uint8_t* buf) {
    uint32_t cluster_size = bpb.sectors_per_cluster * 512;
    uint8_t* dbuf = kmalloc(cluster_size);
    read_cluster(root_cluster, dbuf);

    uint32_t offset = 0;
    while (offset < cluster_size) {
        fat32_entry_t* entry = (fat32_entry_t*)(dbuf + offset);

        if (entry->name[0] == 0x00) break;
        if (entry->name[0] == 0xE5) { offset += 32; continue; }
        if (entry->attributes == 0x0F) { offset += 32; continue; }

        // match name (FAT32 uses 8.3 format uppercase)
        char fatname[11];
        for (int i = 0; i < 11; i++) fatname[i] = ' ';
        int j = 0;
        for (int i = 0; name[i] && name[i] != '.' && j < 8; i++, j++) {
            char c = name[i];
            if (c >= 'a' && c <= 'z') c -= 32;
            fatname[j] = c;
        }
        // find extension
        const char* dot = name;
        while (*dot && *dot != '.') dot++;
        if (*dot == '.') {
            dot++;
            for (int i = 8; i < 11 && *dot; i++, dot++) {
                char c = *dot;
                if (c >= 'a' && c <= 'z') c -= 32;
                fatname[i] = c;
            }
        }

        int match = 1;
        for (int i = 0; i < 11; i++) {
            if (fatname[i] != entry->name[i]) { match = 0; break; }
        }

        if (match) {
            uint32_t cluster = ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
            uint32_t remaining = entry->size;
            while (cluster < 0x0FFFFFF8 && remaining > 0) {
                read_cluster(cluster, buf);
                buf += cluster_size;
                remaining -= (remaining > cluster_size) ? cluster_size : remaining;
                cluster = next_cluster(cluster);
            }
            return entry->size;
        }
        offset += 32;
    }
    return 0;
}