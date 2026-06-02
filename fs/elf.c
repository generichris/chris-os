#include "elf.h"
#include "fat32.h"
#include "../kernel/mm.h"
#include "../kernel/sched.h"
#include "../kernel/kprintf.h"
#include <stdint.h>

#define ELF_MAX_SIZE (256 * 1024)

static uint32_t pending_entry = 0;

static void elf_thread_entry(void) {
    void (*fn)(void) = (void (*)(void))pending_entry;
    fn();
}

int elf_load(const uint8_t* data, uint32_t size, uint32_t* entry_out) {
    if (size < sizeof(elf32_hdr_t)) {
        kprintf("elf: file too small\n");
        return -1;
    }

    const elf32_hdr_t* hdr = (const elf32_hdr_t*)data;

    if (hdr->magic != ELF_MAGIC) {
        kprintf("elf: bad magic\n");
        return -1;
    }
    if (hdr->bits != 1) {
        kprintf("elf: not 32-bit\n");
        return -1;
    }
    if (hdr->machine != EM_386) {
        kprintf("elf: not x86\n");
        return -1;
    }
    if (hdr->type != ET_EXEC) {
        kprintf("elf: not executable\n");
        return -1;
    }

    for (uint16_t i = 0; i < hdr->phnum; i++) {
        const elf32_phdr_t* ph =
            (const elf32_phdr_t*)(data + hdr->phoff + i * hdr->phentsize);

        if (ph->type != PT_LOAD) continue;
        if (ph->memsz == 0)      continue;

        uint8_t* dest = (uint8_t*)ph->vaddr;

        for (uint32_t b = 0; b < ph->filesz; b++)
            dest[b] = data[ph->offset + b];

        for (uint32_t b = ph->filesz; b < ph->memsz; b++)
            dest[b] = 0;

        klog("elf: loaded segment vaddr=%x filesz=%u memsz=%u",
             ph->vaddr, ph->filesz, ph->memsz);
    }

    *entry_out = hdr->entry;
    return 0;
}

int elf_exec(const char* filename) {
    uint8_t* buf = (uint8_t*)kmalloc(ELF_MAX_SIZE);
    if (!buf) {
        kprintf("elf: out of memory\n");
        return -1;
    }

    int sz = fat32_read_file(filename, buf);
    if (sz <= 0) {
        kprintf("elf: file not found: %s\n", filename);
        kfree(buf);
        return -1;
    }

    uint32_t entry = 0;
    if (elf_load(buf, (uint32_t)sz, &entry) < 0) {
        kfree(buf);
        return -1;
    }

    kfree(buf);

    pending_entry = entry;
    int tid = sched_spawn(elf_thread_entry);
    if (tid < 0) {
        kprintf("elf: could not spawn thread\n");
        return -1;
    }

    kprintf("elf: spawned thread %d for %s (entry=%p)\n", tid, filename, entry);
    return tid;
}
