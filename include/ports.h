#ifndef PORTS_H
#define PORTS_H

#include "types.h"

uint8 inportb(uint16 port);
void outportb(uint16 port, uint8 val);

uint16 inports(uint16 port);
void outports(uint16 port, uint16 data);

uint32 inportl(uint16 port);
void outportl(uint16 port, uint32 data);

void io_wait(void);
#endif