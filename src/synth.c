#include "synth.h"
#include "ac97.h"
#include "kheap.h"
#include "kmath.h"
#include "string.h"
#include "serial.h"

static uint32 g_rng = 0xB1B00144;
static inline sint16 rsamp(void) {
    g_rng ^= g_rng << 13;
    g_rng ^= g_rng >> 17;
    g_rng ^= g_rng << 5;
    return (sint16)(g_rng & 0xFFFF);
}

synth_buf_t* ac97_synth_generate(wave_type_t wave, uint32 freq_hz, uint32 duration_ms, uint8 volume, uint32 sample_rate)
{
    if (sample_rate == 0) sample_rate = 48000;
    if (freq_hz == 0) freq_hz = 440;
    if (volume > 100) volume = 100;

    uint32 num_frames = (sample_rate * duration_ms) / 1000;
    uint32 num_samples = num_frames * 2;

    synth_buf_t* buf = (synth_buf_t*)kmalloc(sizeof(synth_buf_t));
    if (!buf) return NULL;

    buf->samples = (sint16*)kmalloc(num_samples * sizeof(sint16));
    buf->num_samples = num_samples;
    buf->sample_rate = sample_rate;

    if (!buf->samples) { kfree(buf); return NULL; }

    uint32 phase_step = (uint32)(((uint64)freq_hz << 32) / sample_rate);
    uint32 phase = 0;

    uint32 scale = (uint32)volume * 327;

    for (uint32 f = 0; f < num_frames; f++) {
        sint16 sample;

        switch (wave) {

        case WAVE_SINE:
            sample = (sint16)((lut_sine(phase) * scale) >> 15);
            break;

        case WAVE_SQUARE:
            sample = (phase < 0x80000000U) ? (sint16)(scale) : (sint16)(-scale);
            break;

        case WAVE_SAW:
            sample = (sint16)(((sint32)(phase >> 16) * scale) >> 15) - (sint16)scale;
            break;

        case WAVE_TRIANGLE: {
            uint32 p = phase;
            uint32 v;
            if (p < 0x40000000U) v = (uint32)(p >> 15);
            else if (p < 0xC0000000U) v = (uint32)(0x8000U) - (uint32)((p - 0x40000000U) >> 14);
            else v = -(uint32)(0x8000U) + (uint32)((p - 0xC0000000U) >> 15);
            sample = (sint16)((v * scale) >> 15);
            break;
        }

        case WAVE_NOISE:
        default:
            sample = (sint16)((rsamp() * scale) >> 15);
            phase = 0;
            break;
        }

        buf->samples[f * 2 + 0] = sample;
        buf->samples[f * 2 + 1] = sample;

        phase += phase_step;
    }

    return buf;
}

void ac97_synth_free(synth_buf_t* buf) {
    if (!buf) return;
    if (buf->samples) kfree(buf->samples);
    kfree(buf);
}

void ac97_play_synth(synth_buf_t* buf) {
    if (!buf || !buf->samples) return;
    ac97_play_buffer(buf->samples, buf->num_samples, (uint16)buf->sample_rate, buf->samples);
}

void ac97_play_synth_delayed(synth_buf_t* buf, uint32 delay_ms) {
    if (!buf || !buf->samples) return;
    ac97_play_buffer_delayed(buf->samples, buf->num_samples, (uint16)buf->sample_rate, buf->samples, delay_ms);
}

void ac97_synth_beep(void) {
    synth_buf_t* b = ac97_synth_generate(WAVE_SINE, 880, 120, 70, 48000);
    if (!b) return;
    ac97_play_synth(b);
    kfree(b);
}

void ac97_synth_dangit(void) {
    synth_buf_t* b = ac97_synth_generate(WAVE_SQUARE, 160, 250, 80, 48000);
    if (!b) return;
    ac97_play_synth(b);
    kfree(b);
}

void ac97_synth_tink(void) {
    synth_buf_t* b = ac97_synth_generate(WAVE_TRIANGLE, 1200, 40, 55, 48000);
    if (!b) return;
    ac97_play_synth(b);
    kfree(b);
}

void ac97_synth_laser(void) {
    const int steps = 8;
    const uint32 start_hz = 1800, end_hz = 300;
    const uint32 step_ms = 50;
    for (int i = 0; i < steps; i++) {
        uint32 freq = start_hz - ((start_hz - end_hz) * (uint32)i) / (steps - 1);
        synth_buf_t* b = ac97_synth_generate(WAVE_SAW, freq, step_ms, 72, 48000);
        if (!b) break;
        ac97_play_synth_delayed(b, i * step_ms);
        kfree(b);
    }
}

