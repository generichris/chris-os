#ifndef NETSTACK_H
#define NETSTACK_H

#include <stdint.h>

void netstack_init(void);
void netstack_rx(uint8_t *frame, uint16_t len);

#endif
