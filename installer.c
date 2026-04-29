#include "installer.h"
#include "ata.h"
#include "kernel.h"
#include "mm.h"









static int active = 0;
static int state  = 0;


static uint8_t found[2];
static int     found_count = 0;


static uint8_t target_drive = 0xFF;


#define SOURCE_DRIVE 0


static void print_num(uint32_t n) {
    if (n == 0) { terminal_putchar('0'); return; }
    char tmp[12];
    int i = 0;
    while (n > 0) { tmp[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) terminal_putchar(tmp[--i]);
}


static void clear_line() {
    
    terminal_writestring("\r                                    \r");
}


static void run_install() {
    terminal_writestring("\nInstalling... please wait.\n");

    uint8_t* buf = kmalloc(512);
    if (!buf) {
        terminal_writestring("installer: out of memory\n");
        return;
    }

    uint32_t total = INSTALL_SECTOR_COUNT;
    uint32_t milestone = total / 20;   
    if (milestone == 0) milestone = 1;

    for (uint32_t lba = 0; lba < total; lba++) {

        if (!ata_read_sector_ex(SOURCE_DRIVE, lba, buf)) {
            terminal_writestring("\nRead error at LBA ");
            print_num(lba);
            terminal_writestring(" — install aborted.\n");
            return;
        }

        if (!ata_write_sector_ex(target_drive, lba, buf)) {
            terminal_writestring("\nWrite error at LBA ");
            print_num(lba);
            terminal_writestring(" — install aborted.\n");
            return;
        }

        
        if ((lba % milestone) == 0) {
            uint32_t pct = (lba * 100) / total;
            terminal_writestring("  [");
            print_num(pct);
            terminal_writestring("%] sector ");
            print_num(lba);
            terminal_writestring(" / ");
            print_num(total);
            terminal_putchar('\n');
        }
    }

    terminal_writestring("\nInstall complete!\n");
    terminal_writestring("Remove the boot USB/drive, then reboot.\n");
    terminal_writestring("Type 'reboot' to restart now.\n");
}



int installer_is_active() {
    return active;
}

void installer_start() {
    active      = 1;
    state       = 0;
    found_count = 0;
    target_drive = 0xFF;

    terminal_putchar('\n');
    terminal_setcolor(make_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("=== Chris OS Installer ===\n\n");
    terminal_setcolor(make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));

    terminal_writestring("Scanning drives...\n");

    for (uint8_t d = 0; d < 2; d++) {
        if (ata_detect(d)) {
            found[found_count++] = d;
            terminal_writestring("  Drive ");
            terminal_putchar('0' + d);
            if (d == SOURCE_DRIVE)
                terminal_writestring(" - primary master (boot drive)\n");
            else
                terminal_writestring(" - primary slave\n");
        }
    }

    if (found_count < 2) {
        terminal_setcolor(make_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("\nOnly one drive detected.\n");
        terminal_writestring("Connect a second drive to install onto, then try again.\n");
        terminal_setcolor(make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        active = 0;
        return;
    }

    
    terminal_putchar('\n');
    terminal_writestring("Available target drives (do NOT pick the boot drive):\n");
    for (int i = 0; i < found_count; i++) {
        if (found[i] == SOURCE_DRIVE) continue;
        terminal_writestring("  ");
        terminal_putchar('0' + found[i]);
        terminal_writestring(" - primary slave\n");
    }

    terminal_putchar('\n');
    terminal_writestring("Type the target drive number: ");
    state = 0;
}

void installer_handle_char(char c) {
    if (!active) return;

    
    if (state == 0) {
        terminal_putchar(c);   

        if (c < '0' || c > '1') {
            terminal_writestring("\nInvalid drive number. Try again: ");
            return;
        }

        uint8_t chosen = (uint8_t)(c - '0');

        
        if (chosen == SOURCE_DRIVE) {
            terminal_writestring("\nThat is the boot drive — choose a different one: ");
            return;
        }

        int detected = 0;
        for (int i = 0; i < found_count; i++) {
            if (found[i] == chosen) { detected = 1; break; }
        }
        if (!detected) {
            terminal_writestring("\nDrive not detected. Try again: ");
            return;
        }

        target_drive = chosen;

        terminal_putchar('\n');
        terminal_setcolor(make_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("\nWARNING: This will ERASE drive ");
        terminal_putchar('0' + target_drive);
        terminal_writestring(" and write ~26MB of data.\n");
        terminal_setcolor(make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Type 'y' to confirm, 'n' to cancel: ");
        state = 1;
        return;
    }

    
    if (state == 1) {
        terminal_putchar(c);
        terminal_putchar('\n');

        if (c == 'y' || c == 'Y') {
            state = 2;
            run_install();
            state = 3;
            active = 0;
        } else {
            terminal_writestring("Install cancelled.\n");
            active = 0;
        }
        return;
    }
}