#include "vesa.h"
#include "kernel.h"
#include <stdint.h>

uint32_t* fb_addr   = (uint32_t*)0xFD000000;
uint32_t  fb_width  = 1024;
uint32_t  fb_height = 768;
uint32_t  fb_pitch  = 4096;
int       fb_active = 0;

void vesa_init(void) {
    // Suppress GCC's array-bounds warning by going through a local var
    uintptr_t base = 0x500;
    uint32_t addr   = *(volatile uint32_t*)(base +  0);
    uint32_t width  = *(volatile uint32_t*)(base +  4);
    uint32_t height = *(volatile uint32_t*)(base +  8);
    uint32_t bpp    = *(volatile uint32_t*)(base + 12);
    uint32_t pitch  = *(volatile uint32_t*)(base + 16);

    if (addr == 0 || bpp != 32) {
        fb_active = 0;
        terminal_writestring("VESA: not available, using text mode\n");
        return;
    }

    fb_addr   = (uint32_t*)addr;
    fb_width  = width;
    fb_height = height;
    fb_pitch  = (pitch > 0) ? pitch : width * 4;
    fb_active = 1;
    terminal_writestring("VESA: framebuffer ready\n");
}