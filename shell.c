#include "shell.h"
#include "kernel.h"
#include "mm.h"
#include "ata.h"
#include "fat32.h"
#include "installer.h"



#define INPUT_SIZE 256

static char input[INPUT_SIZE];
static int  input_len = 0;
static int  input_cur = 0;   



static size_t prompt_col = 0;
static size_t prompt_row = 0;



#define HISTORY_MAX 16

static char history[HISTORY_MAX][INPUT_SIZE];
static int  history_count  = 0;
static int  history_browse = -1;

static void history_add(const char* cmd) {
    if (cmd[0] == '\0') return;
    if (history_count > 0) {
        int last = (history_count - 1) % HISTORY_MAX;
        int same = 1;
        for (int i = 0; cmd[i] || history[last][i]; i++)
            if (cmd[i] != history[last][i]) { same = 0; break; }
        if (same) { history_browse = -1; return; }
    }
    kstrcpy(history[history_count % HISTORY_MAX], cmd);
    history_count++;
    history_browse = -1;
}




static void redraw_input(int old_len) {
    
    terminal_row    = prompt_row;
    terminal_column = prompt_col;

    
    for (int i = 0; i < input_len; i++)
        terminal_putchar(input[i]);

    
    int extra = old_len - input_len;
    for (int i = 0; i < extra; i++)
        terminal_putchar(' ');

    
    size_t cur_col = (prompt_col + (size_t)input_cur) % VGA_WIDTH;
    size_t cur_row = prompt_row + (prompt_col + (size_t)input_cur) / VGA_WIDTH;
    terminal_row    = prompt_row + (prompt_col + (size_t)input_len) / VGA_WIDTH;
    terminal_column = (prompt_col + (size_t)input_len) % VGA_WIDTH;
    move_cursor(cur_col, cur_row);
}


static void input_replace(const char* new_cmd) {
    int old_len = input_len;
    input_len = 0;
    for (int i = 0; new_cmd[i] && input_len < INPUT_SIZE - 1; i++)
        input[input_len++] = new_cmd[i];
    input[input_len] = '\0';
    input_cur = input_len;   
    redraw_input(old_len);
}



void shell_history_up(void) {
    if (history_count == 0) return;
    if (history_browse < 0) history_browse = history_count;
    int oldest = (history_count > HISTORY_MAX) ? history_count - HISTORY_MAX : 0;
    if (history_browse <= oldest) return;
    history_browse--;
    input_replace(history[history_browse % HISTORY_MAX]);
}

void shell_history_down(void) {
    if (history_browse < 0) return;
    history_browse++;
    if (history_browse >= history_count) {
        history_browse = -1;
        input_replace("");
    } else {
        input_replace(history[history_browse % HISTORY_MAX]);
    }
}



void shell_cursor_left(void) {
    if (input_cur <= 0) return;
    input_cur--;
    size_t cur_col = (prompt_col + (size_t)input_cur) % VGA_WIDTH;
    size_t cur_row = prompt_row + (prompt_col + (size_t)input_cur) / VGA_WIDTH;
    move_cursor(cur_col, cur_row);
}

void shell_cursor_right(void) {
    if (input_cur >= input_len) return;
    input_cur++;
    size_t cur_col = (prompt_col + (size_t)input_cur) % VGA_WIDTH;
    size_t cur_row = prompt_row + (prompt_col + (size_t)input_cur) / VGA_WIDTH;
    move_cursor(cur_col, cur_row);
}



#define PIPE_BUF_SIZE 1024
static char pipe_buf[PIPE_BUF_SIZE];



