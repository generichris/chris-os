#include "kprintf.h"
#include "kernel.h"
#include "../drivers/serial.h"
#include <stdarg.h>
#include <stdint.h>

static void emit(char c) {
    terminal_putchar(c);
    serial_putchar(c);
}

static void emit_str(const char* s) {
    if (!s) s = "(null)";
    while (*s) emit(*s++);
}

static void emit_uint(uint32_t n, uint32_t base, int upper) {
    const char* lo = "0123456789abcdef";
    const char* up = "0123456789ABCDEF";
    const char* digits = upper ? up : lo;
    char buf[11];
    int i = 0;
    if (n == 0) { emit('0'); return; }
    while (n > 0) {
        buf[i++] = digits[n % base];
        n /= base;
    }
    while (i > 0) emit(buf[--i]);
}

static void emit_int(int32_t n) {
    if (n < 0) { emit('-'); emit_uint((uint32_t)(-n), 10, 0); }
    else        emit_uint((uint32_t)n, 10, 0);
}

static void emit_hex8(uint32_t n) {
    const char* d = "0123456789abcdef";
    for (int i = 7; i >= 0; i--)
        emit(d[(n >> (i * 4)) & 0xF]);
}

void kprintf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    while (*fmt) {
        if (*fmt != '%') { emit(*fmt++); continue; }
        fmt++;
        switch (*fmt) {
            case 'd': emit_int(va_arg(ap, int32_t));          break;
            case 'u': emit_uint(va_arg(ap, uint32_t), 10, 0); break;
            case 'x': emit_uint(va_arg(ap, uint32_t), 16, 0); break;
            case 'X': emit_uint(va_arg(ap, uint32_t), 16, 1); break;
            case 'p': emit('0'); emit('x');
                      emit_hex8(va_arg(ap, uint32_t));         break;
            case 's': emit_str(va_arg(ap, const char*));       break;
            case 'c': emit((char)va_arg(ap, int));             break;
            case '%': emit('%');                               break;
            default:  emit('%'); emit(*fmt);                   break;
        }
        fmt++;
    }

    va_end(ap);
}

void klog(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    serial_write("[LOG] ");

    while (*fmt) {
        if (*fmt != '%') { serial_putchar(*fmt++); continue; }
        fmt++;
        char buf[12];
        switch (*fmt) {
            case 'd': {
                int32_t n = va_arg(ap, int32_t);
                int i = 0;
                if (n < 0) { serial_putchar('-'); n = -n; }
                if (n == 0) { serial_putchar('0'); break; }
                while (n > 0) { buf[i++] = '0' + n % 10; n /= 10; }
                while (i > 0) serial_putchar(buf[--i]);
                break;
            }
            case 'x': {
                uint32_t n = va_arg(ap, uint32_t);
                const char* d = "0123456789abcdef";
                for (int i = 7; i >= 0; i--)
                    serial_putchar(d[(n >> (i*4)) & 0xF]);
                break;
            }
            case 's': {
                const char* s = va_arg(ap, const char*);
                if (!s) s = "(null)";
                serial_write(s);
                break;
            }
            case '%': serial_putchar('%'); break;
            default:  serial_putchar('%'); serial_putchar(*fmt); break;
        }
        fmt++;
    }

    serial_putchar('\n');
    va_end(ap);
}
