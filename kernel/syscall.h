#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include "isr.h"

#define SYS_EXIT         0
#define SYS_WRITE        1
#define SYS_READ         2
#define SYS_GETPID       3
#define SYS_YIELD        4
#define SYS_NET_SOCKET   50
#define SYS_NET_CONNECT  51
#define SYS_NET_SEND     52
#define SYS_NET_RECV     53
#define SYS_NET_CLOSE    54
#define SYS_NET_RESOLVE  55

void syscall_init(void);
void syscall_handler(struct registers* regs);

#endif
