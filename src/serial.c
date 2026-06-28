#include "ports.h"
#include "serial.h"
#include "string.h"
#include <stdarg.h>

void serial_init(void) {
    outportb(SERIAL_COM1_INTERRUPT_ENABLE_PORT, 0x00);
    outportb(SERIAL_COM1_LINE_CTRL_REG_PORT, 0x80);
    outportb(SERIAL_COM1_INTERRUPT_DIVISOR_LOW, 0x01);
    outportb(SERIAL_COM1_INTERRUPT_DIVISOR_HIGH, 0x00);
    outportb(SERIAL_COM1_LINE_CTRL_REG_PORT, 0x03);
    outportb(SERIAL_COM1_INT_ID_FIFO_CTRL_PORT, 0xC7);
    outportb(SERIAL_COM1_MODEM_CTRL_REG_PORT, 0x0B);
}

static inline int serial_received(void) {
    return inportb(SERIAL_COM1_LINE_STATUS_REG_PORT) & 1;
}

static inline int serial_tx_ready(void) {
    return inportb(SERIAL_COM1_LINE_STATUS_REG_PORT) & 0x20;
}

char serial_getc(void) {
    while (!serial_received());
    return inportb(SERIAL_COM1_DATA_REGISTER_PORT);
}

void serial_putc(char c) {
    while (!serial_tx_ready());
    outportb(SERIAL_COM1_DATA_REGISTER_PORT, c);
}

void serial_puts(const char *s) {
    while (*s)
        serial_putc(*s++);
}

void kprint(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);

    char buf[256];

    while (*fmt) {
        if (*fmt != '%') {
            serial_putc(*fmt++);
            continue;
        }

        fmt++;

        int width = 0;
        if (*fmt == '0')
            while (*++fmt >= '0' && *fmt <= '9')
                width = width * 10 + (*fmt - '0');

        switch (*fmt) {
        case 's': {
            const char *s = va_arg(ap, const char *);
            serial_puts(s ? s : "(null)");
            break;
        }

        case 'd':
        case 'i':
            itoa(buf, 'd', va_arg(ap, int));
            serial_puts(buf);
            break;

        case 'x':
        case 'p': {
            itoa(buf, 'x', va_arg(ap, int));

            int len = strlen(buf);
            while (len++ < width)
                serial_putc('0');

            serial_puts(buf);
            break;
        }

        case 'c':
            serial_putc((char)va_arg(ap, int));
            break;

        case '%':
            serial_putc('%');
            break;
        }

        fmt++;
    }

    va_end(ap);
}