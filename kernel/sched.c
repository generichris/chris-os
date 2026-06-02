#include "sched.h"
#include "mm.h"
#include "irq.h"

extern void sched_switch(uint32_t *old_esp, uint32_t new_esp);
extern void sched_jump(uint32_t new_esp);

static thread_t threads[SCHED_MAX_THREADS];
static int      current = 0;
static int      count   = 0;

static void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %%al, %%dx" : : "a"(val), "d"(port));
}

static void pit_init(uint32_t hz) {
    uint32_t div = 1193180 / hz;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(div & 0xFF));
    outb(0x40, (uint8_t)(div >> 8));
}

static void thread_trampoline(void) {
    threads[current].entry();
    sched_exit();
}

static int next_ready(void) {
    for (int i = 1; i <= count; i++) {
        int idx = (current + i) % count;
        if (threads[idx].state == THREAD_READY ||
            threads[idx].state == THREAD_RUNNING)
            return idx;
    }
    return -1;
}

void sched_init(void) {
    for (int i = 0; i < SCHED_MAX_THREADS; i++) {
        threads[i].state      = THREAD_DEAD;
        threads[i].stack_base = 0;
        threads[i].entry      = 0;
    }

    threads[0].state      = THREAD_RUNNING;
    threads[0].id         = 0;
    threads[0].stack_base = 0;
    count   = 1;
    current = 0;

    pit_init(100);
    irq_install_handler(0, sched_tick);
}

int sched_spawn(void (*entry)(void)) {
    if (count >= SCHED_MAX_THREADS) return -1;

    int id      = count;
    thread_t *t = &threads[id];

    t->stack_base = (uint32_t*)kmalloc(SCHED_STACK_SIZE);
    if (!t->stack_base) return -1;
    t->entry = entry;

    uint32_t *sp = (uint32_t*)((uint8_t*)t->stack_base + SCHED_STACK_SIZE);

    *--sp = (uint32_t)thread_trampoline;

    *--sp = 0x00000202;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;
    *--sp = 0;

    t->esp   = (uint32_t)sp;
    t->state = THREAD_READY;
    t->id    = id;

    count++;
    return id;
}

void sched_exit(void) {
    __asm__ volatile ("cli");

    if (threads[current].stack_base) {
        kfree(threads[current].stack_base);
        threads[current].stack_base = 0;
    }
    threads[current].state = THREAD_DEAD;

    int next = next_ready();
    if (next < 0) {
        __asm__ volatile ("sti");
        for (;;) __asm__ volatile ("hlt");
    }

    current = next;
    threads[next].state = THREAD_RUNNING;
    sched_jump(threads[next].esp);
}

void sched_yield(void) {
    __asm__ volatile ("cli");
    int next = next_ready();
    if (next < 0 || next == current) {
        __asm__ volatile ("sti");
        return;
    }
    int prev = current;
    threads[prev].state = THREAD_READY;
    current = next;
    threads[next].state = THREAD_RUNNING;
    __asm__ volatile ("sti");
    sched_switch(&threads[prev].esp, threads[next].esp);
}

void sched_tick(struct registers regs) {
    (void)regs;
    int next = next_ready();
    if (next < 0 || next == current) return;
    int prev = current;
    threads[prev].state = THREAD_READY;
    current = next;
    threads[next].state = THREAD_RUNNING;
    sched_switch(&threads[prev].esp, threads[next].esp);
}
