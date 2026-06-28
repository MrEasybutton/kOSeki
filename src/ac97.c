#include "mp3.h"
#include "ac97.h"
#include "pci.h"
#include "console.h"
#include "serial.h"
#include "ports.h"
#include "8259_pic.h"
#include "utils.h"
#include "fat32.h"
#include "pmm.h"
#include "types.h"
#include "kheap.h"
#include "string.h"
#include "isr.h"
#include "minimp3.h"

extern void delay(uint32 value);

#define AC97_MIXER_MASTER_VOL 0x02
#define AC97_MIXER_PCM_VOL 0x18
#define AC97_EXT_AUDIO_ID 0x28
#define AC97_EXT_AUDIO_ST 0x2A
#define AC97_MIXER_SAMPLE_RATE 0x2C
#define AC97_PO_BDBAR 0x10
#define AC97_PO_CIV 0x14
#define AC97_PO_LVI 0x15
#define AC97_PO_SR 0x16
#define AC97_PO_CR 0x1B
#define AC97_GLOB_CNT 0x2C
#define AC97_GLOB_STA 0x30
#define AC97_SR_BCIS (1 << 3)
#define AC97_CR_RPBM (1 << 0)
#define AC97_CR_RR (1 << 1)
#define AC97_CR_IOCE (1 << 4)
#define AC97_GC_COLD_RST (1 << 1)
#define AC97_GC_WARM_RST (1 << 2)
#define AC97_GS_PCR (1 << 8)
#define AC97_VRA (1 << 0)

#define RING_SIZE 32
#define CHUNK_SIZE 4096
#define MAX_VOICES 256
#define STREAM_BUFFER_SAMPLES (256 * 1024 / sizeof(sint16)) //128k samples

typedef struct {
    uint32 pointer;
    uint16 length;
    uint16 flags;
} __attribute__((packed)) ac97_bd_t;

#define AC97_BD_FLAG_IOC (1 << 15)

typedef struct {
    sint16* pcm_ptr;
    uint32 total_samples;
    uint32 current_sample;
    uint32 delay_samples;
    void* file_base;
    uint32 generation;
    BOOL active;

    BOOL is_streaming;
    uint32 available_samples;
    uint32 start_cluster;
    uint32 file_offset; // current pos in file
    uint32 file_size;
    int format; // 0=wav, 1=mp3
    uint16 sample_rate;
    
    mp3dec_t mp3d;
    uint8* mp3_chunk;
    uint32 mp3_chunk_size;
    uint32 mp3_chunk_offset;
    uint32 mp3_chunk_limit;
} audio_voice_t;

static uint32 g_mixer_base = 0, g_bm_base = 0;
static audio_voice_t g_voices[MAX_VOICES] = {0};
static uint32 g_generations[MAX_VOICES] = {0};
static ac97_bd_t* g_bdl = NULL;
static BOOL g_mixer_active = FALSE;
static BOOL g_mixer_batch = FALSE;
static int g_active_voices = 0;
static int g_max_voice_index = 0;

void ac97_mixer_batch_begin(void) {
    g_mixer_batch = TRUE;
}

