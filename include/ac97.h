#ifndef AC97_H
#define AC97_H

#include "types.h"

void ac97_init(uint8 bus, uint8 slot, uint8 func, uint8 irq);
void ac97_update_streams(void);
void ac97_mixer_batch_begin(void);
void ac97_mixer_batch_end(void);
int ac97_play(const char* filename);
int ac97_play_buffer(sint16* pcm, uint32 num_samples, uint16 sample_rate, void* to_free);
int ac97_play_buffer_delayed(sint16* pcm, uint32 num_samples, uint16 sample_rate, void* to_free, uint32 delay_ms);
void ac97_stop_voice(int handle);
void ac97_stop_all(void);
void ac97_mixer_batch_begin(void);
void ac97_mixer_batch_end(void);
BOOL ac97_get_voice_progress(int handle, uint32* current, uint32* total);
BOOL ac97_is_playing(void);

#endif
