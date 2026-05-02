#include "keyboard.h"
#include "irq.h"
#include "kernel.h"
#include "shell.h"

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    asm volatile("inb %%dx, %%al" : "=a"(v) : "d"(port));
    return v;
}


static const char sc_normal[] = {
  0,    0,    '1',  '2',  '3',  '4',  '5',  '6',
  '7',  '8',  '9',  '0',  '-',  '=',  0,    0,
  'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
  'o',  'p',  '[',  ']',  0,    0,    'a',  's',
  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
  '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
  'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',
  0,    ' ',
};


static const char sc_shift[] = {
  0,    0,    '!',  '@',  '#',  '$',  '%',  '^',
  '&',  '*',  '(',  ')',  '_',  '+',  0,    0,
  'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
  'O',  'P',  '{',  '}',  0,    0,    'A',  'S',
  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
  '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
  'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',
  0,    ' ',
};

#define SC_TABLE_LEN ((int)(sizeof(sc_normal)))


static int shift_held = 0;   
static int caps_lock  = 0;   
static int ext_pending = 0;  


static void keyboard_handler(struct registers regs) {
    (void)regs;
    
    // We rely purely on IRQ 1 routing. Read exactly one byte.
    uint8_t sc = inb(0x60);
    
    if (sc == 0xE0) { ext_pending = 1; return; }

    int released = (sc & 0x80);
    uint8_t code = sc & 0x7F;

    
    if (ext_pending) {
        ext_pending = 0;
        if (!released) {
            if (code == 0x48) { shell_history_up();    return; } 
            if (code == 0x50) { shell_history_down();  return; } 
            if (code == 0x4B) { shell_cursor_left();   return; } 
            if (code == 0x4D) { shell_cursor_right();  return; } 
        }
        return;
    }

    
    
    if (code == 0x2A || code == 0x36) {
        shift_held = !released;
        return;
    }
    
    if (code == 0x3A && !released) {
        caps_lock = !caps_lock;
        return;
    }

    if (released) return;   

    
    if (code == 0x0E) { shell_process_char('\b'); return; }

    
    if (code == 0x1C) { shell_process_char('\n'); return; }

    
    if (code == 0x0F) { shell_process_char('\t'); return; }

    if (code >= SC_TABLE_LEN) return;

    
    int use_shift = shift_held;

    char c = use_shift ? sc_shift[code] : sc_normal[code];
    if (c == 0) return;

    
    if (caps_lock) {
        if (c >= 'a' && c <= 'z') c -= 32;   
        else if (c >= 'A' && c <= 'Z') c += 32; 
    }

    shell_process_char(c);
}

void keyboard_init(void) {
    irq_install_handler(1, keyboard_handler);
}