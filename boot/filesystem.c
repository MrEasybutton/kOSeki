#include "filesystem.h"
#include <stdint.h>
#include <stdbool.h>

static FileDescriptor file_descriptors[MAX_OPEN_FILES];

static uint32_t fat_start_sector;
static uint32_t root_dir_start_sector;
static uint32_t data_start_sector;
static uint16_t root_dir_entries;
static uint8_t sectors_per_cluster;
static FAT_BPB bpb;

int fs_init(void) {
    for (int i = 0; i < MAX_OPEN_FILES; i++) file_descriptors[i].in_use = 0;

    disk_init();
    return 0;
}

int fs_mount(void) {

    if (disk_read_sector(0, sector_buffer) != 0) return -1;
    for (int i = 0; i < sizeof(FAT_BPB); i++) ((uint8_t*)&bpb)[i] = sector_buffer[i];
    if (sector_buffer[510] != 0x55 || sector_buffer[511] != 0xAA) return -2;

    fat_start_sector = bpb.reserved_sectors;
    root_dir_start_sector = fat_start_sector + (bpb.fat_count * bpb.sectors_per_fat);
    data_start_sector = root_dir_start_sector + 
                        ((bpb.root_entry_count * 32) + (bpb.bytes_per_sector - 1)) / bpb.bytes_per_sector;

    root_dir_entries = bpb.root_entry_count;
    sectors_per_cluster = bpb.sectors_per_cluster;

    return 0;
}

static int find_file(const char* filename, FAT_DirectoryEntry* entry, uint16_t* entry_pos) {
    uint8_t formatted_name[11];
    int name_len = 0;
    int ext_len = 0;
    int dot_pos = -1;

    for (int i = 0; filename[i] != '\0' && i < 12; i++) {
        if (filename[i] == '.') {
            dot_pos = i;
        } else if (dot_pos == -1) {
            name_len++;
        } else {
            ext_len++;
        }
    }

    for (int i = 0; i < 11; i++) {
        formatted_name[i] = ' '; 
    }

    for (int i = 0; i < name_len && i < 8; i++) {
        formatted_name[i] = filename[i];
        if (formatted_name[i] >= 'a' && formatted_name[i] <= 'z') {
            formatted_name[i] -= 32; 
        }
    }

    if (dot_pos != -1) {
        for (int i = 0; i < ext_len && i < 3; i++) {
            formatted_name[8 + i] = filename[dot_pos + 1 + i];
            if (formatted_name[8 + i] >= 'a' && formatted_name[8 + i] <= 'z') {
                formatted_name[8 + i] -= 32; 
            }
        }
    }

    int entries_per_sector = bpb.bytes_per_sector / sizeof(FAT_DirectoryEntry);

    for (uint16_t entry_idx = 0; entry_idx < root_dir_entries; entry_idx++) {
        uint32_t sector = root_dir_start_sector + (entry_idx / entries_per_sector);
        uint32_t offset = (entry_idx % entries_per_sector) * sizeof(FAT_DirectoryEntry);

        if (disk_read_sector(sector, sector_buffer) != 0) return -1;

        FAT_DirectoryEntry* dir_entry = (FAT_DirectoryEntry*)(sector_buffer + offset);

        if (dir_entry->filename[0] == 0) break; 
        if (dir_entry->filename[0] == 0xE5) continue;

        bool match = true;
        for (int i = 0; i < 11; i++) {
            if (i < 8 && dir_entry->filename[i] != formatted_name[i]) {
                match = false;
                break;
            }
            if (i >= 8 && dir_entry->extension[i-8] != formatted_name[i]) {
                match = false;
                break;
            }
        }

        if (match) {

            if (entry) {
                *entry = *dir_entry;
            }
            if (entry_pos) {
                *entry_pos = entry_idx;
            }
            return 0;
        }
    }

    return -1; 
}

