#include "panic.h"
#include "kernel.h"
#include "kprintf.h"
#include "../drivers/serial.h"

static const char* exception_names[] = {
    "Division By Zero",
    "Debug",
    "Non-Maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point",
};

static void paint_panic_screen(void) {
    terminal_setcolor(make_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        terminal_putchar(' ');
    terminal_row    = 1;
    terminal_column = 0;
}

static void print_bar(void) {
    terminal_setcolor(make_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    for (int i = 0; i < VGA_WIDTH; i++) terminal_putchar('=');
}

void kpanic(const char* msg, const char* file, int line) {
    __asm__ volatile ("cli");

    paint_panic_screen();
    terminal_setcolor(make_color(VGA_COLOR_WHITE, VGA_COLOR_RED));

    print_bar();
    kprintf("  *** KERNEL PANIC ***\n");
    print_bar();
    kprintf("\n  %s\n\n", msg);
    kprintf("  File : %s\n", file);
    kprintf("  Line : %d\n", line);
    kprintf("\n");
    print_bar();
    kprintf("  System halted. Restart your computer.\n");

    klog("PANIC: %s at %s:%d", msg, file, line);

    for (;;) __asm__ volatile ("hlt");
}

void panic_isr_handler(struct registers* regs) {
    __asm__ volatile ("cli");

    paint_panic_screen();
    terminal_setcolor(make_color(VGA_COLOR_WHITE, VGA_COLOR_RED));

    const char* name = "Unknown Exception";
    if (regs->int_no < 20)
        name = exception_names[regs->int_no];

    print_bar();
    kprintf("  *** KERNEL PANIC: CPU EXCEPTION ***\n");
    print_bar();
    kprintf("\n  Exception : #%u - %s\n", regs->int_no, name);
    kprintf("  Error Code: 0x%x\n\n", regs->err_code);

    kprintf("  EIP=%p  CS=0x%x  EFLAGS=0x%x\n",
            regs->eip, regs->cs, regs->eflags);
    kprintf("  EAX=%p  EBX=%p  ECX=%p  EDX=%p\n",
            regs->eax, regs->ebx, regs->ecx, regs->edx);
    kprintf("  ESI=%p  EDI=%p  EBP=%p  ESP=%p\n",
            regs->esi, regs->edi, regs->ebp, regs->esp);
    kprintf("\n");
    print_bar();
    kprintf("  System halted. Restart your computer.\n");

    klog("EXCEPTION #%u (%s) EIP=%x ERR=%x",
         regs->int_no, name, regs->eip, regs->err_code);
    klog("  EAX=%x EBX=%x ECX=%x EDX=%x",
         regs->eax, regs->ebx, regs->ecx, regs->edx);

    for (;;) __asm__ volatile ("hlt");
}
