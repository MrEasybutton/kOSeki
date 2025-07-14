#include "filesystem.h"
#include <stdint.h>

uint8_t sector_buffer[512];

#define DISK_SIZE (128 * 512)
static uint8_t disk[DISK_SIZE];
static int disk_initialized = 0;

int disk_init(void) {
    if (disk_initialized) return 0;

    for (int i = 0; i < DISK_SIZE; i++) {
        disk[i] = 0;
    }

    FAT_BPB* bpb = (FAT_BPB*)disk;
    
    bpb->jump_boot[0] = 0xEB;
    bpb->jump_boot[1] = 0x3C;
    bpb->jump_boot[2] = 0x90;
    
    const char* oem = "KOSEKI16";
    for (int i = 0; i < 8; i++) {
        bpb->oem_name[i] = oem[i];
    }
    
    bpb->bytes_per_sector = 512;
    bpb->sectors_per_cluster = 1;
    bpb->reserved_sectors = 1;
    bpb->fat_count = 2;
    bpb->root_entry_count = 512;
    bpb->total_sectors_16 = DISK_SIZE / 512;
    bpb->media_type = 0xF8;
    
    bpb->sectors_per_fat = 1;
    
    bpb->sectors_per_track = 63;
    bpb->head_count = 16;
    bpb->hidden_sectors = 0;
    bpb->total_sectors_32 = 0;
    
    bpb->drive_number = 0x80;
    bpb->reserved = 0;
    bpb->boot_signature = 0x29;
    bpb->volume_id = 0x12345678;
    
    const char* label = "KOSEKI VOL ";
    for (int i = 0; i < 11; i++) {
        bpb->volume_label[i] = label[i];
    }
    
    const char* fs_type = "FAT16   ";
    for (int i = 0; i < 8; i++) {
        bpb->fs_type[i] = fs_type[i];
    }
    
    disk[510] = 0x55;
    disk[511] = 0xAA;
    
    uint16_t* fat = (uint16_t*)(disk + 512);
    fat[0] = 0xFFF8;
    fat[1] = 0xFFFF;
    
    uint16_t* fat2 = (uint16_t*)(disk + 512*2);
    fat2[0] = 0xFFF8;
    fat2[1] = 0xFFFF;

    disk_initialized = 1;
    return 0;
}

int disk_read_sector(uint32_t sector, uint8_t* buffer) {
    if (!disk_initialized) {
        disk_init();
    }
    
    if (sector >= DISK_SIZE / 512) return -1;
    
    for (int i = 0; i < 512; i++) {
        buffer[i] = disk[sector * 512 + i];
    }
    
    return 0;
}

int disk_write_sector(uint32_t sector, const uint8_t* buffer) {

    if (!disk_initialized) disk_init();
    if (sector >= DISK_SIZE / 512) return -1;
    
    for (int i = 0; i < 512; i++) {
        disk[sector * 512 + i] = buffer[i];
    }
    
    return 0;
}