static void print_prompt(void) {
    terminal_setcolor(make_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("root");
    terminal_setcolor(make_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("@chrisos");
    terminal_setcolor(make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring(":~# ");
    terminal_setcolor(make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    prompt_col = terminal_column;
    prompt_row = terminal_row;
}

void shell_init(void) {
    print_prompt();
}



static void shell_run(char* cmd, int len, const char* piped_in, int piped_len);



void shell_process_char(char c) {
    if (installer_is_active()) {
        installer_handle_char(c);
        return;
    }

    if (c == '\n') {
        
        terminal_row    = prompt_row + (prompt_col + (size_t)input_len) / VGA_WIDTH;
        terminal_column = (prompt_col + (size_t)input_len) % VGA_WIDTH;
        terminal_putchar('\n');
        input[input_len] = '\0';
        history_add(input);
        shell_execute(input, input_len);
        input_len = 0;
        input_cur = 0;
        input[0]  = '\0';
        print_prompt();

    } else if (c == '\b') {
        
        if (input_cur <= 0) return;
        int old_len = input_len;
        
        for (int i = input_cur - 1; i < input_len - 1; i++)
            input[i] = input[i + 1];
        input_len--;
        input_cur--;
        input[input_len] = '\0';
        redraw_input(old_len);

    } else if (c == '\t') {
        
        int spaces = 4;
        if (input_len + spaces >= INPUT_SIZE) return;
        int old_len = input_len;
        for (int i = input_len + spaces - 1; i >= input_cur + spaces; i--)
            input[i] = input[i - spaces];
        for (int i = 0; i < spaces; i++)
            input[input_cur + i] = ' ';
        input_len += spaces;
        input_cur += spaces;
        input[input_len] = '\0';
        redraw_input(old_len);

    } else {
        
        if (input_len >= INPUT_SIZE - 1) return;
        int old_len = input_len;
        
        for (int i = input_len; i > input_cur; i--)
            input[i] = input[i - 1];
        input[input_cur] = c;
        input_len++;
        input_cur++;
        input[input_len] = '\0';
        redraw_input(old_len);
    }
}



static char* ltrim(char* s) {
    while (*s == ' ') s++;
    return s;
}

static int rtrim_len(char* s, int len) {
    while (len > 0 && s[len - 1] == ' ') len--;
    return len;
}



void shell_execute(char* cmd, int len) {
    int pipe_at = -1;
    for (int i = 0; i < len; i++)
        if (cmd[i] == '|') { pipe_at = i; break; }

    if (pipe_at >= 0) {
        char  left[INPUT_SIZE], right[INPUT_SIZE];
        int   llen = rtrim_len(cmd, pipe_at);
        char* rsrc = ltrim(cmd + pipe_at + 1);
        int   rlen = rtrim_len(rsrc, (int)strlen(rsrc));

        for (int i = 0; i < llen; i++) left[i]  = cmd[i];  left[llen]  = '\0';
        for (int i = 0; i < rlen; i++) right[i] = rsrc[i]; right[rlen] = '\0';

        terminal_capture_start(pipe_buf, PIPE_BUF_SIZE);
        shell_run(left, llen, 0, 0);
        int captured = terminal_capture_stop();

        while (captured > 0 &&
               (pipe_buf[captured-1] == '\n' || pipe_buf[captured-1] == '\r'))
            captured--;
        pipe_buf[captured] = '\0';

        shell_run(right, rlen, pipe_buf, captured);
        return;
    }

    shell_run(cmd, len, 0, 0);
}



static void shell_run(char* cmd, int len, const char* piped_in, int piped_len) {
    if (len == 0) return;
    char* c = ltrim(cmd);
    len = rtrim_len(c, (int)strlen(c));
    c[len] = '\0';

    if (strcmp(c, "help") == 0) {
        terminal_setcolor(make_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("Chris OS commands\n");
        terminal_setcolor(make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring(
            "  help                    this message\n"
            "  clear                   clear screen\n"
            "  echo <text>             print text\n"
            "  version / uname         OS version\n"
            "  whoami                  current user\n"
            "  meminfo                 heap usage\n"
            "  uptime                  seconds since boot\n"
            "  reboot                  reboot\n"
            "  disk                    boot sector signature\n"
            "  ls                      list files\n"
            "  cat  <file>             print file\n"
            "  touch <file>            create empty file\n"
            "  write <file> <text>     write text to file\n"
            "  rm    <file>            delete file\n"
            "  wc    <file>            line + byte count\n"
            "  color <0-15>            change text color\n"
            "  install                 disk installer\n"
            "\n"
            "  Pipe:        echo hello | touch msg.txt\n"
            "  Arrow up/dn: browse history\n"
            "  Arrow lt/rt: move cursor to edit\n"
        );
    } else if (strcmp(c, "clear") == 0) {
        terminal_initialize();
    } else if (strncmp(c, "echo ", 5) == 0) {
        terminal_writestring(c + 5);
        terminal_putchar('\n');
    } else if (strcmp(c, "version") == 0 || strcmp(c, "uname") == 0) {
        terminal_writestring("Chris OS v0.6\n");
    } else if (strcmp(c, "whoami") == 0) {
        terminal_writestring("root\n");
    } else if (strcmp(c, "meminfo") == 0) {
        char buf[16];
        terminal_writestring("used: "); itoa(mm_used(), buf); terminal_writestring(buf);
        terminal_writestring(" B\nfree: "); itoa(mm_free(), buf); terminal_writestring(buf);
        terminal_writestring(" B\n");
    } else if (strcmp(c, "uptime") == 0) {
        char buf[16];
        terminal_writestring("uptime: ");
        itoa(get_ticks() / 18, buf); terminal_writestring(buf);
        terminal_writestring(" s\n");
    } else if (strcmp(c, "reboot") == 0) {
        terminal_writestring("rebooting...\n");
        kreboot();
    } else if (strcmp(c, "disk") == 0) {
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
    } else if (strcmp(c, "ls") == 0) {
        fat32_ls();
    } else if (strncmp(c, "cat ", 4) == 0) {
        char* fname = ltrim(c + 4);
        if (*fname == '\0') {
            if (piped_in && piped_len > 0) {
                terminal_write(piped_in, (size_t)piped_len);
                terminal_putchar('\n');
            } else {
                terminal_writestring("usage: cat <file>\n");
            }
        } else {
            uint8_t* fbuf = kmalloc(4096);
            int size = fat32_read_file(fname, fbuf);
            if (size > 0) {
                for (int i = 0; i < size; i++) terminal_putchar(fbuf[i]);
                terminal_putchar('\n');
            } else {
                terminal_writestring("cat: not found: ");
                terminal_writestring(fname); terminal_putchar('\n');
            }
        }
    } else if (strncmp(c, "touch ", 6) == 0) {
        char* fname = ltrim(c + 6);
        if (piped_in && piped_len > 0) {
            if (fat32_write_file(fname, (const uint8_t*)piped_in, (uint32_t)piped_len))
                terminal_writestring("written\n");
        } else {
            if (fat32_create_file(fname))
                terminal_writestring("created\n");
        }
    } else if (strncmp(c, "write ", 6) == 0) {
        char* rest  = ltrim(c + 6);
        char* space = kstrchr(rest, ' ');
        if (!space) {
            terminal_writestring("usage: write <file> <text>\n");
        } else {
            *space = '\0';
            char* content = ltrim(space + 1);
            if (fat32_write_file(rest, (const uint8_t*)content, (uint32_t)strlen(content)))
                terminal_writestring("written\n");
        }
    } else if (strncmp(c, "rm ", 3) == 0) {
        char* fname = ltrim(c + 3);
        if (fat32_delete_file(fname))
            terminal_writestring("deleted\n");
        else {
            terminal_writestring("rm: not found: ");
            terminal_writestring(fname); terminal_putchar('\n');
        }
    } else if (strncmp(c, "wc ", 3) == 0) {
        char* fname = ltrim(c + 3);
        uint8_t* fbuf = kmalloc(4096);
        int size = fat32_read_file(fname, fbuf);
        if (size <= 0) {
            terminal_writestring("wc: not found: ");
            terminal_writestring(fname); terminal_putchar('\n');
        } else {
            int lines = 0;
            for (int i = 0; i < size; i++) if (fbuf[i] == '\n') lines++;
            char buf[16];
            terminal_writestring("lines: "); itoa((uint32_t)lines, buf); terminal_writestring(buf);
            terminal_writestring("  bytes: "); itoa((uint32_t)size, buf); terminal_writestring(buf);
            terminal_putchar('\n');
        }
    } else if (strcmp(c, "install") == 0) {
        installer_start();
    } else {
        terminal_setcolor(make_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("unknown: ");
        terminal_setcolor(make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        terminal_writestring(c); terminal_putchar('\n');
        terminal_setcolor(make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("type 'help' for commands\n");
    }
}