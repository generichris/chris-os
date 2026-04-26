#include "keyboard.h"
#include "irq.h"
#include "kernel.h"
#include "shell.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}

static char scancode_table[] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0,
    0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0,
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' '
};

static void keyboard_handler(struct registers regs) {
    (void)regs;
    uint8_t scancode = inb(0x60);

    if (scancode & 0x80)
        return;

    if (scancode == 0x0E) {
        shell_process_char('\b');
        return;
    }

    if (scancode == 0x1C) {
        shell_process_char('\n');
        return;
    }

    if (scancode >= sizeof(scancode_table))
        return;

    char c = scancode_table[scancode];
    if (c != 0)
        shell_process_char(c);
}

void keyboard_init() {
    irq_install_handler(1, keyboard_handler);
}