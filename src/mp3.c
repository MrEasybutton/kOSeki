#include "mp3.h"
#include "kheap.h"
#include "string.h"
#include "serial.h"

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_NO_SIMD
#include "minimp3.h"

sint16* mp3_decode(const uint8* data, uint32 size, uint32* out_num_samples, int* out_sample_rate) {
    kprint("[MP3] begin decode for %d bytes\n", size);
    mp3dec_t mp3d;
    mp3dec_init(&mp3d);
    
    mp3dec_frame_info_t info;
    
    uint32 capacity_samples = (size * 10) / sizeof(sint16);
    if (capacity_samples < MINIMP3_MAX_SAMPLES_PER_FRAME) capacity_samples = MINIMP3_MAX_SAMPLES_PER_FRAME * 10;
    
    sint16* full_pcm = (sint16*)kmalloc(capacity_samples * sizeof(sint16));
    if (!full_pcm) {
        kprint("[MP3] Failed to allocate initial buffer (%d samples)\n", capacity_samples);
        return NULL;
    }

    uint32 current_sample = 0;
    uint32 offset = 0;
    int frames = 0;
    int sample_rate = 0;

    while (offset < size) {
        if (current_sample + MINIMP3_MAX_SAMPLES_PER_FRAME > capacity_samples) {
            capacity_samples *= 2;
            sint16* new_buf = (sint16*)krealloc(full_pcm, capacity_samples * sizeof(sint16));
            if (!new_buf) {
                kprint("[MP3] Failed to regrow buffer to %d samples\n", capacity_samples);
                break;
            }
            full_pcm = new_buf;
        }

        int samples = mp3dec_decode_frame(&mp3d, data + offset, size - offset, full_pcm + current_sample, &info);
        if (samples > 0) {
            current_sample += samples * info.channels;
            if (sample_rate == 0) sample_rate = info.hz;
            frames++;
            
            if (frames % 100 == 0) {
                kprint("[MP3] Decoded %d frames... (%d%%)\n", frames, (offset * 100) / size);
            }
        }
        
        if (info.frame_bytes == 0) break;
        offset += info.frame_bytes;
    }

    kprint("[MP3] Decode finished. Frames: %d, Samples: %d, Rate: %d\n", frames, current_sample, sample_rate);
    
    // trim
    sint16* final_pcm = (sint16*)krealloc(full_pcm, current_sample * sizeof(sint16));
    if (final_pcm) full_pcm = final_pcm;

    *out_num_samples = current_sample;
    *out_sample_rate = sample_rate;
    
    return full_pcm;
}