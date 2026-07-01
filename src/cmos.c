#include "cmos.h"
#include "ports.h"

static uint8_t read_register(uint8_t reg) {
    outportb(CMOS_ADDRESS, reg);
    return inportb(CMOS_DATA);
}

static uint8_t bcd_to_dec(uint8_t bcd) {
    return (bcd / 16 * 10) + (bcd & 0x0F);
}

// adjusted for GMT +8 because i use it but you can adjust within kOSeki
int g_kronii_timezone_ofs = 8;

void get_time(time_t* time) {
    while (read_register(CMOS_STATUS_A) & 0x80);

    time->second = read_register(CMOS_SECOND);
    time->minute = read_register(CMOS_MINUTE);
    time->hour = read_register(CMOS_HOUR);
    time->day = read_register(CMOS_DAY);
    time_t prev_time = *time;
    
    do {
        prev_time = *time;
        while (read_register(CMOS_STATUS_A) & 0x80);

        time->second = read_register(CMOS_SECOND);
        time->minute = read_register(CMOS_MINUTE);
        time->hour = read_register(CMOS_HOUR);
        time->day = read_register(CMOS_DAY);
        time->month = read_register(CMOS_MONTH);
        time->year = read_register(CMOS_YEAR);

    } while ( (prev_time.second != time->second) || (prev_time.minute != time->minute) || (prev_time.hour != time->hour) ||
                (prev_time.day != time->day) || (prev_time.month != time->month) || (prev_time.year != time->year));

    uint8_t status_b = read_register(CMOS_STATUS_B);
    
    uint8_t h = time->hour;
    uint8_t b = ((h&0x7F)>>4)*10 + (h&0x0F);
    int raw = b + ((h&0x80) ? (b==12?0:12) : (b==12?-12:0)) + g_kronii_timezone_ofs;
    raw = ((raw % 24) + 24) % 24; // shift into [0,24]
    time->hour = (uint8_t)raw;
    
    if (!(status_b & 0x04)) {
        time->second = bcd_to_dec(time->second);
        time->minute = bcd_to_dec(time->minute);
        time->day = bcd_to_dec(time->day);
        time->month = bcd_to_dec(time->month);
        time->year = bcd_to_dec(time->year) + 2000;
    } else {
        time->year += 2000;
    }
}