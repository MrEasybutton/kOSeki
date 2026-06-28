#ifndef CONSOLE_H
#define CONSOLE_H

#include "types.h"

#define MAXIMUM_PAGES 16

#define SCROLL_UP 1
#define SCROLL_DOWN 2

#define VGA_ADDRESS 0xB8000
#define VGA_TOTAL_ITEMS 2200

#define VGA_WIDTH 80
#define VGA_HEIGHT 24

typedef enum {
    COLOR_BLACK,
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_CYAN,
    COLOR_RED,
    COLOR_MAGENTA,
    COLOR_BROWN,
    COLOR_GREY,
    COLOR_DARK_GREY,
    COLOR_BRIGHT_BLUE,
    COLOR_BRIGHT_GREEN,
    COLOR_BRIGHT_CYAN,
    COLOR_BRIGHT_RED,
    COLOR_BRIGHT_MAGENTA,
    COLOR_YELLOW,
    COLOR_WHITE,
} VGA_COLOR_TYPE;

uint16 vga_item_entry(uint8 ch, VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color);

void set_cur_pos(uint8 x, uint8 y);

void console_clear(VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color);

//initialize console
void console_init(VGA_COLOR_TYPE fore_color, VGA_COLOR_TYPE back_color);
void console_scroll(int line_count);
void console_putchar(char ch);
// revert back the printed character and add 0 to it
void console_ungetchar();
// revert back the printed character until n characters
void console_ungetchar_bound(uint8 n);

void console_gotoxy(uint16 x, uint16 y);

void console_putstr(const char *str);
void printf(const char *format, ...);

// read string from console, but no backing
void getstr(char *buffer);

// read string from console, and erase or go back util bound occurs
void getstr_bound(char *buffer, uint8 bound);

#endif
