#ifndef GTERM_H
#define GTERM_H

#include <stdint.h>

// Graphical terminal — draws text into a framebuffer window
// using the bitmap font renderer.  Replaces VGA text output
// when VESA is active.

#define GTERM_COLS 80
#define GTERM_ROWS 24

// Position/size of the terminal window on-screen
#define GTERM_WIN_X  60
#define GTERM_WIN_Y  40
#define GTERM_WIN_W  700
#define GTERM_WIN_H  530

void gterm_init(void);
void gterm_putchar(char c);
void gterm_writestring(const char* s);
void gterm_clear(void);

// Cursor blink — call from main loop
void gterm_tick(void);

#endif

// Current foreground color used by gterm_putchar()
extern uint32_t gterm_cur_fg;
