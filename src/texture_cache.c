#include "texture_cache.h"
#include "string.h"
#include "console.h"
#include "serial.h"
#include "kheap.h"

static TexCacheEntry g_texture_cache[MAX_TEX];

void texcache_init(void) {
    memset(g_texture_cache, 0, sizeof(g_texture_cache));
    kprint("texcache init done\n");
}

Bitmap* texcache_get(const char* filename) {
    kvalid();
    if (!filename) {
        return NULL;
    }
    
    for (int i = 0; i < MAX_TEX; i++) {
        if (g_texture_cache[i].is_occ && 
            g_texture_cache[i].filename &&
            strcmp(g_texture_cache[i].filename, filename) == 0) {
            
            g_texture_cache[i].ref_count++;
            kprint("[TEXCACHE] Cache hit for %s (refs: %d)\n", 
                          filename, g_texture_cache[i].ref_count);
            return g_texture_cache[i].bitmap;
        }
    }
    
    // load if not cached
    kprint("[TEXCACHE] Cache miss for %s, loading...\n", filename);
    Bitmap* bmp = load_bmp((char*)filename);
    kvalid();
    if (!bmp) {
        kprint("[TEXCACHE] Failed to load %s\n", filename);
        return NULL;
    }
    
    for (int i = 0; i < MAX_TEX; i++) {
        if (!g_texture_cache[i].is_occ) {
            int len = strlen(filename);
            g_texture_cache[i].filename = (char*)kmalloc(len + 1);
            kvalid();
            if (!g_texture_cache[i].filename) {
                kprint("[TEXCACHE] Out of memory, can't cache %s\n", filename);
                return bmp;
            }
            
            strcpy(g_texture_cache[i].filename, filename);
            g_texture_cache[i].bitmap = bmp;
            g_texture_cache[i].ref_count = 1;
            g_texture_cache[i].is_occ = TRUE;
            
            kprint("[TEXCACHE] Cached %s in slot %d\n", filename, i);
            return bmp;
        }
    }
    
    //fallback, caller must free
    kprint("[TEXCACHE] Cache full, returning the uncached %s\n", filename);
    return bmp;
}

void texcache_rel(const char* filename) {
    kvalid();
    if (!filename) {
        return;
    }
    
    for (int i = 0; i < MAX_TEX; i++) {
        if (g_texture_cache[i].is_occ &&
            g_texture_cache[i].filename &&
            strcmp(g_texture_cache[i].filename, filename) == 0) {
            
            g_texture_cache[i].ref_count--;
            kprint("[TEXCACHE] Released %s (refs: %d)\n", 
                          filename, g_texture_cache[i].ref_count);
            
            if (g_texture_cache[i].ref_count <= 0) {
                kprint("[TEXCACHE] Freeing %s from cache\n", filename);
                
                if (g_texture_cache[i].bitmap) {
                    if ((void*)g_texture_cache[i].bitmap < g_kheap_start_addr || (void*)g_texture_cache[i].bitmap > g_kheap_end_addr) {
                        kprint("[TEXCACHE] Invalid bitmap pointer for %s\n", filename);
                    } else {
                        free_bmp(g_texture_cache[i].bitmap);
                    }
                }
                if (g_texture_cache[i].filename) {
                    kfree(g_texture_cache[i].filename);
                }
                
                g_texture_cache[i].bitmap = NULL;
                g_texture_cache[i].filename = NULL;
                g_texture_cache[i].ref_count = 0;
                g_texture_cache[i].is_occ = FALSE;
            }
            return;
        }
    }
    
    kprint("[TEXCACHE] Warning - tried to release %s but it's not in cache\n", filename);
}

void texcache_clear(void) {
    kprint("[TEXCACHE] Clearing all cached textures\n");
    
    for (int i = 0; i < MAX_TEX; i++) {
        if (g_texture_cache[i].is_occ) {
            if (g_texture_cache[i].bitmap) {
                free_bmp(g_texture_cache[i].bitmap);
            }
            if (g_texture_cache[i].filename) {
                kfree(g_texture_cache[i].filename);
            }
            
            g_texture_cache[i].bitmap = NULL;
            g_texture_cache[i].filename = NULL;
            g_texture_cache[i].ref_count = 0;
            g_texture_cache[i].is_occ = FALSE;
        }
    }
}

BOOL texcache_iscache(const char* filename) {
    if (!filename) {
        return FALSE;
    }
    
    for (int i = 0; i < MAX_TEX; i++) {
        if (g_texture_cache[i].is_occ &&
            g_texture_cache[i].filename &&
            strcmp(g_texture_cache[i].filename, filename) == 0) {
            return TRUE;
        }
    }
    
    return FALSE;
}

void texcache_statget(void) {
    int used_slots = 0;
    int total_refs = 0;
    
    kprint(">> texcache report >>\n");
    
    for (int i = 0; i < MAX_TEX; i++) {
        if (g_texture_cache[i].is_occ) {
            used_slots++;
            total_refs += g_texture_cache[i].ref_count;
            printf("[%d] %s (refs: %d, %dx%d)\n", i,
                    g_texture_cache[i].filename,
                    g_texture_cache[i].ref_count,
                    g_texture_cache[i].bitmap ? g_texture_cache[i].bitmap->width : 0,
                    g_texture_cache[i].bitmap ? g_texture_cache[i].bitmap->height : 0);
        }
    }
    
    printf("Total cached: %d/%d slots\n", used_slots, MAX_TEX);
    printf("Total references: %d\n", total_refs);
}