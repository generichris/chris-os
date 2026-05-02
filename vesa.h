#ifndef VESA_H
#define VESA_H

#include <stdint.h>
#include <stddef.h>

extern uint32_t* fb_addr;
extern uint32_t  fb_width;
extern uint32_t  fb_height;
extern uint32_t  fb_pitch;   // bytes per row (may differ from width*4)
extern int       fb_active;

void vesa_init(void);

// fb_pitch is in bytes; divide by 4 to get uint32_t stride
static inline void fb_put_pixel(int x, int y, uint32_t color) {
    if ((unsigned)x < fb_width && (unsigned)y < fb_height) {
        uint32_t stride = fb_pitch >> 2;   // bytes -> pixels
        fb_addr[y * stride + x] = color;
    }
}

static inline uint32_t rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

#define COLOR_BLACK      rgb(0,   0,   0)
#define COLOR_WHITE      rgb(255, 255, 255)
#define COLOR_DESKTOP    rgb(20,  30,  48)
#define COLOR_TASKBAR    rgb(30,  40,  60)
#define COLOR_TASKBAR_HL rgb(50,  65,  95)
#define COLOR_WIN_BG     rgb(240, 240, 245)
#define COLOR_WIN_TITLE  rgb(60,  100, 180)
#define COLOR_WIN_TEXT   rgb(20,  20,  20)
#define COLOR_PROMPT     rgb(80,  200, 120)
#define COLOR_TERM_BG    rgb(15,  15,  20)
#define COLOR_TERM_FG    rgb(200, 210, 200)
#define COLOR_CURSOR     rgb(255, 255, 255)
#define COLOR_SHADOW     rgb(0,   0,   0)

#endif