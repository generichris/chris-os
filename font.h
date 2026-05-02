#ifndef FONT_H_H
#define FONT_H_H

#include <stdint.h>

// 8x16 bitmap font, ASCII 32-126
// Each character = 16 bytes, one byte per row, MSB = leftmost pixel
#define FONT_W 8
#define FONT_H 16
#define FONT_FIRST 32
#define FONT_LAST  126

extern const uint8_t font_data[][FONT_H];

#endif // FONT_H_H