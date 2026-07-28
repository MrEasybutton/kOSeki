#ifndef PMM_H
#define PMM_H

#include "types.h"

typedef uint32 PMM_PHYSICAL_ADDRESS;

#define PMM_BLOCK_SIZE 4096

inline void cpu_cpuid(int CPUInfo[4], int InfoType) {
#if defined(__PIC__)
    __asm__ __volatile__(
#if defined(__x86_64__)
        "push %%rbx\n"
        "cpuid\n"
        "xchg %%rbx, %1\n"
        "pop %%rbx\n"
        : "=a" (CPUInfo[0]), "=r" (CPUInfo[1]), "=c" (CPUInfo[2]), "=d" (CPUInfo[3])
        : "a" (InfoType)
#else
        "xchgl %%ebx, %1\n"
        "cpuid\n"
        "xchgl %%ebx, %1\n"
        : "=a" (CPUInfo[0]), "=r" (CPUInfo[1]), "=c" (CPUInfo[2]), "=d" (CPUInfo[3])
        : "a" (InfoType)
#endif
    );
#else
    __asm__ __volatile__(
        "cpuid"
        : "=a" (CPUInfo[0]), "=b" (CPUInfo[1]), "=c" (CPUInfo[2]), "=d" (CPUInfo[3])
        : "a" (InfoType)
    );
#endif
}

typedef struct {
    uint32 memory_size;
    uint32 max_blocks;
    uint32 *memory_map_array;
    uint32 memory_map_end;
    uint32 used_blocks;
} PMM_INFO;


uint32 pmm_get_max_blocks();
uint32 pmm_get_used_blocks();

int pmm_next_free_frame(int size);

void pmm_init(PMM_PHYSICAL_ADDRESS bitmap, uint32 total_memory_size);

void pmm_init_region(PMM_PHYSICAL_ADDRESS base, uint32 region_size);
void pmm_deinit_region(PMM_PHYSICAL_ADDRESS base, uint32 region_size);

void* pmm_alloc_block();
void pmm_free_block(void* p);
void* pmm_alloc_blocks(uint32 size);
void pmm_free_blocks(void* p, uint32 size);

#endif
