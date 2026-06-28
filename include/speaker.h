#ifndef SPEAKER_H
#define SPEAKER_H

#include "types.h"

void play_sound(uint32 nFrequency);
void nosound();
void beep(int frequency, int duration_ms);

#endif
