#ifndef TIMER_H
#define TIMER_H

#include "types.h"

void timer_handler(REGISTERS *r);
void delay_spin(int n);

#endif