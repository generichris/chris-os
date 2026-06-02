#ifndef MM_H
#define MM_H

#include <stddef.h>
#include <stdint.h>

void  mm_init();
void* kmalloc(size_t size);
void  kfree(void* ptr);

uint32_t mm_used();
uint32_t mm_free();

#endif
