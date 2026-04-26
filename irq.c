#include "irq.h"
#include "isr.h"
#include "idt.h"
#include "kernel.h"

#include "irq.h"
#include "isr.h"
#include "idt.h"
#include "kernel.h"

extern void irq0();
extern void irq1();
extern void irq2();
extern void irq3();
extern void irq4();
extern void irq5();
extern void irq6();
extern void irq7();
extern void irq8();
extern void irq9();
extern void irq10();
extern void irq11();
extern void irq12();
extern void irq13();
extern void irq14();
extern void irq15();

static irq_handler_t irq_handlers[16];

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile("outb %%al, %%dx" : : "a"(value), "d"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    asm volatile("inb %%dx, %%al" : "=a"(result) : "d"(port));
    return result;
}

void irq_install_handler(int irq, irq_handler_t handler) {
    irq_handlers[irq] = handler;
}

void irq_uninstall_handler(int irq) {
    irq_handlers[irq] = 0;
}

void irq_handler(struct registers regs) {
    if (regs.int_no >= 40)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);

    if (irq_handlers[regs.int_no - 32])
        irq_handlers[regs.int_no - 32](regs);
}

void irq_init() {



    outb(0x20, 0x11);
    outb(0x21, 0x20);
    outb(0x21, 0x04);
    outb(0x21, 0x01);
    outb(0x21, 0x00); 

    // remap PIC - slave
    outb(0xA0, 0x11);
    outb(0xA1, 0x28); 
    outb(0xA1, 0x02);
    outb(0xA1, 0x01);
    outb(0xA1, 0x00);  // unmask all

    idt_set_entry(32, (uint32_t)irq0,  0x08, 0x8E);
    idt_set_entry(33, (uint32_t)irq1,  0x08, 0x8E);
    idt_set_entry(34, (uint32_t)irq2,  0x08, 0x8E);
    idt_set_entry(35, (uint32_t)irq3,  0x08, 0x8E);
    idt_set_entry(36, (uint32_t)irq4,  0x08, 0x8E);
    idt_set_entry(37, (uint32_t)irq5,  0x08, 0x8E);
    idt_set_entry(38, (uint32_t)irq6,  0x08, 0x8E);
    idt_set_entry(39, (uint32_t)irq7,  0x08, 0x8E);
    idt_set_entry(40, (uint32_t)irq8,  0x08, 0x8E);
    idt_set_entry(41, (uint32_t)irq9,  0x08, 0x8E);
    idt_set_entry(42, (uint32_t)irq10, 0x08, 0x8E);
    idt_set_entry(43, (uint32_t)irq11, 0x08, 0x8E);
    idt_set_entry(44, (uint32_t)irq12, 0x08, 0x8E);
    idt_set_entry(45, (uint32_t)irq13, 0x08, 0x8E);
    idt_set_entry(46, (uint32_t)irq14, 0x08, 0x8E);
    idt_set_entry(47, (uint32_t)irq15, 0x08, 0x8E);
}