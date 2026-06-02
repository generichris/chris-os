#include "mm.h"
#include <stdint.h>

#define HEAP_START  0x200000
#define HEAP_SIZE   0x100000
#define ALIGN       8

typedef struct block {
    size_t        size;
    uint32_t      free;
    struct block* next;
} block_t;

#define BLOCK_HDR sizeof(block_t)

static block_t* heap_head = 0;

static size_t align_up(size_t n) {
    return (n + ALIGN - 1) & ~(size_t)(ALIGN - 1);
}

void mm_init() {
    heap_head        = (block_t*)HEAP_START;
    heap_head->size  = HEAP_SIZE - BLOCK_HDR;
    heap_head->free  = 1;
    heap_head->next  = 0;
}

void* kmalloc(size_t size) {
    if (!size) return 0;
    size = align_up(size);

    block_t* b = heap_head;
    while (b) {
        if (b->free && b->size >= size) {
            if (b->size >= size + BLOCK_HDR + ALIGN) {
                block_t* split  = (block_t*)((uint8_t*)b + BLOCK_HDR + size);
                split->size     = b->size - size - BLOCK_HDR;
                split->free     = 1;
                split->next     = b->next;
                b->next         = split;
                b->size         = size;
            }
            b->free = 0;
            return (uint8_t*)b + BLOCK_HDR;
        }
        b = b->next;
    }
    return 0;
}

void kfree(void* ptr) {
    if (!ptr) return;

    block_t* b = (block_t*)((uint8_t*)ptr - BLOCK_HDR);
    b->free = 1;

    block_t* cur = heap_head;
    while (cur) {
        if (cur->free && cur->next && cur->next->free) {
            cur->size += BLOCK_HDR + cur->next->size;
            cur->next  = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

uint32_t mm_used() {
    uint32_t used = 0;
    block_t* b = heap_head;
    while (b) {
        if (!b->free) used += BLOCK_HDR + b->size;
        b = b->next;
    }
    return used;
}

uint32_t mm_free() {
    uint32_t free = 0;
    block_t* b = heap_head;
    while (b) {
        if (b->free) free += b->size;
        b = b->next;
    }
    return free;
}
