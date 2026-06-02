#ifndef ELF_H
#define ELF_H

#include <stdint.h>

#define ELF_MAGIC     0x464C457F
#define ET_EXEC       2
#define EM_386        3
#define PT_LOAD       1

typedef struct {
    uint32_t magic;
    uint8_t  bits;
    uint8_t  endian;
    uint8_t  elf_ver;
    uint8_t  os_abi;
    uint8_t  pad[8];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) elf32_hdr_t;

typedef struct {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed)) elf32_phdr_t;

int  elf_load(const uint8_t* data, uint32_t size, uint32_t* entry_out);
int  elf_exec(const char* filename);

#endif
