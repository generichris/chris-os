#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kernel.h"
#include "idt.h"
#include "isr.h"
#include "irq.h"
#include "keyboard.h"
#include "shell.h"
#include "mm.h"
#include "ata.h"
#include "fat32.h"
#include "vesa.h"
#include "draw.h"
#include "gterm.h"
#include "mouse.h"



size_t strlen(const char* str) {
    size_t len = 0;
    while (str[len]) len++;
    return len;
}

int strcmp(const char* a, const char* b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return *a - *b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && *b && *a == *b) { a++; b++; n--; }
    if (n == 0) return 0;
    return *a - *b;
}

void kstrcpy(char* dest, const char* src) {
    while (*src) *dest++ = *src++;
    *dest = 0;
}

void itoa(int n, char* buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return; }
    int i = 0;
    int is_neg = 0;
    if (n < 0) { is_neg = 1; n = -n; }
    char tmp[12];
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0;
    if (is_neg) buf[j++] = '-';
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}



size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | (uint16_t)color << 8;
}

void move_cursor(size_t x, size_t y) {
    if (fb_active) return;   
    uint16_t pos = y * VGA_WIDTH + x;
    asm volatile("outb %%al, %%dx" : : "a"((uint8_t)0x0F), "d"((uint16_t)0x3D4));
    asm volatile("outb %%al, %%dx" : : "a"((uint8_t)(pos & 0xFF)), "d"((uint16_t)0x3D5));
    asm volatile("outb %%al, %%dx" : : "a"((uint8_t)0x0E), "d"((uint16_t)0x3D4));
    asm volatile("outb %%al, %%dx" : : "a"((uint8_t)((pos >> 8) & 0xFF)), "d"((uint16_t)0x3D5));
}

uint8_t make_color(enum vga_color fg, enum vga_color bg) { return fg | bg << 4; }

void terminal_initialize(void) {
    if (fb_active) { gterm_clear(); return; }
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
}

void terminal_setcolor(uint8_t color) { terminal_color = color; }

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    if (fb_active) return;   
    terminal_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

static void terminal_scroll_vga(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            terminal_buffer[y * VGA_WIDTH + x] = terminal_buffer[(y+1) * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        terminal_buffer[(VGA_HEIGHT-1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
    terminal_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c) {
    if (fb_active) { gterm_putchar(c); return; }

    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else {
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) { terminal_column = 0; terminal_row++; }
    }
    if (terminal_row >= VGA_HEIGHT) terminal_scroll_vga();
    move_cursor(terminal_column, terminal_row);
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}



static uint32_t ticks = 0;
static void timer_handler(struct registers regs) { (void)regs; ticks++; }
uint32_t get_ticks(void) { return ticks; }




static void vga_debug(char c) {
    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    vga[0] = (uint16_t)(0x4F00 | (uint8_t)c);
}

void kernel(void) {
    vga_debug('A');

    volatile uint16_t* vga = (volatile uint16_t*)0xB8000;
    for (int i = 0; i < 80 * 25; i++) vga[i] = 0x0720;
    terminal_initialize();
    terminal_writestring("ChrisOS booting...\n");

    vga_debug('B'); idt_init();
    vga_debug('C'); isr_init();
    vga_debug('D'); irq_init();
    vga_debug('E'); mm_init();
    vga_debug('F'); keyboard_init();
    vga_debug('G'); ata_init();
    vga_debug('H'); fat32_init();
    vga_debug('M'); mouse_init();
    vga_debug('I'); irq_install_handler(0, timer_handler);
    asm volatile("sti");

    
    vesa_init();

    if (fb_active) {
        draw_desktop();
        draw_taskbar();
        draw_window(fb_width - 280, 40, 250, 100, "ChrisOS v0.6");
        draw_string(fb_width - 274, 76, "VESA graphical mode", COLOR_WIN_TEXT, COLOR_WIN_BG);
        draw_string(fb_width - 274, 96, "1024x768x32bpp", COLOR_PROMPT, COLOR_WIN_BG);
        gterm_init();
    }

    shell_init();

    char debug_buf[64];

    for (;;) {
        asm volatile("hlt");
        if (fb_active) {
            draw_taskbar();
            gterm_tick();
            mouse_tick();
            
            
            extern volatile int mouse_x, mouse_y, mouse_b, last_dx, last_dy;
            int len = 0;
            debug_buf[len++] = 'X'; debug_buf[len++] = ':'; 
            itoa((int)mouse_x, debug_buf + len); len = strlen(debug_buf);
            debug_buf[len++] = ' '; debug_buf[len++] = 'Y'; debug_buf[len++] = ':';
            itoa((int)mouse_y, debug_buf + len); len = strlen(debug_buf);
            debug_buf[len++] = ' '; debug_buf[len++] = 'B'; debug_buf[len++] = ':';
            itoa((int)mouse_b, debug_buf + len); len = strlen(debug_buf);
            debug_buf[len++] = ' '; debug_buf[len++] = 'D'; debug_buf[len++] = 'X'; debug_buf[len++] = ':';
            itoa((int)last_dx, debug_buf + len); len = strlen(debug_buf);
            debug_buf[len++] = ' '; debug_buf[len++] = 'D'; debug_buf[len++] = 'Y'; debug_buf[len++] = ':';
            itoa((int)last_dy, debug_buf + len);
            
            draw_rect(0, 0, 200, 20, COLOR_BLACK);
            draw_string(4, 4, debug_buf, COLOR_WHITE, COLOR_BLACK);
        }
    }
}