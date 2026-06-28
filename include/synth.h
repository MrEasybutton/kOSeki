#ifndef SYNTH_H
#define SYNTH_H

#include "utils.h"

typedef enum {
    WAVE_SINE = 0,
    WAVE_SQUARE = 1,
    WAVE_SAW = 2,
    WAVE_TRIANGLE = 3,
    WAVE_NOISE = 4,
} wave_type_t;

typedef struct {
    sint16* samples;
    uint32 num_samples; // total int16 samples
    uint32 sample_rate;
} synth_buf_t;

typedef enum {
    BR_PARAM_VOWEL = 0, BR_PARAM_VOLUME,
    BR_PARAM_OQ_START, BR_PARAM_OQ_TARGET,
    BR_PARAM_VIB_RATE, BR_PARAM_VIB_DEPTH, BR_PARAM_VIB_ONSET,
    BR_PARAM_BREATH, BR_PARAM_SHIMMER,
} br_param_id_t;

//we gotta kfree ->samples after
synth_buf_t* ac97_synth_generate(wave_type_t wave, uint32 freq_hz, uint32 duration_ms, uint8 volume, uint32 sample_rate);

void ac97_play_synth(synth_buf_t* buf);

void ac97_synth_free(synth_buf_t* buf);

void ac97_synth_beep(void);
void ac97_synth_dangit(void);
void ac97_synth_tink(void);
void ac97_synth_laser(void);

typedef enum {
    VOWEL_A = 0,
    VOWEL_E = 1,
    VOWEL_I = 2,
    VOWEL_O = 3,
    VOWEL_U = 4,
    VOWEL_COUNT
} vowel_t;

synth_buf_t* ac97_synth_opera_note(float f0_hz, vowel_t vowel, uint32 duration_ms, uint8 volume, uint32 sample_rate);

void ac97_synth_opera(void);

#endif