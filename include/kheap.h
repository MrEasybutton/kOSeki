#ifndef KHEAP_H
#define KHEAP_H

#include "types.h"

#define KHEAP_MAGIC 0xB1B00144

typedef struct _kheap_block {
    struct {
        uint32 magic;
        uint32 size;
        uint8 is_free;
    } metadata;
    struct _kheap_block *next;
    struct _kheap_block *prev;
    void *data;
} __attribute__((packed)) KHEAP_BLOCK;

extern void *g_kheap_start_addr;
extern void *g_kheap_end_addr;

int k_init(void *start_addr, void *end_addr);

void *kbrk(int size);
void pallocs();

void *kmalloc(int size);
void *kcalloc(int n, int size);
void *krealloc(void *ptr, int size);

void kfree(void *addr);
void kvalid();

#endif
