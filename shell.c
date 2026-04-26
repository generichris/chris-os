#include "shell.h"
#include "kernel.h"
#include "mm.h"
#include "ata.h"
#include "fat32.h"

#define INPUT_SIZE 256

static char input[INPUT_SIZE];
static int input_len = 0;

void shell_init() {
    terminal_setcolor(make_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("chris os> ");
    terminal_setcolor(make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void shell_process_char(char c) {
    if (c == '\n') {
        terminal_putchar('\n');
        shell_execute(input, input_len);
        input_len = 0;
        input[0] = 0;
        terminal_setcolor(make_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("chris os> ");
        terminal_setcolor(make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    } else if (c == '\b') {
        if (input_len > 0) {
            input_len--;
            input[input_len] = 0;
            if (terminal_column > 0) {
                terminal_column--;
                terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
                move_cursor(terminal_column, terminal_row);
            }
        }

    
    } else {
        if (input_len < INPUT_SIZE - 1) {
            input[input_len++] = c;
            input[input_len] = 0;
            terminal_putchar(c);
        }
    }
}

void shell_execute(char* cmd, int len) {
    if (len == 0) return;

    if (strcmp(cmd, "help") == 0) {
        terminal_writestring("commands: help, clear, echo, version, meminfo, uptime, reboot, disk, ls, cat\n");
    } else if (strcmp(cmd, "clear") == 0) {
        terminal_initialize();
        return;
    } else if (strcmp(cmd, "version") == 0) {
        terminal_writestring("     _        _       ___  ___ \n");
        terminal_writestring("  __| |_  _ _(_)___  / _ \\/ __|\n");
        terminal_writestring(" / _| ' \\| '_| (_-< | (_) \\__ \\\n");
        terminal_writestring(" \\__|_||_|_| |_/__/  \\___/|___/\n");
        terminal_writestring("\n");
        terminal_writestring("Chris OS v0.5\n");
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        terminal_writestring(cmd + 5);
        terminal_putchar('\n');
    } else if (strcmp(cmd, "meminfo") == 0) {
        char buf[16];
        terminal_writestring("used: ");
        itoa(mm_used(), buf);
        terminal_writestring(buf);
        terminal_writestring(" bytes\nfree: ");
        itoa(mm_free(), buf);
        terminal_writestring(buf);
        terminal_writestring(" bytes\n");
    } else if (strcmp(cmd, "uptime") == 0) {
        char buf[16];
        terminal_writestring("uptime: ");
        itoa(get_ticks() / 18, buf);
        terminal_writestring(buf);
        terminal_writestring(" seconds\n");
    } else if (strcmp(cmd, "reboot") == 0) {
        terminal_writestring("rebooting...\n");
        asm volatile("int $0x19");
    

    } else if (strcmp(cmd, "ls") == 0) {
    fat32_ls();
    
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        uint8_t* buf = kmalloc(4096);
        int size = fat32_read_file(cmd + 4, buf);
        if (size) {
            for (int i = 0; i < size; i++)
                terminal_putchar(buf[i]);
            terminal_putchar('\n');
        } else {
        terminal_writestring("file not found\n");
        }
    
    } else if (strcmp(cmd, "disk") == 0) {
    uint8_t* buf = kmalloc(512);
    if (ata_read_sector(0, buf)) {
        terminal_writestring("boot sig: ");
        char hex[3];
        hex[2] = 0;

        hex[0] = "0123456789ABCDEF"[buf[510] >> 4];
        hex[1] = "0123456789ABCDEF"[buf[510] & 0xF];
        terminal_writestring(hex);
        terminal_putchar(' ');

        hex[0] = "0123456789ABCDEF"[buf[511] >> 4];
        hex[1] = "0123456789ABCDEF"[buf[511] & 0xF];
        terminal_writestring(hex);
        terminal_putchar('\n');
        } else {
            terminal_writestring("disk read failed\n");
        }
    } else {
        terminal_writestring("unknown command: ");
        terminal_writestring(cmd);
        terminal_putchar('\n');
    }
}