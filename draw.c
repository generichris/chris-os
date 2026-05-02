#include "draw.h"
#include "font.h"
#include "vesa.h"
#include "kernel.h"  // for get_ticks()

// -----------------------------------------------------------------------
// Primitives
// -----------------------------------------------------------------------

void draw_rect(int x, int y, int w, int h, uint32_t color) {
    for (int row = y; row < y + h; row++)
        draw_hline(x, row, w, color);
}

void draw_hline(int x, int y, int len, uint32_t color) {
    for (int i = 0; i < len; i++)
        fb_put_pixel(x + i, y, color);
}

void draw_vline(int x, int y, int len, uint32_t color) {
    for (int i = 0; i < len; i++)
        fb_put_pixel(x, y + i, color);
}

void draw_rect_outline(int x, int y, int w, int h, uint32_t color) {
    draw_hline(x,         y,         w, color);
    draw_hline(x,         y + h - 1, w, color);
    draw_vline(x,         y,         h, color);
    draw_vline(x + w - 1, y,         h, color);
}

// Bresenham line
void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx =  (x1 > x0 ? x1 - x0 : x0 - x1);
    int dy = -(y1 > y0 ? y1 - y0 : y0 - y1);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (1) {
        fb_put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

// -----------------------------------------------------------------------
// Font rendering
// -----------------------------------------------------------------------

int draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    if (c < FONT_FIRST || c > FONT_LAST) c = '?';
    const uint8_t* glyph = font_data[c - FONT_FIRST];
    for (int row = 0; row < FONT_H; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_W; col++) {
            if (bits & (0x80 >> col))
                fb_put_pixel(x + col, y + row, fg);
            else
                fb_put_pixel(x + col, y + row, bg);
        }
    }
    return x + FONT_W;
}

int draw_string(int x, int y, const char* s, uint32_t fg, uint32_t bg) {
    while (*s) {
        x = draw_char(x, y, *s++, fg, bg);
    }
    return x;
}

int draw_string_alpha(int x, int y, const char* s, uint32_t fg) {
    while (*s) {
        char c = *s++;
        if (c < FONT_FIRST || c > FONT_LAST) c = '?';
        const uint8_t* glyph = font_data[c - FONT_FIRST];
        for (int row = 0; row < FONT_H; row++) {
            uint8_t bits = glyph[row];
            for (int col = 0; col < FONT_W; col++) {
                if (bits & (0x80 >> col))
                    fb_put_pixel(x + col, y + row, fg);
            }
        }
        x += FONT_W;
    }
    return x;
}

// -----------------------------------------------------------------------
// UI Widgets
// -----------------------------------------------------------------------

#define TITLEBAR_H 24
#define SHADOW_OFF  3

void draw_window(int x, int y, int w, int h, const char* title) {
    // Drop shadow
    draw_rect(x + SHADOW_OFF, y + SHADOW_OFF, w, h, rgb(0, 0, 0));

    // Window background
    draw_rect(x, y, w, h, COLOR_WIN_BG);

    // Title bar gradient (two-tone approximation: top slightly lighter)
    draw_rect(x, y, w, TITLEBAR_H / 2, rgb(80, 120, 200));
    draw_rect(x, y + TITLEBAR_H / 2, w, TITLEBAR_H / 2, COLOR_WIN_TITLE);

    // Title text centered-ish
    draw_string_alpha(x + 6, y + 4, title, COLOR_WHITE);

    // Close button
    int cx = x + w - 18;
    int cy = y + 4;
    draw_rect(cx, cy, 14, 14, rgb(200, 60, 60));
    draw_string_alpha(cx + 3, cy, "x", COLOR_WHITE);

    // Thin border
    draw_rect_outline(x, y, w, h, rgb(40, 60, 100));
}

#define TASKBAR_H 36

void draw_taskbar(void) {
    int sw = (int)fb_width;
    int sh = (int)fb_height;

    // Bar background
    draw_rect(0, sh - TASKBAR_H, sw, TASKBAR_H, COLOR_TASKBAR);
    // Top edge highlight
    draw_hline(0, sh - TASKBAR_H, sw, rgb(70, 90, 130));

    // Start-style button on the left
    draw_rect(4, sh - TASKBAR_H + 4, 72, TASKBAR_H - 8, COLOR_WIN_TITLE);
    draw_rect_outline(4, sh - TASKBAR_H + 4, 72, TASKBAR_H - 8, rgb(100, 140, 220));
    draw_string_alpha(10, sh - TASKBAR_H + 10, "ChrisOS", COLOR_WHITE);

    // Clock on the right — show tick count as uptime seconds
    char timebuf[16];
    uint32_t secs = get_ticks() / 18;
    // format mm:ss
    int mins = (int)(secs / 60);
    int sec  = (int)(secs % 60);
    // manual format into timebuf
    timebuf[0] = '0' + (mins / 10);
    timebuf[1] = '0' + (mins % 10);
    timebuf[2] = ':';
    timebuf[3] = '0' + (sec  / 10);
    timebuf[4] = '0' + (sec  % 10);
    timebuf[5] = 0;
    draw_string(sw - 60, sh - TASKBAR_H + 10, timebuf, COLOR_WHITE, COLOR_TASKBAR);
}

// Desktop: solid navy + simple grid-dot pattern
void draw_desktop(void) {
    draw_rect(0, 0, (int)fb_width, (int)fb_height - TASKBAR_H, COLOR_DESKTOP);

    // Subtle dot grid every 32px
    for (int y = 16; y < (int)fb_height - TASKBAR_H; y += 32)
        for (int x = 16; x < (int)fb_width; x += 32)
            fb_put_pixel(x, y, rgb(35, 50, 70));
}

// -----------------------------------------------------------------------
// Mouse cursor — simple 12x20 arrow bitmap
// -----------------------------------------------------------------------
static const uint16_t cursor_bmp[20] = {
    0x8000, 0xC000, 0xE000, 0xF000,
    0xF800, 0xFC00, 0xFE00, 0xFF00,
    0xFF80, 0xFFC0, 0xFC00, 0xEC00,
    0xC600, 0x0600, 0x0300, 0x0300,
    0x0180, 0x0180, 0x00C0, 0x0000,
};

void draw_cursor(int cx, int cy) {
    for (int row = 0; row < 20; row++) {
        uint16_t bits = cursor_bmp[row];
        for (int col = 0; col < 12; col++) {
            if (bits & (0x8000 >> col))
                fb_put_pixel(cx + col, cy + row, COLOR_WHITE);
        }
    }
}