static void mix_into_buffer(sint16* output, uint32 num_samples) {
    memset(output, 0, num_samples * sizeof(sint16));
    if (g_active_voices <= 0) return;

    for (int v = 0; v <= g_max_voice_index; v++) {
        if (!g_voices[v].active) continue;

        uint32 samples_to_process = num_samples;
        uint32 output_offset = 0;

        if (g_voices[v].delay_samples > 0) {
            uint32 skip = (g_voices[v].delay_samples > samples_to_process)
                          ? samples_to_process
                          : g_voices[v].delay_samples;
            g_voices[v].delay_samples -= skip;
            samples_to_process -= skip;
            output_offset += skip;
        }

        if (samples_to_process > 0) {
            uint32 available;
            if (g_voices[v].is_streaming) {
                // protecc from underflow
                if (g_voices[v].current_sample >= g_voices[v].available_samples)
                    available = 0;
                else
                    available = g_voices[v].available_samples - g_voices[v].current_sample;
            } else {
                available = (g_voices[v].total_samples > g_voices[v].current_sample) 
                          ? (g_voices[v].total_samples - g_voices[v].current_sample) : 0;
            }

            uint32 to_mix = (available > samples_to_process) ? samples_to_process : available;
            
            if (g_voices[v].is_streaming) to_mix &= ~1;

            if (to_mix > 0) {
                sint16* out_ptr = output + output_offset;
                
                if (g_voices[v].is_streaming) {
                    uint32 mask = STREAM_BUFFER_SAMPLES - 1;
                    uint32 curr = g_voices[v].current_sample;
                    sint16* pcm = g_voices[v].pcm_ptr;
                    
                    for (uint32 i = 0; i < to_mix; i++) {
                        sint32 mixed = (sint32)out_ptr[i] + (sint32)pcm[(curr + i) & mask];
                        if (mixed > 32767) mixed = 32767;
                        else if (mixed < -32768) mixed = -32768;
                        out_ptr[i] = (sint16)mixed;
                    }
                } else {
                    sint16* src_ptr = g_voices[v].pcm_ptr + g_voices[v].current_sample;
                    for (uint32 i = 0; i < to_mix; i++) {
                        sint32 mixed = (sint32)out_ptr[i] + (sint32)src_ptr[i];
                        if (mixed > 32767) mixed = 32767;
                        else if (mixed < -32768) mixed = -32768;
                        out_ptr[i] = (sint16)mixed;
                    }
                }

                g_voices[v].current_sample += to_mix;
            }

            //check for end of voice
            if (g_voices[v].total_samples != (uint32)-1 && g_voices[v].current_sample >= g_voices[v].total_samples) {
                g_voices[v].active = FALSE;
                g_active_voices--;
                
                if (v == g_max_voice_index) {
                    while (g_max_voice_index > 0 && !g_voices[g_max_voice_index].active) g_max_voice_index--;
                }

                if (g_voices[v].file_base) {
                    kfree(g_voices[v].file_base);
                    g_voices[v].file_base = NULL;
                }
                if (g_voices[v].is_streaming) {
                    if (g_voices[v].pcm_ptr) {
                        void* ptr = g_voices[v].pcm_ptr;
                        g_voices[v].pcm_ptr = NULL;
                        kfree(ptr);
                    }
                    if (g_voices[v].mp3_chunk) {
                        void* ptr = g_voices[v].mp3_chunk;
                        g_voices[v].mp3_chunk = NULL;
                        kfree(ptr);
                    }
                }
            }
        }
    }
}

static void ac97_hw_start(uint16 sample_rate) {
    outports(g_mixer_base + AC97_MIXER_SAMPLE_RATE, sample_rate);
    outportb(g_bm_base + AC97_PO_CR, AC97_CR_RR);
    uint32 t = 100000;
    while ((inportb(g_bm_base + AC97_PO_CR) & AC97_CR_RR) && t--);
    
    outportl(g_bm_base + AC97_PO_BDBAR, (uint32)g_bdl);
    
    for (int i = 0; i < 8; i++) {
        mix_into_buffer((sint16*)g_bdl[i].pointer, CHUNK_SIZE / sizeof(sint16));
        g_bdl[i].length = CHUNK_SIZE / 2;
        g_bdl[i].flags = AC97_BD_FLAG_IOC;
    }
    
    outportb(g_bm_base + AC97_PO_LVI, 7);
    outportb(g_bm_base + AC97_PO_CR, AC97_CR_RPBM | AC97_CR_IOCE);
    g_mixer_active = TRUE;
}

void ac97_mixer_start(void) {
    if (g_bm_base == 0 || g_mixer_active) return;
    ac97_hw_start(48000);
}

void ac97_mixer_batch_end(void) {
    g_mixer_batch = FALSE;
    ac97_mixer_start();
}

