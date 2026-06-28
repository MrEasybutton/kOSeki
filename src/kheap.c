#include "kheap.h"
#include "console.h"
#include "kernel.h"
#include "pmm.h"
#include "string.h"
#include "serial.h"
#include "utils.h"

#define INT_MAX ((int)((unsigned int)~0 >> 1))
#define MIN_SPLIT 64

void *g_kheap_start_addr = (void*)0;
void *g_kheap_end_addr = (void*)0;
uint64 g_total_size = 0;
uint64 g_total_used_size = 0;

KHEAP_BLOCK *g_head = (KHEAP_BLOCK*)0;

int k_init(void *start_addr, void *end_addr) {
    kprint("[KHEAP] build ts: %s %s\n", __DATE__, __TIME__);
    if (start_addr > end_addr) {
        printf("failed to init kheap\n");
        return -1;
    }
    
    // if (g_head != NULL) kprint("[KHEAP] WARNING: g_head is 0x%x (not NULL) before init.\n", (uint32)g_head);
    
    kprint("[KHEAP] Initializing heap from 0x%x to 0x%x\n", (uint32)start_addr, (uint32)end_addr);
    g_kheap_start_addr = start_addr;
    g_kheap_end_addr = end_addr;
    g_total_size = (uint32)end_addr - (uint32)start_addr;
    g_total_used_size = 0;

    g_head = NULL;
    
    kprint("[KHEAP] Initializing heap from 0x%x to 0x%x (%d MB)\n",
                  (uint32)start_addr, (uint32)end_addr, 
                  (uint32)(g_total_size / (1024 * 1024)));
    
    return 0;
}

void *kbrk(int size) {
    void *addr = NULL;
    if (size < 0)
        return NULL;
    
    if (g_total_used_size + (uint64)size > (uint64)g_total_size) {
        kprint("[KBRK] Out of memory > requested: %d, used: %d, total: %d\n",
                      size, (uint32)g_total_used_size, (uint32)g_total_size);
        return NULL;
    }
    
    uint8 *heap = (uint8 *)g_kheap_start_addr;
    addr = heap + g_total_used_size;

    g_total_used_size += size;
    return addr;
}

void pallocs() {
    KHEAP_BLOCK *temp = g_head;
    printf("block size: %d\n", sizeof(KHEAP_BLOCK));
    while (temp != NULL) {
        printf("size:%d, free:%d, data: 0x%x, curr: 0x%x, next: 0x%x\n",
               temp->metadata.size, temp->metadata.is_free, temp->data, temp, temp->next);
        temp = temp->next;
    }
}

BOOL is_block_free(KHEAP_BLOCK *block) {
    if (!block)
        return FALSE;
    return (block->metadata.is_free == TRUE);
}

static inline BOOL is_block_valid(KHEAP_BLOCK *block) {
    if (!block) return FALSE;
    
    if ((void*)block < g_kheap_start_addr || (void*)block >= g_kheap_end_addr) return FALSE;
    
    if (block->metadata.magic != KHEAP_MAGIC) return FALSE;
    
    void *expected_data = (void*)((uint8*)block + sizeof(KHEAP_BLOCK));
    if (block->data != expected_data) {
        return FALSE;
    }
    
    return TRUE;
}

KHEAP_BLOCK *find_free_block(int size) {
    KHEAP_BLOCK *temp = g_head;
    KHEAP_BLOCK *best_fit_block = NULL;
    uint32 min_diff = (uint32)-1;

    while (temp != NULL) {
        if (is_block_valid(temp) && temp->metadata.is_free) {
            if ((int)temp->metadata.size >= size) {
                uint32 diff = temp->metadata.size - size;
                if (diff < min_diff) {
                    min_diff = diff;
                    best_fit_block = temp;
                }
            }
        }
        temp = temp->next;
    }
    return best_fit_block;
}

void *kmalloc(int size) {
    cli();
    if (size <= 0) {
        sti();
        return NULL;
    }

    size = (size + 15) & ~15;
    uint64 total_needed = sizeof(KHEAP_BLOCK) + (uint64)size;

    if (g_head == NULL) {
        if (g_total_used_size + total_needed > g_total_size) {
            sti();
            return NULL;
        }
        
        g_head = (KHEAP_BLOCK *)kbrk(total_needed);
        if (!g_head) {
            sti();
            return NULL;
        }
        
        g_head->metadata.magic = KHEAP_MAGIC;
        g_head->metadata.size = size;
        g_head->metadata.is_free = FALSE;
        g_head->next = NULL;
        g_head->prev = NULL;
        g_head->data = (void*)((uint8*)g_head + sizeof(KHEAP_BLOCK));
        
        sti();
        return g_head->data;
    }

    KHEAP_BLOCK *free_block = find_free_block(size);
    if (free_block != NULL) {
        if (free_block->metadata.size >= size + sizeof(KHEAP_BLOCK) + MIN_SPLIT) {
            uint8 *new_block_addr = (uint8 *)free_block->data + size;
            KHEAP_BLOCK *new_block = (KHEAP_BLOCK *)new_block_addr;
            
            new_block->metadata.magic = KHEAP_MAGIC;
            new_block->data = (void*)((uint8*)new_block + sizeof(KHEAP_BLOCK));
            new_block->metadata.size = free_block->metadata.size - size - sizeof(KHEAP_BLOCK);
            new_block->metadata.is_free = TRUE;
            new_block->next = free_block->next;
            new_block->prev = free_block;
            
            free_block->next = new_block;
            free_block->metadata.size = size;
        }
        
        free_block->metadata.is_free = FALSE;
        sti();
        return free_block->data;
    }

    if (g_total_used_size + total_needed > g_total_size) {
        sti();
        return NULL;
    }

    KHEAP_BLOCK *temp = g_head;
    while (temp->next != NULL) {
        temp = temp->next;
    }

    KHEAP_BLOCK *new_block = (KHEAP_BLOCK *)kbrk(total_needed);
    if (!new_block) {
        sti();
        return NULL;
    }
    
    new_block->metadata.magic = KHEAP_MAGIC;
    new_block->metadata.size = size;
    new_block->metadata.is_free = FALSE;
    new_block->next = NULL;
    new_block->prev = temp;
    new_block->data = (void*)((uint8*)new_block + sizeof(KHEAP_BLOCK));
    
    temp->next = new_block;
    
    sti();
    return new_block->data;
}

