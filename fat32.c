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

static void write_sector(uint32_t lba, uint8_t* buf) {
    ata_write_sector(FAT32_START_LBA + lba, buf);
}

static void read_cluster(uint32_t cluster, uint8_t* buf) {
    uint32_t lba = data_start + (cluster - 2) * bpb.sectors_per_cluster;
    for (int i = 0; i < bpb.sectors_per_cluster; i++)
        read_sector(lba + i, buf + i * 512);
}

static void write_cluster(uint32_t cluster, uint8_t* buf) {
    uint32_t lba = data_start + (cluster - 2) * bpb.sectors_per_cluster;
    for (int i = 0; i < bpb.sectors_per_cluster; i++)
        write_sector(lba + i, buf + i * 512);
}



static uint32_t fat_get(uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start + fat_offset / 512;
    uint32_t off        = fat_offset % 512;
    uint8_t* buf = kmalloc(512);
    read_sector(fat_sector, buf);
    return *(uint32_t*)(buf + off) & 0x0FFFFFFF;
}

static void fat_set(uint32_t cluster, uint32_t value) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fat_start + fat_offset / 512;
    uint32_t off        = fat_offset % 512;
    uint8_t* buf = kmalloc(512);
    read_sector(fat_sector, buf);
    uint32_t existing = *(uint32_t*)(buf + off) & 0xF0000000;
    *(uint32_t*)(buf + off) = existing | (value & 0x0FFFFFFF);
    write_sector(fat_sector, buf);
    
    if (bpb.fat_count > 1)
        write_sector(fat_sector + bpb.sectors_per_fat_32, buf);
}


static uint32_t fat_alloc_cluster(void) {
    for (uint32_t s = 0; s < bpb.sectors_per_fat_32; s++) {
        uint8_t* buf = kmalloc(512);
        read_sector(fat_start + s, buf);
        for (uint32_t i = 0; i < 128; i++) {   
            uint32_t cluster = s * 128 + i;
            if (cluster < 2) continue;
            uint32_t val = *(uint32_t*)(buf + i * 4) & 0x0FFFFFFF;
            if (val == 0x00000000) {
                
                *(uint32_t*)(buf + i * 4) =
                    (*(uint32_t*)(buf + i * 4) & 0xF0000000) | 0x0FFFFFFF;
                write_sector(fat_start + s, buf);
                if (bpb.fat_count > 1)
                    write_sector(fat_start + bpb.sectors_per_fat_32 + s, buf);
                return cluster;
            }
        }
    }
    return 0;  
}


static void fat_free_chain(uint32_t cluster) {
    while (cluster >= 2 && cluster < 0x0FFFFFF8) {
        uint32_t next = fat_get(cluster);
        fat_set(cluster, 0x00000000);
        cluster = next;
    }
}



static void name_to_fat83(const char* name, uint8_t fatname[11]) {
    for (int i = 0; i < 11; i++) fatname[i] = ' ';
    int j = 0;
    const char* p = name;
    while (*p && *p != '.' && j < 8) {
        char c = *p++;
        if (c >= 'a' && c <= 'z') c -= 32;
        fatname[j++] = c;
    }
    if (*p == '.') {
        p++;
        int e = 8;
        while (*p && e < 11) {
            char c = *p++;
            if (c >= 'a' && c <= 'z') c -= 32;
            fatname[e++] = c;
        }
    }
}




static int find_dir_entry(uint8_t* dirbuf, uint32_t cluster_size,
                           const uint8_t fatname[11], int* free_off) {
    if (free_off) *free_off = -1;
    for (uint32_t off = 0; off < cluster_size; off += 32) {
        uint8_t first = dirbuf[off];
        if (first == 0x00) {
            if (free_off && *free_off < 0) *free_off = (int)off;
            break;  
        }
        if (first == 0xE5) {
            if (free_off && *free_off < 0) *free_off = (int)off;
            continue;
        }
        fat32_entry_t* e = (fat32_entry_t*)(dirbuf + off);
        if (e->attributes == 0x0F) continue;  

        int match = 1;
        for (int i = 0; i < 11; i++)
            if (fatname[i] != e->name[i]) { match = 0; break; }
        if (match) return (int)off;
    }
    return -1;
}


static void flush_dir_sector(uint8_t* dirbuf, uint32_t off) {
    uint32_t base_lba =
        data_start + (root_cluster - 2) * bpb.sectors_per_cluster;
    uint32_t sec = off / 512;
    write_sector(base_lba + sec, dirbuf + sec * 512);
}



void fat32_init(void) {
    uint8_t* buf = kmalloc(512);
    read_sector(0, buf);
    bpb = *(fat32_bpb_t*)buf;

    if (bpb.signature != 0x28 && bpb.signature != 0x29) {
        terminal_writestring("fat32: bad signature\n");
        return;
    }
    fat_start  = bpb.reserved_sectors;
    data_start = fat_start + bpb.fat_count * bpb.sectors_per_fat_32;
    root_cluster = bpb.root_cluster;
    terminal_writestring("fat32: mounted\n");
}

void fat32_ls(void) {
    uint32_t cluster_size = bpb.sectors_per_cluster * 512;
    uint8_t* buf = kmalloc(cluster_size);
    read_cluster(root_cluster, buf);

    uint32_t offset = 0;
    int count = 0;
    while (offset < cluster_size) {
        fat32_entry_t* entry = (fat32_entry_t*)(buf + offset);
        if (entry->name[0] == 0x00) break;
        if (entry->name[0] == 0xE5) { offset += 32; continue; }
        if (entry->attributes == 0x0F) { offset += 32; continue; }

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
        
        terminal_writestring("  (");
        char szb[12]; itoa(entry->size, szb);
        terminal_writestring(szb);
        terminal_writestring(" B)\n");

        count++;
        offset += 32;
    }
    if (count == 0) terminal_writestring("(empty)\n");
}

