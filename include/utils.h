#ifndef UTILS_H
#define UTILS_H
#include "types.h"

typedef struct {
    char* pixel_data;
    int width;
    int height;
    int row_padded;
} preloaded_t;

void panic(char *message);

preloaded_t* preload_bmp(const char* filename);
void free_preloaded_bmp(preloaded_t* bmp);

static inline uint16 htons(uint16 v) { return (uint16)((v << 8) | (v >> 8)); }
static inline uint16 ntohs(uint16 v) { return htons(v); }

static inline uint32 htonl(uint32 v) {
    return ((v << 24) & 0xFF000000) |
           ((v <<  8) & 0x00FF0000) |
           ((v >>  8) & 0x0000FF00) |
           ((v >> 24) & 0x000000FF);
}

static inline uint32 ntohl(uint32 v) { return htonl(v); }

static inline void cli() {
    __asm__ volatile("cli");
}

static inline void sti() {
    __asm__ volatile("sti");
}

#endif