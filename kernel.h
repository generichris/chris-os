#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>


#define VGA_HEIGHT 25
#define VGA_WIDTH  80
#define VGA_MEMORY 0xB8000


int    strcmp (const char* a, const char* b);
int    strncmp(const char* a, const char* b, size_t n);
size_t strlen (const char* str);
void   kstrcpy(char* dest, const char* src);
char*  kstrchr(const char* s, char c);


void itoa(int n, char* buf);


void     move_cursor(size_t x, size_t y);
void     kreboot(void);
uint32_t get_ticks(void);
void     koutb(uint16_t port, uint8_t val);
uint8_t  kinb (uint16_t port);



void vga_set_80x50(void);


extern size_t    terminal_row;
extern size_t    terminal_column;
extern uint8_t   terminal_color;
extern uint16_t* terminal_buffer;


void terminal_initialize(void);
void terminal_setcolor(uint8_t color);
void terminal_writestring(const char* data);
void terminal_putchar(char c);
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);
void terminal_write(const char* data, size_t size);


void terminal_capture_start(char* buf, int max);
int  terminal_capture_stop(void);  


enum vga_color {
    VGA_COLOR_BLACK         =  0,
    VGA_COLOR_BLUE          =  1,
    VGA_COLOR_GREEN         =  2,
    VGA_COLOR_CYAN          =  3,
    VGA_COLOR_RED           =  4,
    VGA_COLOR_MAGENTA       =  5,
    VGA_COLOR_BROWN         =  6,
    VGA_COLOR_LIGHT_GREY    =  7,
    VGA_COLOR_DARK_GREY     =  8,
    VGA_COLOR_LIGHT_BLUE    =  9,
    VGA_COLOR_LIGHT_GREEN   = 10,
    VGA_COLOR_LIGHT_CYAN    = 11,
    VGA_COLOR_LIGHT_RED     = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN   = 14,
    VGA_COLOR_WHITE         = 15,
};

uint8_t make_color(enum vga_color fg, enum vga_color bg);

#endif 