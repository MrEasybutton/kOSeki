#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

#define SERIAL_COM1_DATA_REGISTER_PORT      0x3F8  // Data register (R/W)
#define SERIAL_COM1_INTERRUPT_ENABLE_PORT   0x3F9  // Interrupt Enable Register (R/W)
#define SERIAL_COM1_INTERRUPT_DIVISOR_LOW   0x3F8  // Divisor Latch Low Byte (R/W)
#define SERIAL_COM1_INTERRUPT_DIVISOR_HIGH  0x3F9  // Divisor Latch High Byte (R/W)
#define SERIAL_COM1_INT_ID_FIFO_CTRL_PORT   0x3FA  // Interrupt Identification and FIFO Control Register (R/W)
#define SERIAL_COM1_LINE_CTRL_REG_PORT      0x3FB  // Line Control Register (R/W)
#define SERIAL_COM1_MODEM_CTRL_REG_PORT     0x3FC  // Modem Control Register (R/W)
#define SERIAL_COM1_LINE_STATUS_REG_PORT    0x3FD  // Line Status Register (R/W)
#define SERIAL_COM1_MODEM_STATUS_REG_PORT   0x3FE  // Modem Status Register (R/W)

void serial_init();
char serial_getc();
void serial_putc(char c);
void serial_puts(const char *s);
void kprint(const char *format, ...);

#endif