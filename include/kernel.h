#ifndef KERNEL_H
#define KERNEL_H

#include "types.h"

extern uint8 __kernel_section_start;
extern uint8 __kernel_section_end;
extern uint8 __kernel_text_section_start;
extern uint8 __kernel_text_section_end;
extern uint8 __kernel_data_section_start;
extern uint8 __kernel_data_section_end;
extern uint8 __kernel_rodata_section_start;
extern uint8 __kernel_rodata_section_end;
extern uint8 __kernel_bss_section_start;
extern uint8 __kernel_bss_section_end;


typedef struct {
    struct {
        uint32 k_start_addr;
        uint32 k_end_addr;
        uint32 k_len;
        uint32 text_start_addr;
        uint32 text_end_addr;
        uint32 text_len;
        uint32 data_start_addr;
        uint32 data_end_addr;
        uint32 data_len;
        uint32 rodata_start_addr;
        uint32 rodata_end_addr;
        uint32 rodata_len;
        uint32 bss_start_addr;
        uint32 bss_end_addr;
        uint32 bss_len;
    } kernel;

    struct {
        uint32 total_memory;
    } system;

    struct {
        uint32 start_addr;
        uint32 end_addr;
        uint32 size;
    } available;
} KERNEL_MEMORY_MAP;

extern KERNEL_MEMORY_MAP g_kmap;

extern uint8 g_video_mode;

typedef struct {
    char* buf;
    int* len;
} pbshPrint;

static pbshPrint g_pbsh_print;

void pbsh_buf_print(char* pbsh_buf, int* pbsh_len, const char* str);
static void pbsh_bx2_print(const char* text) { pbsh_buf_print(g_pbsh_print.buf, g_pbsh_print.len, text); }

typedef void (*IconAction)(void);

typedef struct {
    const char* bmp;
    IconAction action;
} IconDef;

void init_txt();
void init_gui();
void trim_pbsh_buf(char* buf, int* term_len);
void wllp_rendcache(void);
void sys_cleanup();

void set_wallpaper(const char* path);

#endif

