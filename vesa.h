

#ifndef VESA_H
#define VESA_H

#include <stdint.h>

typedef struct {
    uint32_t* base;    
    uint32_t  width;   
    uint32_t  height;  
    uint32_t  pitch;   
    uint8_t   bpp;     
} vesa_fb_t;

extern vesa_fb_t vesa_fb;  


void vesa_init(uint32_t* mb2_info);


static inline void vesa_putpixel(uint32_t x, uint32_t y, uint32_t colour) {
    if (x < vesa_fb.width && y < vesa_fb.height) {
        uint32_t* row = (uint32_t*)((uint8_t*)vesa_fb.base + y * vesa_fb.pitch);
        row[x] = colour;
    }
}


void vesa_clear(uint32_t colour);


void vesa_putchar(uint32_t x, uint32_t y, char ch,
                  uint32_t fg, uint32_t bg);


void vesa_writestring(uint32_t* px, uint32_t* py, const char* str,
                      uint32_t fg, uint32_t bg);

#endif 