#ifndef ATA_H
#define ATA_H

#include <stdint.h>

void    ata_init();
uint8_t ata_read_sector(uint32_t lba, uint8_t* buf);
uint8_t ata_write_sector(uint32_t lba, uint8_t* buf);


int     ata_detect(uint8_t drive);
uint8_t ata_read_sector_ex(uint8_t drive, uint32_t lba, uint8_t* buf);
uint8_t ata_write_sector_ex(uint8_t drive, uint32_t lba, uint8_t* buf);

#endif