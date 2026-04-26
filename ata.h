#ifndef ATA_H
#define ATA_H

#include <stdint.h>

void ata_init();
uint8_t ata_read_sector(uint32_t lba, uint8_t* buf);
uint8_t ata_write_sector(uint32_t lba, uint8_t* buf);

#endif