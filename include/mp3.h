#ifndef MP3_H
#define MP3_H

#include "types.h"

sint16* mp3_decode(const uint8* mp3_data, uint32 mp3_size, uint32* out_num_samples, int* out_sample_rate);

#endif
