#ifndef VESA_H
#define VESA_H

#include <stdint.h>
#include <stddef.h>

extern uint32_t* fb_addr;
extern uint32_t  fb_width;
extern uint32_t  fb_height;
extern uint32_t  fb_pitch;   // bytes per row (may differ from width*4)
extern uint32_t  fb_bpp;
extern int       fb_active;

void vesa_init(void);

// fb_pitch is in bytes
static inline void fb_put_pixel(int x, int y, uint32_t color) {
    if ((unsigned)x < fb_width && (unsigned)y < fb_height) {
        if (fb_bpp == 32) {
            uint32_t stride = fb_pitch >> 2;   // bytes -> pixels
            fb_addr[y * stride + x] = color;
        } else if (fb_bpp == 24) {
            uint8_t* p = (uint8_t*)fb_addr + y * fb_pitch + x * 3;
            p[0] = color & 0xFF;
            p[1] = (color >> 8) & 0xFF;
            p[2] = (color >> 16) & 0xFF;
        }
    }
}

static inline uint32_t fb_get_pixel(int x, int y) {
    if ((unsigned)x < fb_width && (unsigned)y < fb_height) {
        if (fb_bpp == 32) {
            uint32_t stride = fb_pitch >> 2;
            return fb_addr[y * stride + x];
        } else if (fb_bpp == 24) {
            uint8_t* p = (uint8_t*)fb_addr + y * fb_pitch + x * 3;
            return p[0] | (p[1] << 8) | (p[2] << 16);
        }
    }
    return 0;
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