typedef struct {
    float a1, a2, b0;
    float y1, y2;
} formant_t;

static void formant_init(formant_t* f, float freq, float bw, float sr) {
    float r = expf(-PI * bw / sr);
    float cosw = cosf(2.0f * PI * freq / sr);
    f->a1 = 2.0f * r * cosw;
    f->a2 = -(r * r);
    f->b0 = 1.0f - r;
    f->y1 = f->y2 = 0.0f;
}

static inline float formant_tick(formant_t* f, float x)
{
    float y = f->b0 * x + f->a1 * f->y1 + f->a2 * f->y2;
    f->y2 = f->y1;
    f->y1 = y;
    return y;
}

// modded from Peterson & Barney (1952), raised a bit
// https://praat.org/manual/Create_formant_table__Peterson___Barney_1952_.html
// https://www.ee.columbia.edu/~dpwe/papers/PetB52-vowels.pdf

static const float VOWEL_FORMANTS[VOWEL_COUNT][4] = {
    { 1050, 1900, 2900, 3400 }, // A
    { 820, 2700, 3100, 3400 }, // E
    { 530, 3050, 3400, 3600 }, // I
    { 780, 1300, 2900, 3300 }, // O
    { 570, 1150, 2900, 3200 }, // U
};

static const float FORMANT_BW[4] = { 75.0f, 140.0f, 150.0f, 80.0f };
static const float FORMANT_GAIN[4] = { 0.75f, 1.15f, 1.1f, 1.9f };

//sigmoid so the higher notes dont earbeep you
static inline float soft_clip(float x) {
    return x / (1.0f + (x < 0.0f ? -x : x));
}

static inline float lf_glottal_tick(float* phase, float f0, float sr, float Oq) {
    // peak higher to stop buzz (0.55)
    const float ta = (1.0f - Oq) * 0.5f;

    float p = *phase;
    float tp = Oq * 0.55f;
    float te = Oq;

    float out;

    if (p < tp) {
        out = sinf(PI * p / tp);
    } else if (p < te) {
        out = 1.0f - (p - tp) / (te - tp) * 0.9f;
    } else {
        float decay = (p - te) / ta;
        if (decay > 1.0f) decay = 1.0f;
        out = -0.2f * (1.0f - decay);
    }

    *phase += f0 / sr;
    if (*phase >= 1.0f) *phase -= 1.0f;

    return out * 0.5f;
}

static inline float vibrato_tick(float* lfo_phase, float sr) {
    float v = sinf(2.0f * PI * (*lfo_phase));
    // should be pretty fem, somehwere in soprano range
    *lfo_phase += 6.8f / sr;
    if (*lfo_phase >= 1.0f) *lfo_phase -= 1.0f;
    return v;
}

static inline float vibrato_depth(uint32 i, uint32 onset_frames) {
    if (i >= onset_frames) return 1.0f;
    float t = (float)i / (float)onset_frames;
    return t * t;
}

