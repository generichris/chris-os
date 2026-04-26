#include "isr.h"
#include "idt.h"
#include "kernel.h"

extern void isr0();
extern void isr1();
extern void isr2();
extern void isr3();
extern void isr4();
extern void isr5();
extern void isr6();
extern void isr7();
extern void isr8();
extern void isr9();
extern void isr10();
extern void isr11();
extern void isr12();
extern void isr13();
extern void isr14();
extern void isr15();

static const char *exceptions[] = {
    "division by zero",
    "debug",
    "non-maskable interrupt",
    "breakpoint",
    "overflow",
    "out of bounds",
    "invalid opcode",
    "no FPU",
    "double fault",
    "coprocessor segment overrun",
    "invalid TSS",
    "segment not present",
    "stack fault",
    "general protection fault",
    "page fault",
    "unknown interrupt"
};

void isr_handler(struct registers regs) {
    terminal_writestring("KERNEL PANIC: ");
    terminal_writestring(exceptions[regs.int_no]);
    terminal_writestring("\n");

    // print EIP
    terminal_writestring("EIP: 0x");
    uint32_t eip = regs.eip;
    char hex[9];
    hex[8] = 0;
    for (int i = 7; i >= 0; i--) {
        int nibble = eip & 0xF;
        hex[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        eip >>= 4;
    }
    terminal_writestring(hex);
    terminal_writestring("\n");

    // print error code
    terminal_writestring("ERR: 0x");
    uint32_t err = regs.err_code;
    char hex2[9];
    hex2[8] = 0;
    for (int i = 7; i >= 0; i--) {
        int nibble = err & 0xF;
        hex2[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        err >>= 4;
    }
    terminal_writestring(hex2);
    terminal_writestring("\n");

    for(;;);
}

void isr_init() {
    idt_set_entry(0,  (uint32_t)isr0,  0x08, 0x8E);
    idt_set_entry(1,  (uint32_t)isr1,  0x08, 0x8E);
    idt_set_entry(2,  (uint32_t)isr2,  0x08, 0x8E);
    idt_set_entry(3,  (uint32_t)isr3,  0x08, 0x8E);
    idt_set_entry(4,  (uint32_t)isr4,  0x08, 0x8E);
    idt_set_entry(5,  (uint32_t)isr5,  0x08, 0x8E);
    idt_set_entry(6,  (uint32_t)isr6,  0x08, 0x8E);
    idt_set_entry(7,  (uint32_t)isr7,  0x08, 0x8E);
    idt_set_entry(8,  (uint32_t)isr8,  0x08, 0x8E);
    idt_set_entry(9,  (uint32_t)isr9,  0x08, 0x8E);
    idt_set_entry(10, (uint32_t)isr10, 0x08, 0x8E);
    idt_set_entry(11, (uint32_t)isr11, 0x08, 0x8E);
    idt_set_entry(12, (uint32_t)isr12, 0x08, 0x8E);
    idt_set_entry(13, (uint32_t)isr13, 0x08, 0x8E);
    idt_set_entry(14, (uint32_t)isr14, 0x08, 0x8E);
    idt_set_entry(15, (uint32_t)isr15, 0x08, 0x8E);
}