static void ac97_irq_handler(REGISTERS* regs) {
    (void)regs;
    if (g_bm_base == 0) return;

    uint16 status = inports(g_bm_base + AC97_PO_SR);
    if (!(status & AC97_SR_BCIS)) return;
    outports(g_bm_base + AC97_PO_SR, AC97_SR_BCIS);

    uint8 civ = inportb(g_bm_base + AC97_PO_CIV);
    uint8 lvi = inportb(g_bm_base + AC97_PO_LVI);

    if (g_active_voices <= 0) {
        if (civ == lvi) {
            outportb(g_bm_base + AC97_PO_CR, 0);
            g_mixer_active = FALSE;
        }
        return;
    }

    //refill
    while (TRUE) {
        uint8 next_lvi = (lvi + 1) % RING_SIZE;
        if (next_lvi == civ) break;

        lvi = next_lvi;
        mix_into_buffer((sint16*)g_bdl[lvi].pointer, CHUNK_SIZE / sizeof(sint16));
        g_bdl[lvi].length = CHUNK_SIZE / 2;
        g_bdl[lvi].flags = AC97_BD_FLAG_IOC;
        outportb(g_bm_base + AC97_PO_LVI, lvi);
    }
}

void ac97_init(uint8 bus, uint8 slot, uint8 func, uint8 irq) {
    pci_enable_bus_mastering(bus, slot, func);
    g_mixer_base = pci_get_bar(bus, slot, func, 0).base_address;
    g_bm_base = pci_get_bar(bus, slot, func, 1).base_address;
    
    if (g_mixer_base == 0 || g_bm_base == 0) {
        kprint("AC97: Error - BARs not assigned!\n");
        return;
    }

    register_interrupt_handler(IRQ_BASE + irq, ac97_irq_handler);
    pic8259_unmask(irq);

    //cold reset
    outportl(g_bm_base + AC97_GLOB_CNT, 0);
    delay(1000); 
    outportl(g_bm_base + AC97_GLOB_CNT, AC97_GC_COLD_RST);
    
    uint32 timeout = 500000;
    while (!(inportl(g_bm_base + AC97_GLOB_STA) & AC97_GS_PCR) && timeout > 0) timeout--;

    if (timeout == 0) {
        kprint("AC97: Error - Codec not ready (PCR timeout)!\n");
    }

    // enable VRA
    uint16 ext_id = inports(g_mixer_base + AC97_EXT_AUDIO_ID);
    if (ext_id & 1) {
        outports(g_mixer_base + AC97_EXT_AUDIO_ST, inports(g_mixer_base + AC97_EXT_AUDIO_ST) | AC97_VRA);
    }

    // set ts a bit lower
    outports(g_mixer_base + AC97_MIXER_MASTER_VOL, 0x0202);
    outports(g_mixer_base + AC97_MIXER_PCM_VOL, 0x0202);
    outports(g_mixer_base + AC97_MIXER_SAMPLE_RATE, 48000);

    if (!g_bdl) {
        g_bdl = (ac97_bd_t*)pmm_alloc_block();
        if (!g_bdl) return;
        memset(g_bdl, 0, 4096);
        for (int i = 0; i < RING_SIZE; i++) {
            g_bdl[i].pointer = (uint32)pmm_alloc_blocks(CHUNK_SIZE / 4096);
            if (!g_bdl[i].pointer) return;
            g_bdl[i].length = CHUNK_SIZE / 2;
            g_bdl[i].flags = AC97_BD_FLAG_IOC;
        }
    }

    kprint("AC97: init with mixer, IRQ %d\n", irq);
}

#define MP3_CHUNK_READ_SIZE 16384
#define REFILL_SAMPLES_PER_TICK 16384
static sint16 g_refill_scratch[REFILL_SAMPLES_PER_TICK];

