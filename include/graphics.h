#ifndef GRAPHICS_H
#define GRAPHICS_H
#include "types.h"
#include <stdbool.h>
#include "fonts.h"

void rect(int x, int y, int width, int height, uint32 color);
void rect_grad(int x, int y, int width, int height, uint32 start_color, uint32 end_color);
void circle(int center_x, int center_y, int radius, uint32 color);
void text(const char* str, int x, int y, uint32 colour, font_t font_type, bool bold);
void text_ex(const char* str, int x, int y, uint32 colour, font_t font_type, bool bold, int scale);
void text_clip(const char* str, int x, int y, uint32 colour, font_t font_type);
void line(int x, int y, int width, int height, uint32 color);

void set_clip(int x, int y, int w, int h);
void reset_clip();

#endif
