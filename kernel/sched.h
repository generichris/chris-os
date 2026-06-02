#ifndef SCHED_H
#define SCHED_H

#include <stdint.h>
#include <stddef.h>
#include "isr.h"

#define SCHED_STACK_SIZE  8192
#define SCHED_MAX_THREADS 16

typedef enum {
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_DEAD
} thread_state_t;

typedef struct thread {
    uint32_t       esp;
    uint32_t*      stack_base;
    void          (*entry)(void);
    thread_state_t state;
    uint32_t       id;
} thread_t;

void sched_init(void);
int  sched_spawn(void (*entry)(void));
void sched_yield(void);
void sched_exit(void);
void sched_tick(struct registers regs);

#endif