static int create_file(const char* filename, FAT_DirectoryEntry* entry, uint16_t* entry_pos) {
    uint8_t formatted_name[11];
    int name_len = 0;
    int ext_len = 0;
    int dot_pos = -1;

    for (int i = 0; filename[i] != '\0' && i < 12; i++) {
        if (filename[i] == '.') {
            dot_pos = i;
        } else if (dot_pos == -1) {
            name_len++;
        } else {
            ext_len++;
        }
    }

    for (int i = 0; i < 11; i++) {
        formatted_name[i] = ' '; 
    }

    for (int i = 0; i < name_len && i < 8; i++) {
        formatted_name[i] = filename[i];
        if (formatted_name[i] >= 'a' && formatted_name[i] <= 'z') {
            formatted_name[i] -= 32; 
        }
    }

    if (dot_pos != -1) {
        for (int i = 0; i < ext_len && i < 3; i++) {
            formatted_name[8 + i] = filename[dot_pos + 1 + i];
            if (formatted_name[8 + i] >= 'a' && formatted_name[8 + i] <= 'z') {
                formatted_name[8 + i] -= 32; 
            }
        }
    }

    int entries_per_sector = bpb.bytes_per_sector / sizeof(FAT_DirectoryEntry);

    for (uint16_t entry_idx = 0; entry_idx < root_dir_entries; entry_idx++) {
        uint32_t sector = root_dir_start_sector + (entry_idx / entries_per_sector);
        uint32_t offset = (entry_idx % entries_per_sector) * sizeof(FAT_DirectoryEntry);

        if (disk_read_sector(sector, sector_buffer) != 0) {
            return -1;
        }

        FAT_DirectoryEntry* dir_entry = (FAT_DirectoryEntry*)(sector_buffer + offset);

        if (dir_entry->filename[0] == 0 || dir_entry->filename[0] == 0xE5) {

            for (int i = 0; i < 8; i++) {
                dir_entry->filename[i] = formatted_name[i];
            }
            for (int i = 0; i < 3; i++) {
                dir_entry->extension[i] = formatted_name[i+8];
            }

            dir_entry->attributes = FAT_ATTR_ARCHIVE;
            dir_entry->starting_cluster = 0; 
            dir_entry->file_size = 0;        
            dir_entry->time = 0;             
            dir_entry->date = 0;             

            if (disk_write_sector(sector, sector_buffer) != 0) {
                return -1;
            }

            if (entry) {
                *entry = *dir_entry;
            }
            if (entry_pos) {
                *entry_pos = entry_idx;
            }

            return 0;
        }
    }

    return -1; 
}

static uint16_t read_fat_entry(uint16_t cluster) {
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fat_start_sector + (fat_offset / bpb.bytes_per_sector);
    uint32_t entry_offset = fat_offset % bpb.bytes_per_sector;

    if (disk_read_sector(fat_sector, sector_buffer) != 0) {
        return 0xFFFF; 
    }

    return *(uint16_t*)(sector_buffer + entry_offset);
}

static int write_fat_entry(uint16_t cluster, uint16_t value) {
    uint32_t fat_offset = cluster * 2;
    uint32_t fat_sector = fat_start_sector + (fat_offset / bpb.bytes_per_sector);
    uint32_t entry_offset = fat_offset % bpb.bytes_per_sector;

    if (disk_read_sector(fat_sector, sector_buffer) != 0) {
        return -1;
    }

    *(uint16_t*)(sector_buffer + entry_offset) = value;

    if (disk_write_sector(fat_sector, sector_buffer) != 0) {
        return -1;
    }

    if (bpb.fat_count > 1) {
        uint32_t backup_fat_sector = fat_sector + bpb.sectors_per_fat;
        if (disk_write_sector(backup_fat_sector, sector_buffer) != 0) {
            return -1;
        }
    }

    return 0;
}