void ac97_update_streams(void) {
    if (g_active_voices <= 0) return;

    for (int v = 0; v <= g_max_voice_index; v++) {
        if (!g_voices[v].active || !g_voices[v].is_streaming) continue;

        uint32 played = g_voices[v].current_sample;
        uint32 available = g_voices[v].available_samples;
        
        //treat as empty jic played is ahead
        uint32 buffered = (available > played) ? (available - played) : 0;
        
        if (buffered > (STREAM_BUFFER_SAMPLES * 7) / 8) continue;

        uint32 can_write = STREAM_BUFFER_SAMPLES - buffered - 2; 
        if (can_write < 1024) continue;
        
        //limit
        if (can_write > REFILL_SAMPLES_PER_TICK) can_write = REFILL_SAMPLES_PER_TICK;
        can_write &= ~1; // maintain even or you get earbeeped

        if (g_voices[v].format == 0) { //wav
            uint32 bytes_to_read = can_write * sizeof(sint16);
            if (g_voices[v].file_offset + bytes_to_read > g_voices[v].file_size)
                bytes_to_read = g_voices[v].file_size - g_voices[v].file_offset;
            
            bytes_to_read &= ~1; 

            if (bytes_to_read > 0) {
                int read = fat_read_file_offset(g_voices[v].start_cluster, g_voices[v].file_offset, bytes_to_read, g_refill_scratch);
                if (read > 0) {
                    uint32 samples_read = (uint32)read / 2;
                    uint32 mask = STREAM_BUFFER_SAMPLES - 1;
                    for (uint32 i = 0; i < samples_read; i++) {
                        g_voices[v].pcm_ptr[(g_voices[v].available_samples + i) & mask] = g_refill_scratch[i];
                    }
                    g_voices[v].available_samples += samples_read;
                    g_voices[v].file_offset += (uint32)read;
                }
            } else if (g_voices[v].file_offset >= g_voices[v].file_size) {
                g_voices[v].total_samples = g_voices[v].available_samples;
            }
        } else if (g_voices[v].format == 1) { //mp3
            int frames_decoded = 0;
            uint32 mask = STREAM_BUFFER_SAMPLES - 1;
            while (can_write > MINIMP3_MAX_SAMPLES_PER_FRAME && frames_decoded < 4) {
                if (g_voices[v].mp3_chunk_offset + 1024 > g_voices[v].mp3_chunk_limit) {
                    uint32 left = g_voices[v].mp3_chunk_limit - g_voices[v].mp3_chunk_offset;
                    if (left > 0) memmove(g_voices[v].mp3_chunk, g_voices[v].mp3_chunk + g_voices[v].mp3_chunk_offset, left);
                    
                    uint32 to_read = MP3_CHUNK_READ_SIZE - left;
                    if (g_voices[v].file_offset + to_read > g_voices[v].file_size)
                        to_read = g_voices[v].file_size - g_voices[v].file_offset;
                    
                    if (to_read > 0) {
                        fat_read_file_offset(g_voices[v].start_cluster, g_voices[v].file_offset, to_read, g_voices[v].mp3_chunk + left);
                        g_voices[v].file_offset += to_read;
                        g_voices[v].mp3_chunk_limit = left + to_read;
                    } else {
                        g_voices[v].mp3_chunk_limit = left;
                    }
                    g_voices[v].mp3_chunk_offset = 0;
                    
                    if (g_voices[v].mp3_chunk_limit == 0) {
                        g_voices[v].total_samples = g_voices[v].available_samples;
                        break;
                    }
                }

                mp3dec_frame_info_t info;
                sint16 frame_pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
                int samples = mp3dec_decode_frame(&g_voices[v].mp3d, 
                                                 g_voices[v].mp3_chunk + g_voices[v].mp3_chunk_offset, 
                                                 g_voices[v].mp3_chunk_limit - g_voices[v].mp3_chunk_offset, 
                                                 frame_pcm, &info);
                
                if (samples > 0) {
                    frames_decoded++;
                    
                    //estimate (sucks, fix for v3.1)
                    if (g_voices[v].total_samples == (uint32)-1 && info.frame_bytes > 0) {
                        uint32 estimated_frames = g_voices[v].file_size / info.frame_bytes;
                        g_voices[v].total_samples = estimated_frames * (uint32)samples * 2;
                    }

                    if (info.channels == 1) {
                        for (uint32 i = 0; i < (uint32)samples; i++) {
                            g_voices[v].pcm_ptr[(g_voices[v].available_samples + i*2) & mask] = frame_pcm[i];
                            g_voices[v].pcm_ptr[(g_voices[v].available_samples + i*2 + 1) & mask] = frame_pcm[i];
                        }
                        g_voices[v].available_samples += (uint32)samples * 2;
                        can_write -= (uint32)samples * 2;
                    } else {
                        for (uint32 i = 0; i < (uint32)samples * 2; i++) {
                            g_voices[v].pcm_ptr[(g_voices[v].available_samples + i) & mask] = frame_pcm[i];
                        }
                        g_voices[v].available_samples += (uint32)samples * 2;
                        can_write -= (uint32)samples * 2;
                    }
                }
                
                if (info.frame_bytes == 0) break;
                g_voices[v].mp3_chunk_offset += info.frame_bytes;
            }
        }
    }
}

