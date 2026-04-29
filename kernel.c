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



static inline uint8_t inb_k(uint16_t port) {
    uint8_t v;
    asm volatile("inb %%dx, %%al" : "=a"(v) : "d"(port));
    return v;
}
static inline void outb_k(uint16_t port, uint8_t v) {
    asm volatile("outb %%al, %%dx" : : "a"(v), "d"(port));
}


void koutb(uint16_t port, uint8_t val) { outb_k(port, val); }
uint8_t kinb(uint16_t port)            { return inb_k(port); }



void vga_set_80x50(void) {
    uint8_t v;

    
    outb_k(0x3D4, 0x11);
    v = inb_k(0x3D5);
    outb_k(0x3D5, v & ~0x80);

    
    outb_k(0x3D4, 0x09);
    v = inb_k(0x3D5);
    outb_k(0x3D5, (v & 0xE0) | 0x07);

    
    outb_k(0x3D4, 0x0A);
    v = inb_k(0x3D5);
    outb_k(0x3D5, (v & 0xC0) | 0x06);

    outb_k(0x3D4, 0x0B);
    v = inb_k(0x3D5);
    outb_k(0x3D5, (v & 0xE0) | 0x07);
}



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

char* kstrchr(const char* s, char c) {
    while (*s) {
        if (*s == c) return (char*)s;
        s++;
    }
    return 0;
}



void itoa(uint32_t n, char* buf) {
    if (n == 0) { buf[0] = '0'; buf[1] = 0; return; }
    int i = 0;
    char tmp[12];
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    int j = 0;
    while (i > 0) buf[j++] = tmp[--i];
    buf[j] = 0;
}



void move_cursor(size_t x, size_t y) {
    uint16_t pos = y * VGA_WIDTH + x;
    outb_k(0x3D4, 0x0F); outb_k(0x3D5, (uint8_t)(pos & 0xFF));
    outb_k(0x3D4, 0x0E); outb_k(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}



static uint32_t ticks = 0;

static void timer_handler(struct registers regs) {
    (void)regs;
    ticks++;
}

uint32_t get_ticks(void) { return ticks; }



size_t    terminal_row;
size_t    terminal_column;
uint8_t   terminal_color;
uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;


static char* cap_buf = 0;
static int   cap_len = 0;
static int   cap_max = 0;

void terminal_capture_start(char* buf, int max) {
    cap_buf = buf;
    cap_len = 0;
    cap_max = max;
}

int terminal_capture_stop(void) {
    int len = cap_len;
    if (cap_buf && cap_len < cap_max)
        cap_buf[cap_len] = '\0';
    cap_buf = 0; cap_len = 0; cap_max = 0;
    return len;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t)uc | (uint16_t)color << 8;
}

void terminal_initialize(void) {
    terminal_row    = 0;
    terminal_column = 0;
    terminal_color  = make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    for (size_t y = 0; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            terminal_buffer[y * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
}

void terminal_setcolor(uint8_t color) { terminal_color = color; }

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    terminal_buffer[y * VGA_WIDTH + x] = vga_entry(c, color);
}

void terminal_scroll(void) {
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            terminal_buffer[y * VGA_WIDTH + x] =
                terminal_buffer[(y + 1) * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        terminal_buffer[(VGA_HEIGHT - 1) * VGA_WIDTH + x] =
            vga_entry(' ', terminal_color);
    terminal_row = VGA_HEIGHT - 1;
}

void terminal_putchar(char c) {
    
    if (cap_buf) {
        if (cap_len < cap_max - 1)
            cap_buf[cap_len++] = c;
        return;
    }

    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
    } else {
        terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
        if (++terminal_column == VGA_WIDTH) {
            terminal_column = 0;
            terminal_row++;
        }
    }
    if (terminal_row >= VGA_HEIGHT) terminal_scroll();
    move_cursor(terminal_column, terminal_row);
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++) terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    terminal_write(data, strlen(data));
}

uint8_t make_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}



void kreboot(void) {
    uint8_t tmp;
    int timeout = 100000;
    do {
        tmp = inb_k(0x64);
        if (tmp & 1) inb_k(0x60);
    } while ((tmp & 2) && --timeout > 0);
    outb_k(0x64, 0xFE);
    asm volatile("cli");
    asm volatile("lidt (0)");
    asm volatile("int $3");
}



void kernel(void) {
    
    uint16_t* vga = (uint16_t*)0xB8000;
    for (int i = 0; i < 80 * 25; i++) vga[i] = 0x0720;

    terminal_initialize();
    idt_init();
    isr_init();
    irq_init();
    mm_init();
    keyboard_init();
    ata_init();
    fat32_init();
    irq_install_handler(0, timer_handler);

    asm volatile("sti");
    shell_init();
    for (;;) asm volatile("hlt");
}