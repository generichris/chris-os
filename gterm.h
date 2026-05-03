#ifndef GTERM_H
#define GTERM_H

#include <stdint.h>





#define GTERM_COLS 80
#define GTERM_ROWS 24


#define GTERM_WIN_X  60
#define GTERM_WIN_Y  40
#define GTERM_WIN_W  700
#define GTERM_WIN_H  530

void gterm_init(void);
void gterm_putchar(char c);
void gterm_writestring(const char* s);
void gterm_clear(void);


void gterm_tick(void);

#endif


extern uint32_t gterm_cur_fg;
