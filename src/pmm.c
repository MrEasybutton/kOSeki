#include "pmm.h"
#include "string.h"
#include "console.h"
#include "multiboot.h"

static PMM_INFO g_pmm_info;

// set bit in memory map array
static inline void pmm_mmap_set(int bit) {
    g_pmm_info.memory_map_array[bit / 32] |= (1 << (bit % 32));
}

// unset bit in memory map array
static inline void pmm_mmap_unset(int bit) {
    g_pmm_info.memory_map_array[bit / 32] &= ~(1 << (bit % 32));
}

// test if given nth bit is set
static inline char pmm_mmap_test(int bit) {
    return g_pmm_info.memory_map_array[bit / 32] & (1 << (bit % 32));
}

uint32 pmm_get_max_blocks() {
    return g_pmm_info.max_blocks;
}

uint32 pmm_get_used_blocks() {
    return g_pmm_info.used_blocks;
}

// find first free frame in bitmap array and return its index
int pmm_mmap_first_free() {
    uint32 i, j;

    // find the first free bit
    for (i = 0; i < (g_pmm_info.max_blocks + 31) / 32; i++) {
        if (g_pmm_info.memory_map_array[i] != 0xffffffff) {
            // check each bit, if not set
            for (j = 0; j < 32; j++) {
                int bit = 1 << j;
                if (!(g_pmm_info.memory_map_array[i] & bit)) {
                    int frame = i * 32 + j;
                    if (frame < g_pmm_info.max_blocks)
                        return frame;
                }
            }
        }
    }
    return -1;
}

// find first free number of frames(size) and return its index
int pmm_mmap_first_free_by_size(uint32 size) {
    uint32 count = 0;
    uint32 start_frame = 0;

    if (size == 0) return -1;
    if (size == 1) return pmm_mmap_first_free();

    for (uint32 i = 0; i < g_pmm_info.max_blocks; i++) {
        if (!pmm_mmap_test(i)) {
            if (count == 0) start_frame = i;
            count++;
            if (count == size) return start_frame;
        } else {
            count = 0;
        }
    }
    return -1;
}

/**
 * returns index of bitmap array if no of bits(size) are free
 */
int pmm_next_free_frame(int size) {
    return pmm_mmap_first_free_by_size(size);
}

/**
 * initialize memory bitmap array by making blocks from total memory size
 */
void pmm_init(PMM_PHYSICAL_ADDRESS bitmap, uint32 total_memory_size) {
    g_pmm_info.memory_size = total_memory_size;
    g_pmm_info.memory_map_array = (uint32 *)bitmap;
    // Remember - memory_size is in bytes
    g_pmm_info.max_blocks = total_memory_size / PMM_BLOCK_SIZE;
    g_pmm_info.used_blocks = g_pmm_info.max_blocks;
    
    // Size of bitmap in bytes: 1 bit per block
    uint32 bitmap_size = (g_pmm_info.max_blocks + 7) / 8;
    // set all of memory is in use
    memset(g_pmm_info.memory_map_array, 0xff, bitmap_size);
    
    // ending address of map array for starting point of regions
    // Align to block size to keep things clean
    g_pmm_info.memory_map_end = (bitmap + bitmap_size + PMM_BLOCK_SIZE - 1) & ~(PMM_BLOCK_SIZE - 1);
}

/**
 * initialize/request for a free region of region_size from pmm
 */
void pmm_init_region(PMM_PHYSICAL_ADDRESS base, uint32 region_size) {
    if (base < g_pmm_info.memory_map_end) {
        // If the region starts before our managed area (e.g. it's the area containing the bitmap)
        // we only free the part that is actually managed.
        if (base + region_size <= g_pmm_info.memory_map_end) return;
        region_size -= (g_pmm_info.memory_map_end - base);
        base = g_pmm_info.memory_map_end;
    }

    int align = (base - g_pmm_info.memory_map_end) / PMM_BLOCK_SIZE;
    int blocks = region_size / PMM_BLOCK_SIZE;

    // make free the blocks associated with given address base to mark that region as free
    for (int i = 0; i < blocks; i++) {
        if (align + i >= g_pmm_info.max_blocks) break;
        // unset bit to make it free
        pmm_mmap_unset(align + i);
        // reduce used blocks count
        g_pmm_info.used_blocks--;
    }
}

/**
 * de-initialize/free allocated region of region_size from pmm
 */
void pmm_deinit_region(PMM_PHYSICAL_ADDRESS base, uint32 region_size) {
    if (base < g_pmm_info.memory_map_end) {
        if (base + region_size <= g_pmm_info.memory_map_end) return;
        region_size -= (g_pmm_info.memory_map_end - base);
        base = g_pmm_info.memory_map_end;
    }

    int align = (base - g_pmm_info.memory_map_end) / PMM_BLOCK_SIZE;
    int blocks = region_size / PMM_BLOCK_SIZE;

    for (int i = 0; i < blocks; i++) {
        if (align + i >= g_pmm_info.max_blocks) break;
        // set block bit
        pmm_mmap_set(align + i);
        // increase used blocks count
        g_pmm_info.used_blocks++;
    }
}

/**
 * request to allocate a single block of memory from pmm
 */
void* pmm_alloc_block() {
    // out of memory
    if ((g_pmm_info.max_blocks - g_pmm_info.used_blocks) < 1)
        return NULL;

    int frame = pmm_mmap_first_free();
    if (frame == -1)
        return NULL;

    pmm_mmap_set(frame);

    // get actual address by skipping memory map
    PMM_PHYSICAL_ADDRESS addr = (frame * PMM_BLOCK_SIZE) + g_pmm_info.memory_map_end;
    g_pmm_info.used_blocks++;

    return (void *)addr;
}

/**
 * free given requested single block of memory from pmm
 */
void pmm_free_block(void* p) {
    PMM_PHYSICAL_ADDRESS addr = (PMM_PHYSICAL_ADDRESS)p;
    // go to the bitmap array address
    addr -= g_pmm_info.memory_map_end;
    int frame = addr / PMM_BLOCK_SIZE;
    pmm_mmap_unset(frame);
    g_pmm_info.used_blocks--;
}

/**
 * request to allocate no of blocks of memory from pmm
 */
void* pmm_alloc_blocks(uint32 size) {
    uint32 i;

    // out of memory
    if ((g_pmm_info.max_blocks - g_pmm_info.used_blocks) < size)
        return NULL;

    int frame = pmm_mmap_first_free_by_size(size);
    if (frame == -1)
        return NULL;

    // set bits in memory map
    for (i = 0; i < size; i++)
        pmm_mmap_set(frame + i);

    PMM_PHYSICAL_ADDRESS addr = (frame * PMM_BLOCK_SIZE) + g_pmm_info.memory_map_end;
    g_pmm_info.used_blocks += size;

    return (void *)addr;
}

/**
 * free given requested no of blocks of memory from pmm
 */
void pmm_free_blocks(void* p, uint32 size) {
    uint32 i;

    PMM_PHYSICAL_ADDRESS addr = (PMM_PHYSICAL_ADDRESS)p;
    // go to the bitmap array address
    addr -= g_pmm_info.memory_map_end;
    int frame = addr / PMM_BLOCK_SIZE;
    for (i = 0; i < size; i++)
        pmm_mmap_unset(frame + i);
    g_pmm_info.used_blocks -= size;
}
