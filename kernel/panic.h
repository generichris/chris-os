#ifndef PANIC_H
#define PANIC_H

#include "isr.h"

#define PANIC(msg) kpanic(msg, __FILE__, __LINE__)

void kpanic(const char* msg, const char* file, int line);
void panic_isr_handler(struct registers* regs);

#endif
