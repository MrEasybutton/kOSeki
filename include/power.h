#ifndef POWER_H
#define POWER_H
#include "types.h"

// NO ONE ROCK SHOULD HAVE ALL THAT POWER
extern void shutdown(uint32 pm1a_cnt, uint32 pm1b_cnt);
extern void reboot(void);
extern uint32 PM1A_CNT_BLK, PM1B_CNT_BLK;

#endif