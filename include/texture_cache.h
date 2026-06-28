#ifndef TEXTURE_CACHE_H
#define TEXTURE_CACHE_H

#include "bmp.h"
#include "types.h"

#define MAX_TEX 50

typedef struct {
    char* filename;
    Bitmap* bitmap;
    int ref_count;
    BOOL is_occ;
} TexCacheEntry;

void texcache_init(void);

// try cache, ref_count++
Bitmap* texcache_get(const char* filename);

// free if none
void texcache_rel(const char* filename);
void texcache_clear(void);

void texcache_statget(void);

BOOL texcache_iscache(const char* filename);

#endif