static uint16_t allocate_cluster() {

    for (uint16_t cluster = 2; cluster < bpb.sectors_per_fat * 256; cluster++) {
        uint16_t entry = read_fat_entry(cluster);

        if (entry == 0) {

            if (write_fat_entry(cluster, 0xFFFF) != 0) {
                return 0;
            }
            return cluster;
        }
    }

    return 0; 
}

int fs_open(const char* filename, uint8_t mode) {

    int fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_descriptors[i].in_use) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        return -1;
    }

    FAT_DirectoryEntry entry;
    uint16_t entry_pos;
    int result = find_file(filename, &entry, &entry_pos);

    if (result != 0 && !(mode & FS_MODE_CREATE)) {
        return -2;
    }

    if (result != 0 && (mode & FS_MODE_CREATE)) {
        result = create_file(filename, &entry, &entry_pos);
        if (result != 0) {
            return -3;
        }
    }

    file_descriptors[fd].in_use = 1;

    int name_len = 0;
    while (filename[name_len] != '\0' && name_len < 12) {
        file_descriptors[fd].filename[name_len] = filename[name_len];
        name_len++;
    }
    file_descriptors[fd].filename[name_len] = '\0';

    file_descriptors[fd].current_cluster = entry.starting_cluster;
    file_descriptors[fd].position = 0;
    file_descriptors[fd].size = entry.file_size;
    file_descriptors[fd].mode = mode;
    file_descriptors[fd].directory_entry_pos = entry_pos;

    if (mode & FS_MODE_APPEND) {
        fs_seek(fd, entry.file_size);
    }

    return fd;
}

int fs_read(int fd, void* buffer, uint32_t size) {

    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].in_use) {
        return -1;
    }

    if (!(file_descriptors[fd].mode & FS_MODE_READ)) {
        return -2;
    }

    uint32_t bytes_left = file_descriptors[fd].size - file_descriptors[fd].position;
    uint32_t bytes_to_read = (size < bytes_left) ? size : bytes_left;

    if (bytes_to_read == 0) {
        return 0;
    }

    uint32_t cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint16_t start_cluster = file_descriptors[fd].current_cluster;
    uint32_t cluster_offset = file_descriptors[fd].position % cluster_size;

    if (cluster_offset == 0 && file_descriptors[fd].position > 0) {
        start_cluster = read_fat_entry(start_cluster);
        if (start_cluster >= 0xFFF8) {

            return 0;
        }
    }

    uint32_t bytes_read = 0;
    uint8_t* buf_ptr = (uint8_t*)buffer;
    uint16_t current_cluster = start_cluster;

    while (bytes_read < bytes_to_read) {

        uint32_t sector_offset = cluster_offset / bpb.bytes_per_sector;
        uint32_t sector = data_start_sector + 
            ((current_cluster - 2) * sectors_per_cluster) + sector_offset;

        uint32_t offset_in_sector = cluster_offset % bpb.bytes_per_sector;

        if (disk_read_sector(sector, sector_buffer) != 0) {
            return -3;
        }

        uint32_t bytes_this_sector = bpb.bytes_per_sector - offset_in_sector;
        if (bytes_this_sector > bytes_to_read - bytes_read) {
            bytes_this_sector = bytes_to_read - bytes_read;
        }

        for (uint32_t i = 0; i < bytes_this_sector; i++) {
            buf_ptr[bytes_read++] = sector_buffer[offset_in_sector + i];
        }

        file_descriptors[fd].position += bytes_this_sector;
        cluster_offset += bytes_this_sector;

        if (cluster_offset >= cluster_size) {
            current_cluster = read_fat_entry(current_cluster);
            if (current_cluster >= 0xFFF8) {
                break; 
            }
            cluster_offset = 0;
        }
    }

    file_descriptors[fd].current_cluster = current_cluster;

    return bytes_read;
}

