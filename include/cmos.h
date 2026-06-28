#ifndef CMOS_H
#define CMOS_H

#include "types.h"
#include <stdint.h>

#define CMOS_ADDRESS 0x70
#define CMOS_DATA    0x71

#define CMOS_SECOND  0x00
#define CMOS_MINUTE  0x02
#define CMOS_HOUR    0x04
#define CMOS_DAY     0x07
#define CMOS_MONTH   0x08
#define CMOS_YEAR    0x09
#define CMOS_STATUS_A 0x0A
#define CMOS_STATUS_B 0x0B

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} time_t;

void get_time(time_t* time);

#endif