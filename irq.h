#ifndef IRQ_H
#define IRQ_H

#include <stdint.h>
#include "isr.h"

typedef void (*irq_handler_t)(struct registers);

void irq_init();
void irq_install_handler(int irq, irq_handler_t handler);
void irq_uninstall_handler(int irq);

#endif