int fs_write(int fd, const void* buffer, uint32_t size) {

    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].in_use) {
        return -1;
    }

    if (!(file_descriptors[fd].mode & (FS_MODE_WRITE | FS_MODE_APPEND))) {
        return -2;
    }

    if (size == 0) {
        return 0;
    }

    uint32_t cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint16_t current_cluster = file_descriptors[fd].current_cluster;
    uint32_t cluster_offset = file_descriptors[fd].position % cluster_size;

    if (current_cluster == 0) {
        current_cluster = allocate_cluster();
        if (current_cluster == 0) {
            return -3; 
        }

        file_descriptors[fd].current_cluster = current_cluster;

        int entries_per_sector = bpb.bytes_per_sector / sizeof(FAT_DirectoryEntry);
        uint32_t sector = root_dir_start_sector + 
            (file_descriptors[fd].directory_entry_pos / entries_per_sector);
        uint32_t offset = (file_descriptors[fd].directory_entry_pos % entries_per_sector) * 
            sizeof(FAT_DirectoryEntry);

        if (disk_read_sector(sector, sector_buffer) != 0) {
            return -4;
        }

        FAT_DirectoryEntry* dir_entry = (FAT_DirectoryEntry*)(sector_buffer + offset);
        dir_entry->starting_cluster = current_cluster;

        if (disk_write_sector(sector, sector_buffer) != 0) {
            return -5;
        }
    }

    if (cluster_offset == 0 && file_descriptors[fd].position > 0) {
        uint16_t next_cluster = read_fat_entry(current_cluster);
        if (next_cluster >= 0xFFF8) {

            next_cluster = allocate_cluster();
            if (next_cluster == 0) {
                return -3; 
            }

            if (write_fat_entry(current_cluster, next_cluster) != 0) {
                return -6;
            }
        }
        current_cluster = next_cluster;
    }

    uint32_t bytes_written = 0;
    const uint8_t* buf_ptr = (const uint8_t*)buffer;

    while (bytes_written < size) {

        if (cluster_offset >= cluster_size) {
            uint16_t next_cluster = read_fat_entry(current_cluster);
            if (next_cluster >= 0xFFF8) {

                next_cluster = allocate_cluster();
                if (next_cluster == 0) {
                    break; 
                }

                if (write_fat_entry(current_cluster, next_cluster) != 0) {
                    break;
                }
            }
            current_cluster = next_cluster;
            cluster_offset = 0;
        }

        uint32_t sector_offset = cluster_offset / bpb.bytes_per_sector;
        uint32_t sector = data_start_sector + 
            ((current_cluster - 2) * sectors_per_cluster) + sector_offset;

        uint32_t offset_in_sector = cluster_offset % bpb.bytes_per_sector;

        if (offset_in_sector > 0 || bytes_written + bpb.bytes_per_sector > size) {
            if (disk_read_sector(sector, sector_buffer) != 0) {
                break;
            }
        }

        uint32_t bytes_this_sector = bpb.bytes_per_sector - offset_in_sector;
        if (bytes_this_sector > size - bytes_written) {
            bytes_this_sector = size - bytes_written;
        }

        for (uint32_t i = 0; i < bytes_this_sector; i++) {
            sector_buffer[offset_in_sector + i] = buf_ptr[bytes_written++];
        }

        if (disk_write_sector(sector, sector_buffer) != 0) {
            break;
        }

        file_descriptors[fd].position += bytes_this_sector;
        cluster_offset += bytes_this_sector;
    }

    if (file_descriptors[fd].position > file_descriptors[fd].size) {
        file_descriptors[fd].size = file_descriptors[fd].position;

        int entries_per_sector = bpb.bytes_per_sector / sizeof(FAT_DirectoryEntry);
        uint32_t sector = root_dir_start_sector + 
            (file_descriptors[fd].directory_entry_pos / entries_per_sector);
        uint32_t offset = (file_descriptors[fd].directory_entry_pos % entries_per_sector) * 
            sizeof(FAT_DirectoryEntry);

        if (disk_read_sector(sector, sector_buffer) != 0) {
            return bytes_written;
        }

        FAT_DirectoryEntry* dir_entry = (FAT_DirectoryEntry*)(sector_buffer + offset);
        dir_entry->file_size = file_descriptors[fd].size;

        if (disk_write_sector(sector, sector_buffer) != 0) {
            return bytes_written;
        }
    }

    file_descriptors[fd].current_cluster = current_cluster;

    return bytes_written;
}