int ac97_play_buffer(sint16* pcm, uint32 num_samples, uint16 sample_rate, void* to_free) {
    return ac97_play_buffer_delayed(pcm, num_samples, sample_rate, to_free, 0);
}

int ac97_play_buffer_delayed(sint16* pcm, uint32 num_samples, uint16 sample_rate,
                              void* to_free, uint32 delay_ms) {
    if (g_mixer_base == 0) return -1;

    int slot = -1;
    cli();
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!g_voices[i].active) { slot = i; break; }
    }

    if (slot == -1) {
        sti();
        kprint("[AC97] Mixer full!\n");
        return -1;
    }

    g_generations[slot]++;
    
    //reset voice states
    g_voices[slot].pcm_ptr = pcm;
    g_voices[slot].total_samples = num_samples;
    g_voices[slot].current_sample = 0;
    g_voices[slot].delay_samples = (sample_rate * 2 * delay_ms) / 1000;
    g_voices[slot].file_base = to_free;
    g_voices[slot].generation = g_generations[slot];
    g_voices[slot].active = TRUE;
    
    g_voices[slot].is_streaming = FALSE;
    g_voices[slot].available_samples = 0;
    g_voices[slot].mp3_chunk = NULL;

    g_active_voices++;
    if (slot > g_max_voice_index) g_max_voice_index = slot;

    int handle = (g_voices[slot].generation << 8) | slot;

    if (!g_mixer_active && !g_mixer_batch)
        ac97_hw_start(sample_rate);

    sti();
    return handle;
}

