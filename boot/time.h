#ifndef TIME_H
#define TIME_H

typedef struct {
    unsigned char second;
    unsigned char minute;
    unsigned char hour;
    unsigned char day;
    unsigned char month;
    unsigned char year;
    unsigned char century;
} RTCTime;

void read_time(RTCTime* time);
char* format_time(RTCTime* time, char* buffer);
char* format_date(RTCTime* time, char* buffer);
#endif