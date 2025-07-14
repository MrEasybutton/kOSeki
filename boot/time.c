#include "time.h"
#include "input.h"

extern unsigned char inportb(unsigned short port);
extern void outportb(unsigned short port, unsigned char data);

#define CMOS_ADDRESS 0x70
#define CMOS_DATA 0x71

#define CMOS_SECOND 0x00
#define CMOS_MINUTE 0x02
#define CMOS_HOUR 0x04
#define CMOS_DAY 0x07
#define CMOS_MONTH 0x08
#define CMOS_YEAR 0x09
#define CMOS_CENTURY 0x32
#define CMOS_STATUS_A 0x0A
#define CMOS_STATUS_B 0x0B

static unsigned char bcd_to_binary(unsigned char bcd) {
    return ((bcd / 16) * 10) + (bcd & 0x0F);
}

static unsigned char read_cmos(unsigned char reg) {
    unsigned char prev_state = inportb(0x70) & 0x80;
    outportb(CMOS_ADDRESS, (reg & 0x7F) | prev_state);
    
    return inportb(CMOS_DATA);
}

void read_time(RTCTime* time) {
    while ((read_cmos(CMOS_STATUS_A) & 0x80));
    
    unsigned char second = read_cmos(CMOS_SECOND);
    unsigned char minute = read_cmos(CMOS_MINUTE);
    unsigned char hour = read_cmos(CMOS_HOUR);
    unsigned char day = read_cmos(CMOS_DAY);
    unsigned char month = read_cmos(CMOS_MONTH);
    unsigned char year = read_cmos(CMOS_YEAR);
    unsigned char century = read_cmos(CMOS_CENTURY);
    
    unsigned char status_b = read_cmos(CMOS_STATUS_B);
    
    if (!(status_b & 0x04)) {
        second = bcd_to_binary(second);
        minute = bcd_to_binary(minute);
        hour = bcd_to_binary(hour);
        day = bcd_to_binary(day);
        month = bcd_to_binary(month);
        year = bcd_to_binary(year);
        century = bcd_to_binary(century);
    }

    if (!(status_b & 0x02) && (hour & 0x80)) {
        hour = ((hour & 0x7F) + 12) % 24;
    }
    
    time->second = second;
    time->minute = minute;
    time->hour = hour;
    time->day = day;
    time->month = month;
    time->year = year;
    time->century = century;
}

char* format_time(RTCTime* time, char* buffer) {
    unsigned char hour = time->hour > 23 ? 0 : time->hour;
    unsigned char minute = time->minute > 59 ? 0 : time->minute;
    unsigned char second = time->second > 59 ? 0 : time->second;
    hour += 8; // i use gmt+8

    buffer[0] = '0' + (hour / 10);
    buffer[1] ='0' + (hour % 10);
    buffer[2] = ':';
    buffer[3] = '0' + (minute / 10);
    buffer[4] = '0' + (minute % 10);
    buffer[5] = ':';
    buffer[6] = '0' + (second / 10);
    buffer[7] = '0' + (second % 10);
    buffer[8] = '\0';
    
    return buffer;
}

char* format_date(RTCTime* time, char* buffer) {
    unsigned char day = time->day > 31 ? 1 : time->day;
    unsigned char month = time->month > 12 ? 1 : time->month;
    unsigned char year = time->year > 99 ? 0 : time->year;
    unsigned char century = time->century;

    buffer[0] = '0' + (day / 10);
    buffer[1] = '0' + (day % 10);
    buffer[2] = '/';
    buffer[3] = '0' + (month / 10);
    buffer[4] = '0' + (month % 10);
    buffer[5] = '/';
    buffer[6] = '0' + (century / 10);
    buffer[7] = '0' + (century % 10);
    buffer[8] = '0' + (year / 10);
    buffer[9] = '0' + (year % 10);
    buffer[10] = '\0';

    return buffer;
}