synth_buf_t* ac97_synth_opera_note(float f0_hz, vowel_t vowel, uint32 duration_ms, uint8 volume, uint32 sample_rate) {
    if (sample_rate == 0) sample_rate = 48000;
    if (volume > 100) volume = 100;
    if ((uint32)vowel >= VOWEL_COUNT) vowel = VOWEL_A;

    uint32 num_frames = (sample_rate * duration_ms) / 1000;
    uint32 num_samples = num_frames * 2;

    synth_buf_t* buf = (synth_buf_t*)kmalloc(sizeof(synth_buf_t));
    if (!buf) return NULL;
    buf->samples = (sint16*)kmalloc(num_samples * sizeof(sint16));
    if (!buf->samples) { kfree(buf); return NULL; }
    buf->num_samples = num_samples;
    buf->sample_rate = sample_rate;

    float sr = (float)sample_rate;

    float pitch_ratio = f0_hz / 523.25f; // C5
    float hf_scale = 1.0f / (pitch_ratio * pitch_ratio);
    if (hf_scale > 1.0f) hf_scale = 1.0f;
    if (hf_scale < 0.35f) hf_scale = 0.35f;

    float gain_f[4];
    gain_f[0] = FORMANT_GAIN[0];
    gain_f[1] = FORMANT_GAIN[1];
    gain_f[2] = FORMANT_GAIN[2] * (0.6f + 0.4f * hf_scale);
    gain_f[3] = FORMANT_GAIN[3] * hf_scale;

    // TODO: arbitrary, gotta tweak later
    uint32 breath_frames = (uint32)(sr * 0.180f);
    uint32 tonal_frames = (uint32)(sr * 0.200f);
    uint32 bw_frames = (uint32)(sr * 0.080f);
    uint32 Oq_frames = (uint32)(sr * 0.100f);
    uint32 vib_onset = (uint32)(sr * 0.200f);

    float asp_lp = 0.0f;
    const float asp_coef = 0.09f;

    formant_t f1, f2, f3, f4;

    // we will lerp betwene start and target (was expf, cosf)
    const float r_start_mul = 2.5f; // match formant_init wide BW
    float r_start[3], r_target[3], cos_val[3];
    for (int k = 0; k < 3; k++) {
        float bw_wide = FORMANT_BW[k] * r_start_mul;
        float bw_narrow = FORMANT_BW[k];
        r_start[k] = expf(-PI * bw_wide / sr);
        r_target[k] = expf(-PI * bw_narrow / sr);
        cos_val[k] = cosf(2.0f * PI * VOWEL_FORMANTS[vowel][k] / sr);
    }

    float r = r_start[0];
    f1.a1 = 2.0f*r*cos_val[0]; f1.a2 = -(r*r); f1.b0 = 1.0f-r; f1.y1=f1.y2=0.0f;
    r = r_start[1];
    f2.a1 = 2.0f*r*cos_val[1]; f2.a2 = -(r*r); f2.b0 = 1.0f-r; f2.y1=f2.y2=0.0f;
    r = r_start[2];
    f3.a1 = 2.0f*r*cos_val[2]; f3.a2 = -(r*r); f3.b0 = 1.0f-r; f3.y1=f3.y2=0.0f;

    formant_init(&f4, VOWEL_FORMANTS[vowel][3], FORMANT_BW[3], sr);

    float phase = 0.0f;
    float lfo_phase = 0.0f;
    //remove DC or sound will be muddy
    float glot_hp = 0.0f;
    float prev_glot = 0.0f;

    float gain = (float)volume * (32767.0f / 100.0f) * 0.82f;

    float shim_lp1 = 0.0f, shim_lp2 = 0.0f;
    const float shim_coef = 0.12f;

    // low-pass so its more HUMAN
    float out_lp = 0.0f;
    float lp_cutoff = 9000.0f / pitch_ratio;
    if (lp_cutoff < 4000.0f) lp_cutoff = 4000.0f;
    if (lp_cutoff > 12000.0f) lp_cutoff = 12000.0f;
    float lp_coef = 1.0f - expf(-2.0f * PI * lp_cutoff / sr);

    uint32 fade_atk = (uint32)(sr * 0.015f);
    uint32 fade_rel = (uint32)(sr * 0.020f);

    const float inv_Oq_frames = (Oq_frames > 0) ? 1.0f / (float)Oq_frames : 0.0f;
    const float inv_bw_frames = (bw_frames > 0) ? 1.0f / (float)bw_frames : 0.0f;
    const float inv_breath_frames = (breath_frames > 0) ? 1.0f / (float)breath_frames : 0.0f;
    const float inv_tonal_frames = (tonal_frames > 0) ? 1.0f / (float)tonal_frames : 0.0f;
    const float inv_fade_atk = (fade_atk > 0) ? 1.0f / (float)fade_atk : 0.0f;
    const float inv_fade_rel = (fade_rel > 0) ? 1.0f / (float)fade_rel : 0.0f;

    for (uint32 i = 0; i < num_frames; i++) {
        float onset_t = (i < Oq_frames) ? (float)i * inv_Oq_frames : 1.0f;
        float onset_t2 = onset_t * onset_t;

        // lerp lerp lerp sahur
        if (i < bw_frames) {
            float bw_t = (float)i * inv_bw_frames;
            float bw_t2 = bw_t * bw_t;
            for (int k = 0; k < 3; k++) {
                float r = r_start[k] + (r_target[k] - r_start[k]) * bw_t2;
                formant_t* fk = (k == 0) ? &f1 : (k == 1) ? &f2 : &f3;
                fk->a1 = 2.0f * r * cos_val[k];
                fk->a2 = -(r * r);
                fk->b0 = 1.0f - r;
            }
        }

        float vib = vibrato_tick(&lfo_phase, sr);
        float vdepth = vibrato_depth(i, vib_onset);
        float trem = 1.0f + vib * vdepth * 0.035f;

        if (i < bw_frames) { // skip
        } else {
            float f1_mod = VOWEL_FORMANTS[vowel][0] * (1.0f + vib * vdepth * 0.007f);
            float f2_mod = VOWEL_FORMANTS[vowel][1] * (1.0f + shim_lp2 * 0.004f);
            float r1 = r_target[0];
            float c1 = cosf(2.0f * PI * f1_mod / sr);
            f1.a1 = 2.0f * r1 * c1; f1.a2 = -(r1 * r1); f1.b0 = 1.0f - r1;
            float r2 = r_target[1];
            float c2 = cosf(2.0f * PI * f2_mod / sr);
            f2.a1 = 2.0f * r2 * c2; f2.a2 = -(r2 * r2); f2.b0 = 1.0f - r2;
        }
        float Oq = 0.92f - 0.12f * onset_t2;

        // increase for higher pitch
        float jitter_depth = 0.0025f * (0.8f + 0.4f * pitch_ratio);
        if (jitter_depth > 0.005f) jitter_depth = 0.005f;
        float jitter = (float)(sint16)rsamp() * (1.0f / 32768.0f) * jitter_depth;
        float f0 = f0_hz * (1.0f + vib * vdepth * 0.01733f + jitter);

        float raw_glot = lf_glottal_tick(&phase, f0, sr, Oq);
        glot_hp = 0.999f * glot_hp + raw_glot - prev_glot;
        prev_glot = raw_glot;
        float tonal_src = glot_hp;

        float noise_raw = (float)(sint16)rsamp() * (1.0f / 32768.0f);
        asp_lp = asp_lp + asp_coef * (noise_raw - asp_lp);

        float shim_raw = (float)(sint16)rsamp() * (1.0f / 32768.0f);
        shim_lp1 = shim_lp1 + shim_coef * (shim_raw - shim_lp1);
        shim_lp2 = shim_lp2 + shim_coef * (shim_lp1 - shim_lp2);

        float breath_t = (i < breath_frames) ? (float)i * inv_breath_frames : 1.0f;
        float breath_env = (i < breath_frames)
                         ? (1.0f - breath_t) * (breath_t < 0.15f ? breath_t / 0.15f : 1.0f)
                         : 0.0f;
        float tonal_env = (i < tonal_frames) ? (float)i * inv_tonal_frames : 1.0f;
        tonal_env = tonal_env * tonal_env;

        float shim_env = (i < tonal_frames) ? (1.0f - tonal_env) * 0.12f : 0.0f;

        float shim_steady = shim_lp2 * 0.06f;
        float src = tonal_src * tonal_env * (1.0f + shim_steady)
                  + asp_lp * (breath_env * 0.60f + 0.08f)
                  + shim_lp2 * shim_env;

        float out = (formant_tick(&f1, src) * gain_f[0]
                  + formant_tick(&f2, src) * gain_f[1]
                  + formant_tick(&f3, src) * gain_f[2]
                  + formant_tick(&f4, src) * gain_f[3]) * trem;

        out = soft_clip(out);

        out_lp += lp_coef * (out - out_lp);
        out = out_lp;

        float env = 1.0f;
        if (i < fade_atk) env = (float)i * inv_fade_atk;
        else if (i > num_frames - fade_rel) env = (float)(num_frames - i) * inv_fade_rel;

        sint16 s = (sint16)(out * env * gain);
        buf->samples[i * 2 + 0] = s;
        buf->samples[i * 2 + 1] = s;
    }

    return buf;
}

void ac97_synth_opera(void) {
    static const struct { float hz; vowel_t v; uint32 ms; } phrase[] = {
        { 523.25f, VOWEL_A, 800 },
        { 659.25f, VOWEL_O, 800 },
        { 783.99f, VOWEL_I, 1200 },
        { 880.00f, VOWEL_E, 800 },
        { 987.77f, VOWEL_U, 800 },
        { 1046.50f, VOWEL_A, 1200 },
    };
    ac97_mixer_batch_begin();
    uint32 current_delay = 0;
    for (int i = 0; i < 6; i++) {
        synth_buf_t* b = ac97_synth_opera_note(phrase[i].hz, phrase[i].v, phrase[i].ms, 75, 48000);
        if (!b) continue;
        ac97_play_synth_delayed(b, current_delay);
        current_delay += phrase[i].ms;
        kfree(b);
    }
    ac97_mixer_batch_end();
}