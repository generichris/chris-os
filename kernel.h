#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>
#include <stddef.h>

#define VGA_HEIGHT 25
#define VGA_WIDTH 80
#define VGA_MEMORY 0xB8000

int strcmp(const char* a, const char* b);
int strncmp(const char* a, const char* b, size_t n);
void move_cursor(size_t x, size_t y);

extern size_t terminal_column;
extern size_t terminal_row;
extern uint8_t terminal_color;
extern uint16_t* terminal_buffer;

void itoa(uint32_t n, char* buf);

void kstrcpy(char* dest, const char* src);

uint32_t get_ticks();

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

uint8_t make_color(enum vga_color fg, enum vga_color bg);
void terminal_setcolor(uint8_t color);
void terminal_initialize(void);
void terminal_writestring(const char* data);
void terminal_putchar(char c);
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);

#endif