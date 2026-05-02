#include "gterm.h"
#include "draw.h"
#include "vesa.h"
#include "font.h"
#include "kernel.h"

// Inner text area margins inside the window chrome
#define INNER_X  (GTERM_WIN_X + 6)
#define INNER_Y  (GTERM_WIN_Y + 30)   // below titlebar
#define CELL_W   FONT_W
#define CELL_H   (FONT_H + 2)         // slight line spacing

static int cur_col = 0;
static int cur_row = 0;
uint32_t gterm_cur_fg = 0;   // initialised in gterm_init(); shell.c can write this

// Backing cell buffer so we can scroll without full redraw
static char   cell_char [GTERM_ROWS][GTERM_COLS];
static uint32_t cell_fg [GTERM_ROWS][GTERM_COLS];

static void redraw_cell(int col, int row) {
    int px = INNER_X + col * CELL_W;
    int py = INNER_Y + row * CELL_H;
    char c = cell_char[row][col];
    if (c == 0) c = ' ';
    draw_char(px, py, c, cell_fg[row][col], COLOR_TERM_BG);
}

static void redraw_all(void) {
    // Redraw window chrome
    draw_window(GTERM_WIN_X, GTERM_WIN_Y, GTERM_WIN_W, GTERM_WIN_H, "Terminal");
    // Redraw text area background
    draw_rect(INNER_X, INNER_Y,
              GTERM_COLS * CELL_W,
              GTERM_ROWS * CELL_H,
              COLOR_TERM_BG);
    for (int r = 0; r < GTERM_ROWS; r++)
        for (int c = 0; c < GTERM_COLS; c++)
            redraw_cell(c, r);
}

static void scroll_up(void) {
    for (int r = 0; r < GTERM_ROWS - 1; r++) {
        for (int c = 0; c < GTERM_COLS; c++) {
            cell_char[r][c] = cell_char[r+1][c];
            cell_fg  [r][c] = cell_fg  [r+1][c];
        }
    }
    for (int c = 0; c < GTERM_COLS; c++) {
        cell_char[GTERM_ROWS-1][c] = ' ';
        cell_fg  [GTERM_ROWS-1][c] = gterm_cur_fg;
    }
    cur_row = GTERM_ROWS - 1;
    redraw_all();
}

void gterm_clear(void) {
    for (int r = 0; r < GTERM_ROWS; r++)
        for (int c = 0; c < GTERM_COLS; c++) {
            cell_char[r][c] = ' ';
            cell_fg  [r][c] = gterm_cur_fg;
        }
    cur_col = 0;
    cur_row = 0;
    redraw_all();
}

void gterm_init(void) {
    gterm_cur_fg = COLOR_TERM_FG;   // set at runtime, not compile-time
    gterm_clear();
}

void gterm_putchar(char ch) {
    if (ch == '\n') {
        cur_col = 0;
        cur_row++;
        if (cur_row >= GTERM_ROWS) scroll_up();
        return;
    }
    if (ch == '\b') {
        if (cur_col > 0) {
            cur_col--;
            cell_char[cur_row][cur_col] = ' ';
            redraw_cell(cur_col, cur_row);
        }
        return;
    }
    if (cur_col >= GTERM_COLS) {
        cur_col = 0;
        cur_row++;
        if (cur_row >= GTERM_ROWS) scroll_up();
    }
    cell_char[cur_row][cur_col] = ch;
    cell_fg  [cur_row][cur_col] = gterm_cur_fg;
    redraw_cell(cur_col, cur_row);
    cur_col++;
}

void gterm_writestring(const char* s) {
    while (*s) gterm_putchar(*s++);
}

// Cursor blink — call periodically from the main loop
static uint32_t last_blink = 0;
static int cursor_visible = 1;

void gterm_tick(void) {
    uint32_t now = get_ticks();
    if (now - last_blink > 9) {   // ~9 ticks = ~0.5s at 18Hz
        last_blink = now;
        cursor_visible ^= 1;
        int px = INNER_X + cur_col * CELL_W;
        int py = INNER_Y + cur_row * CELL_H;
        uint32_t color = cursor_visible ? COLOR_CURSOR : COLOR_TERM_BG;
        draw_rect(px, py, 2, FONT_H, color);
    }
}