int fs_close(int fd) {

    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].in_use) {
        return -1;
    }

    file_descriptors[fd].in_use = 0;

    return 0;
}

int fs_seek(int fd, uint32_t position) {

    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].in_use) {
        return -1;
    }

    if (position > file_descriptors[fd].size) {
        return -2;
    }

    if (position == file_descriptors[fd].position) {
        return 0;
    }

    if (position == 0) {
        file_descriptors[fd].position = 0;
        file_descriptors[fd].current_cluster = 
            file_descriptors[fd].current_cluster > 0 ? 
            file_descriptors[fd].current_cluster : 0;
        return 0;
    }

    uint32_t cluster_size = bpb.sectors_per_cluster * bpb.bytes_per_sector;
    uint16_t cluster = file_descriptors[fd].current_cluster;

    if (position < file_descriptors[fd].position) {

        int entries_per_sector = bpb.bytes_per_sector / sizeof(FAT_DirectoryEntry);
        uint32_t sector = root_dir_start_sector + 
            (file_descriptors[fd].directory_entry_pos / entries_per_sector);
        uint32_t offset = (file_descriptors[fd].directory_entry_pos % entries_per_sector) * 
            sizeof(FAT_DirectoryEntry);

        if (disk_read_sector(sector, sector_buffer) != 0) {
            return -3;
        }

        FAT_DirectoryEntry* dir_entry = (FAT_DirectoryEntry*)(sector_buffer + offset);
        cluster = dir_entry->starting_cluster;
    }

    uint32_t target_cluster_index = position / cluster_size;
    uint32_t current_cluster_index = file_descriptors[fd].position / cluster_size;

    while (current_cluster_index < target_cluster_index) {
        cluster = read_fat_entry(cluster);
        if (cluster >= 0xFFF8) {

            return -4;
        }
        current_cluster_index++;
    }

    file_descriptors[fd].position = position;
    file_descriptors[fd].current_cluster = cluster;

    return 0;
}

int fs_tell(int fd) {

    if (fd < 0 || fd >= MAX_OPEN_FILES || !file_descriptors[fd].in_use) {
        return -1;
    }

    return file_descriptors[fd].position;
}

int fs_delete(const char* filename) {

    FAT_DirectoryEntry entry;
    uint16_t entry_pos;
    int result = find_file(filename, &entry, &entry_pos);

    if (result != 0) {
        return -1;
    }

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (file_descriptors[i].in_use && 
            file_descriptors[i].directory_entry_pos == entry_pos) {
            return -2; 
        }
    }

    int entries_per_sector = bpb.bytes_per_sector / sizeof(FAT_DirectoryEntry);
    uint32_t sector = root_dir_start_sector + (entry_pos / entries_per_sector);
    uint32_t offset = (entry_pos % entries_per_sector) * sizeof(FAT_DirectoryEntry);

    if (disk_read_sector(sector, sector_buffer) != 0) {
        return -3;
    }

    FAT_DirectoryEntry* dir_entry = (FAT_DirectoryEntry*)(sector_buffer + offset);
    dir_entry->filename[0] = 0xE5; 

    if (disk_write_sector(sector, sector_buffer) != 0) {
        return -4;
    }

    uint16_t cluster = entry.starting_cluster;
    while (cluster >= 2 && cluster < 0xFFF8) {
        uint16_t next_cluster = read_fat_entry(cluster);
        write_fat_entry(cluster, 0);
        cluster = next_cluster;
    }

    return 0;
}