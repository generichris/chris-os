#include "idt.h"

struct idt_entry idt[256];
struct idt_ptr idtp;         

void idt_set_entry(int num, uint32_t base, uint16_t selector, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;         
    idt[num].base_high = (base >> 16) & 0xFFFF; 
    idt[num].selector  = selector;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

void idt_init() {
    idtp.limit = (sizeof(struct idt_entry) * 256) - 1;  
    idtp.base  = (uint32_t)&idt;


    for (int i = 0; i < 256; i++)
        idt_set_entry(i, 0, 0, 0);


    asm volatile("lidt %0" : : "m"(idtp));
}