#include "graphics.h"
#include "cmos.h"
#include "ports.h"
#include "string.h"
#include "vesa.h"
#include "isr.h"
#include "gui.h"

#define CLOCK_X 880
#define CLOCK_Y 3

#define DIGIT_W 15
#define DIGIT_H 20
#define FONT FONT_FUZZY
#define DIGIT_SPACING 4
#define GROUP_SPACING 7

// this will be expanded upon in v3.1, just trust me guys

uint32 timer_ticks = 0;

void format_time(time_t* t, char* buf) {
    char temp[3];
    
    itoa(temp, 'd', t->hour);
    if (t->hour < 10) {
        strcpy(buf, "0");
        strcat(buf, temp);
    } else {
        strcpy(buf, temp);
    }
    strcat(buf, ":");

    itoa(temp, 'd', t->minute);
    if (t->minute < 10) {
        strcat(buf, "0");
        strcat(buf, temp);
    } else {
        strcat(buf, temp);
    }
    strcat(buf, ":");

    itoa(temp, 'd', t->second);
    if (t->second < 10) {
        strcat(buf, "0");
        strcat(buf, temp);
    } else {
        strcat(buf, temp);
    }
}

void draw_digit(int digit, int x, int y, int font, bool bold) {
    char digit_str[2];
    digit_str[0] = digit + '0';
    digit_str[1] = '\0';

    rect_grad(x, y, 20, 30, RGB(172, 140, 188), RGB(147, 101, 158));
    text(digit_str, x + 6, y + 5, RGB(255, 255, 255), font, bold);
}

void draw_clock(time_t* t, int clock_x, int clock_y) {
    int x = clock_x;
    int y = clock_y;

    rect_grad(clock_x - 4, clock_y - 5, 148, 38, RGB(102, 72, 122), RGB(112, 84, 132));

    draw_digit(t->hour / 10, x, y, FONT, true);
    x += DIGIT_W + DIGIT_SPACING;

    draw_digit(t->hour % 10, x, y, FONT, true);
    x += DIGIT_W + GROUP_SPACING;

    text(":", x + 3, y + 5, RGB(225, 210, 245), FONT, true);
    x += 10;

    draw_digit(t->minute / 10, x, y, FONT, true);
    x += DIGIT_W + DIGIT_SPACING;

    draw_digit(t->minute % 10, x, y, FONT, true);
    x += DIGIT_W + GROUP_SPACING;

    text(":", x + 3, y + 5, RGB(225, 210, 245), FONT, true);
    x += 10;

    draw_digit(t->second / 10, x, y, FONT, true);
    x += DIGIT_W + DIGIT_SPACING;

    draw_digit(t->second % 10, x, y, FONT, true);
}

void timer_handler(REGISTERS* r) {
    timer_ticks++; is_dirty(TRUE);
}

void timer_init(uint32 frequency) {
    uint32 divisor = 1193180 / frequency;
    outportb(0x43, 0x36);
    outportb(0x40, divisor & 0xFF);
    outportb(0x40, (divisor >> 8) & 0xFF);
}