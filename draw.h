#ifndef DRAW_H
#define DRAW_H

#include <stdint.h>
#include "vesa.h"

// ---- Primitives ----
void draw_rect(int x, int y, int w, int h, uint32_t color);
void draw_rect_outline(int x, int y, int w, int h, uint32_t color);
void draw_hline(int x, int y, int len, uint32_t color);
void draw_vline(int x, int y, int len, uint32_t color);
void draw_line(int x0, int y0, int x1, int y1, uint32_t color);

// ---- Font rendering ----
// Draw one character; returns next x position
int  draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);
// Draw a null-terminated string; returns next x position
int  draw_string(int x, int y, const char* s, uint32_t fg, uint32_t bg);
// Draw string with transparent background (bg ignored)
int  draw_string_alpha(int x, int y, const char* s, uint32_t fg);

// ---- Higher-level UI pieces ----
// Titled window chrome (title bar + background, no border)
void draw_window(int x, int y, int w, int h, const char* title);
// Taskbar across the bottom
void draw_taskbar(void);
// Desktop background
void draw_desktop(void);

// Cursor blit (simple arrow, 12x20)
void draw_cursor(int x, int y);

#endif
