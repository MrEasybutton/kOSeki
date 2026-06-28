#ifndef FAT32_H
#define FAT32_H

#include "types.h"

typedef struct fat_bpb {
	unsigned char bootjmp[3];
	unsigned char oem_name[8];
	unsigned short bytes_per_sector;
	unsigned char sectors_per_cluster;
	unsigned short reserved_sector_count;
	unsigned char table_count;
	unsigned short root_entry_count;
	unsigned short total_sectors_16;
	unsigned char media_type;
	unsigned short table_size_16;
	unsigned short sectors_per_track;
	unsigned short head_side_count;
	unsigned int hidden_sector_count;
	unsigned int total_sectors_32;

	unsigned int table_size_32;
	unsigned short extended_flags;
	unsigned short fat_version;
	unsigned int root_cluster;
	unsigned short fat_info;
	unsigned short backup_BS_sector;
	unsigned char reserved_0[12];
	unsigned char drive_number;
	unsigned char reserved_1;
	unsigned char boot_signature;
	unsigned int volume_id;
	unsigned char volume_label[11];
	unsigned char fat_type_label[8];
 
}__attribute__((packed)) fat_bpb;

typedef struct {
    uint8 Name[11];
    uint8 Attributes;
    uint8 Reserved;
    uint8 CreatedTimeTenths;
    uint16 CreatedTime;
    uint16 CreatedDate;
    uint16 AccessedDate;
    uint16 FirstClusterHigh;
    uint16 ModifiedTime;
    uint16 ModifiedDate;
    uint16 FirstClusterLow;
    uint32 Size;
} __attribute__((packed)) FAT_dirent;

typedef struct {
    uint8 SequenceNumber;
    uint16 NamePart1[5];
    uint8 Attributes; // 0x0F
    uint8 Type; // 0x00
    uint8 Checksum;
    uint16 NamePart2[6];
    uint16 FirstClusterLow; // 0x0000
    uint16 NamePart3[2];
} __attribute__((packed)) FAT_LFN_entry;

#define MAX_FILENAME_LEN 128

typedef struct {
    char name[MAX_FILENAME_LEN];
    uint32 cluster;
    uint32 size;
    uint8 attributes;
} FAT_FileEntry;

typedef struct {
    FAT_FileEntry* entries;
    uint32 count;
} FAT_DirList;

typedef struct fat_file
{
	BOOL is_dir;
	uint32 position;
	uint32 size;
} fat_file;

void fat_init();
char* fat_read_file(char* fpath);
int fat_read_file_offset(uint32 start_cluster, uint32 offset, uint32 size, void* buffer);
uint8* load_cluster(int clu);
void ide_read_cluster_into_buffer(int clu, uint8* buffer);
FAT_dirent* fat_find_file(const char* path);
void fat_dir_ls(uint32 cluster);
char* get_ls(const char* path);
FAT_dirent* fat_find_in_dir(char* fname, int cluster);
uint64 fat_get_free_space();
FAT_DirList* fat_get_dir_ls(uint32 cluster);
int fat_create_file(const char* path);
int fat_rename_file(const char* old_path, const char* new_path);
int fat_append_file(const char* path, const char* data, uint32 size);
int fat_delete_file(const char* path);
int fat_write_file(const char* path, const char* data, uint32 size);
int fat_write_file_call(const char* path, uint32 size, void* ctx, int (*fill_chunk)(void* ctx, uint32 offset, uint32 size, char* buffer));
int fat_create_dir(const char* path);

#endif