int fat32_read_file(const char* name, uint8_t* buf) {
    uint8_t fatname[11];
    name_to_fat83(name, fatname);

    uint32_t cluster_size = bpb.sectors_per_cluster * 512;
    uint8_t* dbuf = kmalloc(cluster_size);
    read_cluster(root_cluster, dbuf);

    int off = find_dir_entry(dbuf, cluster_size, fatname, 0);
    if (off < 0) return 0;

    fat32_entry_t* entry = (fat32_entry_t*)(dbuf + off);
    uint32_t cluster   = ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
    uint32_t remaining = entry->size;

    while (cluster >= 2 && cluster < 0x0FFFFFF8 && remaining > 0) {
        read_cluster(cluster, buf);
        uint32_t chunk = (remaining > cluster_size) ? cluster_size : remaining;
        buf       += chunk;
        remaining -= chunk;
        cluster    = fat_get(cluster);
    }
    return (int)entry->size;
}

int fat32_create_file(const char* name) {
    uint8_t fatname[11];
    name_to_fat83(name, fatname);

    uint32_t cluster_size = bpb.sectors_per_cluster * 512;
    uint8_t* buf = kmalloc(cluster_size);
    read_cluster(root_cluster, buf);

    int free_off;
    int existing = find_dir_entry(buf, cluster_size, fatname, &free_off);
    if (existing >= 0) {
        terminal_writestring("touch: file already exists\n");
        return 0;
    }
    if (free_off < 0) {
        terminal_writestring("touch: directory full\n");
        return 0;
    }

    fat32_entry_t* entry = (fat32_entry_t*)(buf + free_off);
    for (int i = 0; i < 11; i++) entry->name[i] = fatname[i];
    entry->attributes    = 0x20;
    entry->reserved      = 0;
    entry->created_tenths = 0;
    entry->created_time  = 0;
    entry->created_date  = 0;
    entry->accessed_date = 0;
    entry->cluster_high  = 0;
    entry->modified_time = 0;
    entry->modified_date = 0;
    entry->cluster_low   = 0;
    entry->size          = 0;

    flush_dir_sector(buf, (uint32_t)free_off);
    return 1;
}


int fat32_write_file(const char* name, const uint8_t* data, uint32_t size) {
    uint8_t fatname[11];
    name_to_fat83(name, fatname);

    uint32_t cluster_size = bpb.sectors_per_cluster * 512;
    if (size > cluster_size) {
        terminal_writestring("write: file too large (max one cluster)\n");
        return 0;
    }

    uint8_t* dirbuf = kmalloc(cluster_size);
    read_cluster(root_cluster, dirbuf);

    int free_off;
    int dir_off = find_dir_entry(dirbuf, cluster_size, fatname, &free_off);

    uint32_t data_cluster = 0;

    if (dir_off >= 0) {
        
        fat32_entry_t* old = (fat32_entry_t*)(dirbuf + dir_off);
        uint32_t old_cluster =
            ((uint32_t)old->cluster_high << 16) | old->cluster_low;
        if (old_cluster >= 2) fat_free_chain(old_cluster);
    } else {
        
        dir_off = free_off;
        if (dir_off < 0) {
            terminal_writestring("write: directory full\n");
            return 0;
        }
    }

    
    if (size > 0) {
        data_cluster = fat_alloc_cluster();
        if (!data_cluster) {
            terminal_writestring("write: disk full\n");
            return 0;
        }
        
        uint8_t* cbuf = kmalloc(cluster_size);
        for (uint32_t i = 0; i < cluster_size; i++) cbuf[i] = 0;
        for (uint32_t i = 0; i < size; i++)          cbuf[i] = data[i];
        write_cluster(data_cluster, cbuf);
    }

    
    fat32_entry_t* entry = (fat32_entry_t*)(dirbuf + dir_off);
    for (int i = 0; i < 11; i++) entry->name[i] = fatname[i];
    entry->attributes    = 0x20;
    entry->reserved      = 0;
    entry->created_tenths = 0;
    entry->created_time  = 0;
    entry->created_date  = 0;
    entry->accessed_date = 0;
    entry->cluster_high  = (uint16_t)(data_cluster >> 16);
    entry->modified_time = 0;
    entry->modified_date = 0;
    entry->cluster_low   = (uint16_t)(data_cluster & 0xFFFF);
    entry->size          = size;

    flush_dir_sector(dirbuf, (uint32_t)dir_off);
    return 1;
}


int fat32_delete_file(const char* name) {
    uint8_t fatname[11];
    name_to_fat83(name, fatname);

    uint32_t cluster_size = bpb.sectors_per_cluster * 512;
    uint8_t* dirbuf = kmalloc(cluster_size);
    read_cluster(root_cluster, dirbuf);

    int dir_off = find_dir_entry(dirbuf, cluster_size, fatname, 0);
    if (dir_off < 0) return 0;

    fat32_entry_t* entry = (fat32_entry_t*)(dirbuf + dir_off);
    uint32_t cluster = ((uint32_t)entry->cluster_high << 16) | entry->cluster_low;
    if (cluster >= 2) fat_free_chain(cluster);

    
    dirbuf[dir_off] = 0xE5;
    flush_dir_sector(dirbuf, (uint32_t)dir_off);
    return 1;
}