void *kcalloc(int n, int size) {
    if (n != 0 && size > INT_MAX / n) return NULL;
    void *mem = kmalloc(n * size);
    if (mem) {
        memset(mem, 0, n * size);
    }
    return mem;
}

void *krealloc(void *ptr, int size) {
    if (!ptr) return kmalloc(size);
    if (size <= 0) {
        kfree(ptr);
        return NULL;
    }

    KHEAP_BLOCK *block = (KHEAP_BLOCK *)((uint8*)ptr - sizeof(KHEAP_BLOCK));
    if (!is_block_valid(block) || block->data != ptr) {
        kprint("!CRITICAL! invalid/corrupted block at 0x%x\n", (uint32)ptr);
        return NULL;
    }

    if ((int)block->metadata.size >= size) return ptr;

    void *new_ptr = kmalloc(size);
    if (!new_ptr) return NULL;

    int old_size = block->metadata.size;
    memcpy(new_ptr, ptr, old_size);

    kfree(ptr);

    return new_ptr;
}

void kfree(void *addr) {
    if (!addr) return;
    
    cli();

    KHEAP_BLOCK *block_to_free = (KHEAP_BLOCK *)((uint8*)addr - sizeof(KHEAP_BLOCK));
    
    if (!is_block_valid(block_to_free) || block_to_free->data != addr) {
        kprint("[KFREE] !CRITICAL! Invalid or corrupted block at 0x%x\n", (uint32)addr);
        sti();
        return;
    }

    if (block_to_free->metadata.is_free) {
        kprint("[KFREE] !CRITICAL! Double free at 0x%x\n", (uint32)addr);
        sti();
        return;
    }

    block_to_free->metadata.is_free = TRUE;

    // coalesce if valid free, O(1)
    if (block_to_free->next && is_block_valid(block_to_free->next) && 
        block_to_free->next->metadata.is_free) {
        // iscontig
        uint8 *expected_next = (uint8*)block_to_free + sizeof(KHEAP_BLOCK) + block_to_free->metadata.size;
        if ((uint8*)block_to_free->next == expected_next) {
            KHEAP_BLOCK *next = block_to_free->next;
            block_to_free->metadata.size += sizeof(KHEAP_BLOCK) + next->metadata.size;
            block_to_free->next = next->next;
            if (next->next) next->next->prev = block_to_free;

        }
    }

    // attempt coalesce, O(1)
    KHEAP_BLOCK *prev = block_to_free->prev;

    if (prev && is_block_valid(prev) && prev->metadata.is_free) {
        uint8 *expected_current = (uint8*)prev + sizeof(KHEAP_BLOCK) + prev->metadata.size;
        if ((uint8*)block_to_free == expected_current) {
            prev->metadata.size += sizeof(KHEAP_BLOCK) + block_to_free->metadata.size;
            prev->next = block_to_free->next;
            if (block_to_free->next)
                block_to_free->next->prev = prev;
        }
    }
    sti();
}

void kvalid() {
    KHEAP_BLOCK *temp = g_head;
    int block_count = 0;
    int corrupted_blocks = 0;
    uint64 total_allocated = 0;
    uint64 total_free = 0;
    int free_blocks = 0;
    int allocated_blocks = 0;
    
    while (temp != NULL) {
        block_count++;
        
        if (block_count > 10000) {
            kprint("circular\n");
            return;
        }
        
        if (!is_block_valid(temp)) {
            corrupted_blocks++;
            kprint("[KHEAP] skipping corrupted block 0x%x\n", (uint32)temp);
            temp = temp->next;
            continue;
        }
        
        if (temp->metadata.is_free) {
            total_free += temp->metadata.size;
            free_blocks++;
        } else {
            total_allocated += temp->metadata.size;
            allocated_blocks++;
        }
        
        temp = temp->next;
    }
    
    kprint(">> kheap report >>\n");
    kprint("BLOCKS: %d (allocated: %d, free: %d, corrupted: %d)\n", 
                  block_count, allocated_blocks, free_blocks, corrupted_blocks);
    kprint("allocated %d bytes\n", (uint32)total_allocated);
    kprint("free mem of %d bytes\n", (uint32)total_free);
    kprint("committed %d bytes\n", (uint32)g_total_used_size);
    kprint("%d bytes total\n", (uint32)g_total_size);
    kprint("%d bytes is available\n", (uint32)(g_total_size - g_total_used_size));
    
    if (corrupted_blocks > 0) kprint("[KHEAP] %d corrupted\n", corrupted_blocks);
    if (free_blocks > 10) kprint("[KHEAP] heap is highly fragmented (%d free blocks)\n", free_blocks);
}