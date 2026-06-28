#ifndef KRONII_H
#define KRONII_H

#include "isr.h"
#include "cmos.h"

extern uint32_t timer_ticks;

void format_time(time_t* t, char* buf);
void draw_digit(int digit, int x, int y, int font, BOOL bold);
void draw_clock(time_t* t, int clock_x, int clock_y);
void timer_handler(REGISTERS* r);
void timer_init(uint32 frequency);

#endif