int ac97_play(const char* filename) {
    if (g_mixer_base == 0) return -1;

    FAT_dirent* dirent = fat_find_file(filename);
    if (!dirent) return -1;
    uint32 file_size = dirent->Size;
    uint32 start_cluster = (dirent->FirstClusterHigh << 16) | dirent->FirstClusterLow;
    kfree(dirent);

    int slot = -1;
    cli();
    for (int i = 0; i < MAX_VOICES; i++) {
        if (!g_voices[i].active) { slot = i; break; }
    }

    if (slot == -1) {
        sti();
        kprint("[AC97] Mixer full!\n");
        return -1;
    }

    g_generations[slot]++;
    g_voices[slot].active = TRUE;
    g_voices[slot].total_samples = (uint32)-1;
    g_voices[slot].generation = g_generations[slot];
    if (slot > g_max_voice_index) g_max_voice_index = slot;
    sti();
    
    sint16* ring_buf = (sint16*)kmalloc(STREAM_BUFFER_SAMPLES * sizeof(sint16));
    if (!ring_buf) {
        g_voices[slot].active = FALSE;
        return -1;
    }
    memset(ring_buf, 0, STREAM_BUFFER_SAMPLES * sizeof(sint16));

    g_voices[slot].pcm_ptr = ring_buf;
    g_voices[slot].current_sample = 0;
    g_voices[slot].delay_samples = 0;
    g_voices[slot].file_base = NULL;
    g_voices[slot].is_streaming = TRUE;
    g_voices[slot].available_samples = 0;
    g_voices[slot].start_cluster = start_cluster;
    g_voices[slot].file_offset = 0;
    g_voices[slot].file_size = file_size;

    int handle = (g_voices[slot].generation << 8) | slot;

    const char* ext = strrchr(filename, '.');
    if (ext && (strcmp(ext, ".mp3") == 0 || strcmp(ext, ".MP3") == 0)) {
        g_voices[slot].format = 1;
        g_voices[slot].sample_rate = 44100;
        g_voices[slot].mp3_chunk = (uint8*)kmalloc(MP3_CHUNK_READ_SIZE);
        g_voices[slot].mp3_chunk_offset = 0;
        g_voices[slot].mp3_chunk_limit = 0;
        mp3dec_init(&g_voices[slot].mp3d);
    } else {
        g_voices[slot].format = 0;
        g_voices[slot].sample_rate = 48000;
        g_voices[slot].mp3_chunk = NULL;
        
        uint8 header[512];
        fat_read_file_offset(start_cluster, 0, 512, header);
        if (memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "WAVE", 4) == 0) {
            g_voices[slot].sample_rate = *(uint32*)(header + 24);
            for (int i = 0; i < 500; i++) {
                if (memcmp(header + i, "data", 4) == 0) {
                    g_voices[slot].file_offset = i + 8;
                    g_voices[slot].total_samples = (*(uint32*)(header + i + 4)) / sizeof(sint16);
                    break;
                }
            }
        }
    }

    // initial so it wont stutter
    for (int i = 0; i < 8; i++) {
        ac97_update_streams();
    }

    cli();
    g_active_voices++;
    if (g_voices[slot].total_samples == 0 && g_voices[slot].format == 0) g_voices[slot].total_samples = (uint32)-1;

    if (!g_mixer_active)
        ac97_hw_start(g_voices[slot].sample_rate);
    sti();

    return handle;
}

void ac97_stop_voice(int handle) {
    if (handle < 0) return;
    int slot = handle & 0xFF;
    uint32 gen = handle >> 8;
    if (slot >= MAX_VOICES) return;

    cli();
    if (g_voices[slot].active && g_voices[slot].generation == gen) {
        g_voices[slot].active = FALSE;
        g_active_voices--;
        if (g_voices[slot].file_base) {
            kfree(g_voices[slot].file_base);
            g_voices[slot].file_base = NULL;
        }
        if (g_voices[slot].is_streaming) {
            if (g_voices[slot].pcm_ptr) kfree(g_voices[slot].pcm_ptr);
            if (g_voices[slot].mp3_chunk) kfree(g_voices[slot].mp3_chunk);
            g_voices[slot].pcm_ptr = NULL;
            g_voices[slot].mp3_chunk = NULL;
        }
    }
    sti();
}

void ac97_stop_all(void) {
    cli();
    for (int i = 0; i < MAX_VOICES; i++) {
        if (g_voices[i].active) {
            g_voices[i].active = FALSE;
            if (g_voices[i].file_base) {
                kfree(g_voices[i].file_base);
                g_voices[i].file_base = NULL;
            }
            if (g_voices[i].is_streaming) {
                if (g_voices[i].pcm_ptr) kfree(g_voices[i].pcm_ptr);
                if (g_voices[i].mp3_chunk) kfree(g_voices[i].mp3_chunk);
                g_voices[i].pcm_ptr = NULL;
                g_voices[i].mp3_chunk = NULL;
            }
        }
    }
    g_active_voices = 0;
    sti();
}

BOOL ac97_get_voice_progress(int handle, uint32* current, uint32* total) {
    if (handle < 0) return FALSE;
    int slot = handle & 0xFF;
    uint32 gen = handle >> 8;
    if (slot >= MAX_VOICES) return FALSE;

    BOOL ret = FALSE;
    cli();
    if (g_voices[slot].active && g_voices[slot].generation == gen) {
        if (current) *current = g_voices[slot].current_sample;
        if (total) *total = g_voices[slot].total_samples;
        ret = TRUE;
    }
    sti();
    return ret;
}

BOOL ac97_is_playing(void) {
    return g_active_voices > 0;
}