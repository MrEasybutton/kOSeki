#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stdint.h>

typedef struct {
    uint8_t jump_boot[3];
    uint8_t   oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entry_count;
    uint16_t total_sectors_16;
    uint8_t media_type;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint8_t drive_number;
    uint8_t reserved;
    uint8_t boot_signature;
    uint32_t volume_id;
    uint8_t volume_label[11];
    uint8_t fs_type[8];
} __attribute__((packed)) FAT_BPB;

typedef struct {
    uint8_t filename[8];
    uint8_t extension[3];
    uint8_t attributes;
    uint8_t reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t starting_cluster;
    uint32_t file_size;
} __attribute__((packed)) FAT_DirectoryEntry;

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LONG_NAME 0x0F

#define MAX_OPEN_FILES 8

typedef struct {
    uint8_t in_use;
    uint8_t filename[13];
    uint16_t current_cluster;
    uint32_t position;
    uint32_t size;
    uint8_t mode;
    uint16_t directory_entry_pos;
} FileDescriptor;

#define FS_MODE_READ 0x01
#define FS_MODE_WRITE 0x02
#define FS_MODE_APPEND 0x04
#define FS_MODE_CREATE 0x08

extern uint8_t sector_buffer[512];

int fs_init(void);
int fs_mount(void);
int fs_open(const char* filename, uint8_t mode);
int fs_read(int fd, void* buffer, uint32_t size);
int fs_write(int fd, const void* buffer, uint32_t size);
int fs_close(int fd);
int fs_seek(int fd, uint32_t position);
int fs_tell(int fd);
int fs_delete(const char* filename);
int fs_create_directory(const char* dirname);

int disk_read_sector(uint32_t sector, uint8_t* buffer);
int disk_write_sector(uint32_t sector, const uint8_t* buffer);

#endif
