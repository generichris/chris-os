#include "shell.h"
#include "kernel.h"
#include "mm.h"
#include "ata.h"
#include "fat32.h"
#include "vesa.h"
#include "draw.h"
#include "gterm.h"

#define INPUT_SIZE 256

static char input[INPUT_SIZE];
static int input_len = 0;

// In graphics mode we set a "color" by swapping the gterm fg;
// in text mode we use VGA color attributes as before.
static void set_fg(uint32_t gfx_color, uint8_t vga_attr) {
    if (fb_active) {
        // There's no per-char color API exposed yet — gterm uses a
        // module-level cur_fg.  We reach it through a small accessor.
        gterm_cur_fg = gfx_color;
        (void)vga_attr;
    } else {
        terminal_setcolor(vga_attr);
    }
}

void shell_init(void) {
    set_fg(COLOR_PROMPT, make_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("chris os> ");
    set_fg(COLOR_TERM_FG, make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void shell_process_char(char c) {
    if (c == '\n') {
        terminal_putchar('\n');
        shell_execute(input, input_len);
        input_len = 0;
        input[0]  = 0;
        set_fg(COLOR_PROMPT, make_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("chris os> ");
        set_fg(COLOR_TERM_FG, make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    } else if (c == '\b') {
        if (input_len > 0) {
            input_len--;
            input[input_len] = 0;
            if (fb_active) {
                gterm_putchar('\b');
            } else {
                if (terminal_column > 0) {
                    terminal_column--;
                    terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
                    move_cursor(terminal_column, terminal_row);
                }
            }
        }
    } else {
        if (input_len < INPUT_SIZE - 1) {
            input[input_len++] = c;
            input[input_len]   = 0;
            terminal_putchar(c);
        }
    }
}

void shell_execute(char* cmd, int len) {
    if (len == 0) return;

    if (strcmp(cmd, "help") == 0) {
        terminal_writestring("commands: help, clear, echo, version, meminfo, uptime, reboot, disk, ui\n");
    } else if (strcmp(cmd, "clear") == 0) {
        terminal_initialize();
        return;
    } else if (strcmp(cmd, "version") == 0) {
        terminal_writestring("Chris OS v0.5 (VESA UI build)\n");
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        terminal_writestring(cmd + 5);
        terminal_putchar('\n');
    } else if (strcmp(cmd, "meminfo") == 0) {
        char buf[16];
        terminal_writestring("used: "); itoa(mm_used(), buf); terminal_writestring(buf);
        terminal_writestring(" bytes\nfree: "); itoa(mm_free(), buf); terminal_writestring(buf);
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
            for (int i = 0; i < size; i++) terminal_putchar(buf[i]);
            terminal_putchar('\n');
        } else {
            terminal_writestring("file not found\n");
        }
    } else if (strcmp(cmd, "disk") == 0) {
        uint8_t* buf = kmalloc(512);
        if (ata_read_sector(0, buf)) {
            terminal_writestring("boot sig: ");
            char hex[3]; hex[2] = 0;
            hex[0] = "0123456789ABCDEF"[buf[510] >> 4];
            hex[1] = "0123456789ABCDEF"[buf[510] & 0xF];
            terminal_writestring(hex); terminal_putchar(' ');
            hex[0] = "0123456789ABCDEF"[buf[511] >> 4];
            hex[1] = "0123456789ABCDEF"[buf[511] & 0xF];
            terminal_writestring(hex); terminal_putchar('\n');
        } else {
            terminal_writestring("disk read failed\n");
        }
    } else if (strcmp(cmd, "ui") == 0) {
        if (fb_active) {
            // Redraw the desktop to demo the UI layer
            draw_desktop();
            draw_taskbar();
            draw_window(100, 60, 400, 200, "About ChrisOS");
            draw_string(108, 96,  "ChrisOS v0.5 - VESA Graphical Mode", COLOR_WIN_TEXT, COLOR_WIN_BG);
            draw_string(108, 114, "1024 x 768 x 32bpp linear framebuffer", COLOR_WIN_TEXT, COLOR_WIN_BG);
            draw_string(108, 132, "Bitmap font renderer (8x16 PSF-style)", COLOR_WIN_TEXT, COLOR_WIN_BG);
            draw_string(108, 150, "draw_rect / draw_line / draw_window", COLOR_WIN_TEXT, COLOR_WIN_BG);
            // Restore terminal window on top
            gterm_init();
            shell_init();
        } else {
            terminal_writestring("ui: VESA not available, running in text mode\n");
        }
    } else {
        terminal_writestring("unknown command: ");
        terminal_writestring(cmd);
        terminal_putchar('\n');
    }
}

// ---- Arrow key / history stubs ----
// cursor_pos tracks where in the input line the cursor is
static int cursor_pos = 0;

void shell_cursor_left(void) {
    if (cursor_pos > 0) cursor_pos--;
}

void shell_cursor_right(void) {
    if (cursor_pos < input_len) cursor_pos++;
}

// Simple single-level history (last command)
static char history[INPUT_SIZE];
static int  history_len = 0;

void shell_history_up(void) {
    if (history_len == 0) return;
    // Replace current input with last history entry
    for (int i = 0; i < history_len; i++) input[i] = history[i];
    input_len  = history_len;
    input[input_len] = 0;
    cursor_pos = input_len;
}

void shell_history_down(void) {
    // Clear input
    input_len  = 0;
    input[0]   = 0;
    cursor_pos = 0;
}