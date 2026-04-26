#include "mm.h"
#include <stdint.h>

#define HEAP_START 0x200000    
#define HEAP_SIZE  0x100000    

static uint8_t* heap_ptr = (uint8_t*)HEAP_START;
static uint8_t* heap_end = (uint8_t*)(HEAP_START + HEAP_SIZE);

uint32_t mm_used() {
    return heap_ptr - (uint8_t*)HEAP_START;
}

uint32_t mm_free() {
    return heap_end - heap_ptr;
}

void mm_init() {
    heap_ptr = (uint8_t*)HEAP_START;
}

void* kmalloc(size_t size) {
    if (heap_ptr + size > heap_end)
        return 0;    // out of memory

    void* addr = heap_ptr;
    heap_ptr += size;
    return addr;
}