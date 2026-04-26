#ifndef ISR_H
#define ISR_H

#include <stdint.h>

struct registers {
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags, useresp, ss;
};

void isr_init();
void isr_handler(struct registers regs);

#endif