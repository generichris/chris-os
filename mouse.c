#include "mouse.h"
#include "irq.h"
#include "kernel.h"
#include "vesa.h"
#include "draw.h"
#include "shell.h"
#include "installer.h"

volatile int mouse_x = 0;
volatile int mouse_y = 0;
volatile int mouse_b = 0;

volatile int last_dx = 0;
volatile int last_dy = 0;

static uint8_t mouse_cycle = 0;
static uint8_t mouse_byte[3];


static uint32_t cursor_bg[20][12];
static int cursor_drawn = 0;
static int cursor_drawn_x = 0;
static int cursor_drawn_y = 0;

static inline uint8_t inb(uint16_t port) {
    uint8_t v; asm volatile("inb %%dx, %%al" : "=a"(v) : "d"(port)); return v;
}
static inline void outb(uint16_t port, uint8_t v) {
    asm volatile("outb %%al, %%dx" : : "a"(v), "d"(port));
}

static void mouse_wait(uint8_t a_type) {
    uint32_t timeout = 100000;
    if (a_type == 0) {
        while (timeout--) {
            if ((inb(0x64) & 1) == 1) return;  // wait for output buffer full
        }
    } else {
        while (timeout--) {
            if ((inb(0x64) & 2) == 0) return;  // wait for input buffer empty
        }
    }
}
static void mouse_write(uint8_t a_write) {
    mouse_wait(1);
    outb(0x64, 0xD4);
    mouse_wait(1);
    outb(0x60, a_write);
}

static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(0x60);
}

static void mouse_handler(struct registers regs) {
    (void)regs;
    
    
    uint8_t d = inb(0x60);
    
    
    if (mouse_cycle == 0) {
        if ((d & 0x08) == 0 || d == 0xFA) return;
    }
    
    mouse_byte[mouse_cycle++] = d;

    if (mouse_cycle == 3) {
        mouse_cycle = 0;

        int prev_b = mouse_b;
        mouse_b = mouse_byte[0] & 0x07;
        
        int dx = (int)mouse_byte[1];
        int dy = (int)mouse_byte[2];

        
        if (mouse_byte[0] & 0x10) dx |= 0xFFFFFF00;
        if (mouse_byte[0] & 0x20) dy |= 0xFFFFFF00;

        last_dx = dx;
        last_dy = dy;

        mouse_x += dx;
        mouse_y -= dy; 

        if (fb_active) {
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_x >= (int)fb_width) mouse_x = fb_width - 1;
            if (mouse_y >= (int)fb_height) mouse_y = fb_height - 1;

            
            if ((mouse_b & 1) && !(prev_b & 1)) {
                
                int sh = fb_height;
                if (mouse_x >= 4 && mouse_x <= 4 + 72 && mouse_y >= sh - 36 + 4 && mouse_y <= sh - 36 + 4 + 28) {
                    installer_start();
                }
            }
        }
    }
}

void mouse_init(void) {
    // Flush output buffer
    while (inb(0x64) & 1) inb(0x60);

    // Enable auxiliary device
    mouse_wait(1);
    if ((inb(0x64) & 2)) return;  // controller not responding, bail out
    outb(0x64, 0xA8);

    // Enable mouse interrupt in command byte
    mouse_wait(1);
    outb(0x64, 0x20);
    uint8_t status = mouse_read() | 2;
    status &= ~0x20;
    mouse_wait(1);
    outb(0x64, 0x60);
    mouse_wait(1);
    outb(0x60, status);

    // Set defaults
    mouse_write(0xF6);
    uint8_t ack = mouse_read();
    if (ack != 0xFA) return;  // no ACK, bail

    // Enable data reporting
    mouse_write(0xF4);
    ack = mouse_read();
    if (ack != 0xFA) return;

    irq_install_handler(12, mouse_handler);
}

void mouse_tick(void) {
    if (!fb_active) return;
    
    static int initialized = 0;
    if (!initialized) {
        mouse_x = fb_width / 2;
        mouse_y = fb_height / 2;
        initialized = 1;
    }
    
    
    if (cursor_drawn) {
        for (int row = 0; row < 20; row++) {
            for (int col = 0; col < 12; col++) {
                fb_put_pixel(cursor_drawn_x + col, cursor_drawn_y + row, cursor_bg[row][col]);
            }
        }
    }
    
    
    cursor_drawn_x = mouse_x;
    cursor_drawn_y = mouse_y;
    for (int row = 0; row < 20; row++) {
        for (int col = 0; col < 12; col++) {
            cursor_bg[row][col] = fb_get_pixel(cursor_drawn_x + col, cursor_drawn_y + row);
        }
    }
    
    
    draw_cursor(cursor_drawn_x, cursor_drawn_y);
    cursor_drawn = 1;
}
