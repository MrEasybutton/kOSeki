#include "fat32.h"
#include "console.h"
#include "ide.h"
#include "string.h"
#include "kheap.h"
#include "serial.h"
#include "utils.h"

#define DRIVE 0
#define FAT_CACHE_SIZE 5
#define OFFSET_ATTRIB 0x0B
#define OFFSET_CLUS_HI 0x14
#define OFFSET_CLUS_LOW 0x1A
#define FLAG_LFN 0x0F

uint32 g_bytes_per_sector;
uint32 g_DataSectionLBA;
uint8 g_FAT_Cache[FAT_CACHE_SIZE * 512];
fat_bpb* g_fat_bpb;
const char LFN_idxs[] = {1,3,5,7,9,14,16,18,20,22,24,28,30};
uint32 g_last_fat_sector = -1;

uint32 FAT_ClusterToLba(uint32 cluster) {
    return g_DataSectionLBA + (cluster - 2) * g_fat_bpb->sectors_per_cluster;
}

void FAT_readfat(uint32 lba_index) {
    ide_read_sectors(DRIVE, FAT_CACHE_SIZE, g_fat_bpb->reserved_sector_count + lba_index, g_FAT_Cache);
}

fat_bpb* fat_read_bpb() {
    fat_bpb* bpb = kmalloc(sizeof(fat_bpb));
    if (!bpb) return NULL;
    
    uint8* sector_buf = kmalloc(512);
    if (!sector_buf) {
        kfree(bpb);
        return NULL;
    }
    
    // read primary boot sector
    if (ide_read_sectors(DRIVE, 1, 0, sector_buf) != 0) {
        // fallback boot (6)
        if (ide_read_sectors(DRIVE, 1, 6, sector_buf) != 0) {
            kfree(sector_buf);
            kfree(bpb);
            return NULL;
        }
    }
    
    memcpy(bpb, sector_buf, sizeof(fat_bpb));
    kfree(sector_buf);
    return bpb;
}

uint8* load_cluster(int clu) {
    uint8* buf = kmalloc(g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector);
    ide_read_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(clu), buf);
    return buf;
}

void ide_read_cluster_into_buffer(int clu, uint8* buffer) {
    ide_read_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(clu), buffer);
}

uint32 fat_get_entry(uint32 cluster) {
    uint32 fat_index = cluster * 4;
    uint32 fat_index_sector = fat_index / g_bytes_per_sector;
    uint32 fat_ent_offset = fat_index % g_bytes_per_sector;

    if (g_last_fat_sector == (uint32)-1 || fat_index_sector < g_last_fat_sector || fat_index_sector >= g_last_fat_sector + FAT_CACHE_SIZE) {
        FAT_readfat(fat_index_sector);
        g_last_fat_sector = fat_index_sector;
    }

    uint32 cache_sector_offset = fat_index_sector - g_last_fat_sector;
    uint32 cache_offset = (cache_sector_offset * g_bytes_per_sector) + fat_ent_offset;

    return *(uint32*)(g_FAT_Cache + cache_offset);
}

uint32 fat_get_next_clu(uint32 curr_cluster) {
    uint32 next_cluster = fat_get_entry(curr_cluster);
    return (next_cluster >= 0x0FFFFFF8) ? (uint32)-1 : next_cluster;
}

static void fat_set_entry(uint32 cluster, uint32 value) {
    uint32 fat_index = cluster * 4;
    uint32 fat_index_sector = fat_index / g_bytes_per_sector;
    uint32 fat_ent_offset = fat_index % g_bytes_per_sector;

    // read sector with FAT entry
    char* sector_buf = kmalloc(g_bytes_per_sector);
    ide_read_sectors(DRIVE, 1, g_fat_bpb->reserved_sector_count + fat_index_sector, (uint8*)sector_buf);

    *(uint32*)(sector_buf + fat_ent_offset) = value;

    //write back to disk
    ide_write_sectors(DRIVE, 1, g_fat_bpb->reserved_sector_count + fat_index_sector, (uint8*)sector_buf);

    kfree(sector_buf);

    // invalidate cache
    g_last_fat_sector = (uint32)-1;
}

static uint32 fat_find_free_cluster() {
    uint32 total_clusters = (g_fat_bpb->total_sectors_32 - g_DataSectionLBA) / g_fat_bpb->sectors_per_cluster;
    for (uint32 i = 2; i < total_clusters; i++) {
        if (fat_get_entry(i) == 0) {
            return i;
        }
    }
    return (uint32)-1;
}

void fat_dir_ls(uint32 cluster) {
    uint8* cluster_buf = load_cluster(cluster);
    if (!cluster_buf) {
        kprint("[FAT32] Failed to load cluster %d in fat_dir_ls\n", cluster);
        return;
    }
    uint32 record_offset = 0;
    uint32 LFN_index = 0;
    char LFN[105], SFN[12];
    memset(LFN, 0, 105);
    memset(SFN, 0, 12);

    while(TRUE) {
        if(cluster_buf[record_offset] == 0) {
            kprint("\nEnd of directory.");
            kprint("\n");
            break;
        }
        if(cluster_buf[record_offset] != 0xE5) {
            if((cluster_buf[record_offset + OFFSET_ATTRIB] & FLAG_LFN) == FLAG_LFN) {
                if((cluster_buf[record_offset] & 0xF0) == 0x40)
                    LFN_index = cluster_buf[record_offset] & 0x0F;
                if((cluster_buf[record_offset] & 0x0F) == LFN_index) {
                    if (LFN_index > 0) LFN_index--;
                } else {
                    kprint("\nError");
                }
                for(int i = 0; i < 13; i++) {
                    int byte_pos = record_offset + LFN_idxs[i];
                    LFN[LFN_index * 13 + i] = cluster_buf[byte_pos + 1] == 0x00 ? cluster_buf[byte_pos] : 0;
                }
            } else {
                memcpy(SFN, cluster_buf+record_offset, 11);
                kprint("\n%s", LFN[0] == 0 ? SFN : LFN);
                memset(LFN, 0, 105);
            }
        }
        record_offset += 32;
        if(record_offset == (g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector)) {
            uint32 next_cluster = fat_get_next_clu(cluster);
            if(next_cluster == (uint32)-1) {
                break;
            }

            kfree(cluster_buf);
            cluster = next_cluster;
            cluster_buf = load_cluster(cluster);
            if (!cluster_buf) {
                kprint("[FAT32] Failed to load next cluster %d in fat_dir_ls\n", cluster);
                break;
            }
            record_offset = 0;
        }
    }
    
    if (cluster_buf) kfree(cluster_buf);
}

#define LS_BUFFER_SIZE 8192

char* get_ls(const char* path) {
    FAT_dirent* dirent = fat_find_file(path);
    if (!dirent) {
        char* buf = kmalloc(128);
        strcpy(buf, "Directory not found: ");
        strcat(buf, path);
        return buf;
    }
    if (!(dirent->Attributes & 0x10)) {
        char* buf = kmalloc(128);
        strcpy(buf, "Not a directory: ");
        strcat(buf, path);
        kfree(dirent);
        return buf;
    }

    uint32 cluster = (dirent->FirstClusterHigh << 16) | dirent->FirstClusterLow;
    kfree(dirent);

    FAT_DirList* list = fat_get_dir_ls(cluster);
    if (!list) {
        char* buf = kmalloc(128);
        strcpy(buf, "Failed to read directory: ");
        strcat(buf, path);
        return buf;
    }

    char* out = kmalloc(LS_BUFFER_SIZE);
    if (!out) {
        kfree(list->entries);
        kfree(list);
        return NULL;
    }
    out[0] = '\0';

    append_str(out, "DIR: ");
    append_str(out, path);
    append_str(out, "\n>>>\n");

    for (uint32 i = 0; i < list->count; i++) {
        FAT_FileEntry* entry = &list->entries[i];
        append_str(out, entry->name);

        if (!(entry->attributes & 0x10)) {
            append_str(out, " (");
            append_int(out, entry->size);
            append_str(out, " bytes)");
        } else {
            append_str(out, " [dir]");
        }

        append_str(out, "\n");
    }

    kfree(list->entries);
    kfree(list);

    return out;
}

void make_sfn(const char *src, char dest[12]) {
    memset(dest, ' ', 11);
    dest[11] = '\0';

    const char *dot = strchr(src, '.');

    int i = 0;

    while (*src && src != dot && i < 8) {
        dest[i++] = upper(*src++);
    }

    if (dot) {
        src = dot + 1;
        int j = 8;
        int k = 0;

        while (*src && k < 3) {
            dest[j++] = upper(*src++);
            k++;
        }
    }
}

FAT_dirent* fat_find_in_dir(char* fname, int cluster) {
    kprint("fat_find_in_dir: fname='%s', cluster=%d\n", fname, cluster);
    uint8* cluster_buf = load_cluster(cluster);
    uint32 record_offset = 0;
    uint32 LFN_index = 0;
    char LFN[MAX_FILENAME_LEN + 1];
    char SFN_entry[12];
    char fname_upper[MAX_FILENAME_LEN + 1];
    memset(LFN, 0, MAX_FILENAME_LEN + 1);
    memset(SFN_entry, 0, 12);

    make_sfn(fname, fname_upper);
    kprint("fat_find_in_dir: fname_upper(SFN padded)='%11s'\n", fname_upper);


    while(TRUE) {
        if(cluster_buf[record_offset] == 0) {
            kprint("fat_find_in_dir: End of directory, break.\n");
            kfree(cluster_buf);
            cluster_buf = NULL;
            break;
        }
        if(cluster_buf[record_offset] != 0xE5) {
            if((cluster_buf[record_offset + OFFSET_ATTRIB] & FLAG_LFN) == FLAG_LFN) {
                // lfn entry
                if((cluster_buf[record_offset] & 0xF0) == 0x40) {
                    memset(LFN, 0, MAX_FILENAME_LEN + 1);
                    LFN_index = cluster_buf[record_offset] & 0x0F;
                }
                
                if((cluster_buf[record_offset] & 0x0F) == LFN_index) {
                    int part_idx = LFN_index - 1;
                    for(int i = 0; i < 13; i++) {
                        if (part_idx * 13 + i < MAX_FILENAME_LEN) {
                            uint16 char_part = *(uint16*)(cluster_buf + record_offset + LFN_idxs[i]);
                            if (char_part <= 0xFF) {
                                LFN[part_idx * 13 + i] = (char)char_part;
                            } else {
                                LFN[part_idx * 13 + i] = '?'; //plc
                            }
                        }
                    }
                    if (LFN_index > 0) LFN_index--;
                }
            } else {
                memcpy(SFN_entry, cluster_buf+record_offset, 11);
                SFN_entry[11] = '\0';

                BOOL found = FALSE;
                if (LFN[0] != '\0' && strcmp(LFN, fname) == 0) {
                    found = TRUE;
                } else if (strcmp(SFN_entry, fname_upper) == 0) {
                    found = TRUE;
                }

                if(found) {
                    FAT_dirent* dirent = kmalloc(sizeof(FAT_dirent));
                    memcpy(dirent, cluster_buf + record_offset, sizeof(FAT_dirent));
                    kfree(cluster_buf);
                    kprint("fat_find_in_dir: Dirent found: %s\n", LFN[0] == 0 ? SFN_entry : LFN);
                    return dirent;
                }
                memset(LFN, 0, MAX_FILENAME_LEN + 1); // clear after proc sfn
                LFN_index = 0;
            }
        } else {
            memset(LFN, 0, MAX_FILENAME_LEN + 1);
            LFN_index = 0;
        }
        record_offset += 32;
        if(record_offset == (g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector)) {
            uint32 next_cluster = fat_get_next_clu(cluster);
            kprint("fat_find_in_dir: End of cluster. Next cluster: %d\n", next_cluster);
            if(next_cluster == (uint32)-1) {
                kfree(cluster_buf);
                cluster_buf = NULL;
                break;
            }
            kfree(cluster_buf);
            cluster = next_cluster;
            cluster_buf = load_cluster(cluster);
            if (!cluster_buf) {
                kprint("[FAT32] Failed to load next cluster %d for fat_find_in_dir\n", cluster);
                break;
            }
            record_offset = 0;
            LFN_index = 0;
            memset(LFN, 0, MAX_FILENAME_LEN + 1);
        }
    }

    if (cluster_buf) kfree(cluster_buf);

    kprint("fat_find_in_dir: Dirent not found.\n");
    return NULL;
}

static uint32 fat_get_parent_cluster(const char* path) {
    if (!path || path[0] != '/') return (uint32)-1;
    if (strcmp(path, "/") == 0) return 2;

    const char* last = strrchr(path, '/');
    if (last == path) return 2;

    int len = last - path;
    char* parent_path = kmalloc(len + 1);
    memcpy(parent_path, path, len);
    parent_path[len] = '\0';

    FAT_dirent* dirent = fat_find_file(parent_path);
    kfree(parent_path);

    if (!dirent) return (uint32)-1;
    if (!(dirent->Attributes & 0x10)) {
        kfree(dirent);
        return (uint32)-1;
    }

    uint32 clu = (dirent->FirstClusterHigh << 16) | dirent->FirstClusterLow;
    kfree(dirent);
    return clu;
}

static const char* fat_get_last_segment(const char* path) {
    if (!path) return NULL;
    const char* last_slash = strrchr(path, '/');
    if (!last_slash) return path;
    return last_slash + 1;
}

static uint32 fat_find_free_slot(uint32 dir_cluster, int* out_offset) {
    uint32 cluster_size = g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector;
    uint32 current_clu = dir_cluster;
    
    while (current_clu >= 2 && current_clu < 0x0FFFFFF8) {
        uint8* buf = load_cluster(current_clu);
        for (uint32 off = 0; off < cluster_size; off += 32) {
            if (buf[off] == 0x00 || buf[off] == 0xE5) {
                *out_offset = off;
                kfree(buf);
                return current_clu;
            }
        }
        kfree(buf);
        
        uint32 next = fat_get_next_clu(current_clu);
        if (next >= 0x0FFFFFF8) {
            // extend dir
            uint32 new_clu = fat_find_free_cluster();
            if (new_clu == (uint32)-1) return (uint32)-1;
            
            fat_set_entry(current_clu, new_clu);
            fat_set_entry(new_clu, 0x0FFFFFFF);
            
            uint8* empty = kmalloc(cluster_size);
            memset(empty, 0, cluster_size);
            ide_write_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(new_clu), empty);
            kfree(empty);
            
            *out_offset = 0;
            return new_clu;
        }
        current_clu = next;
    }
    return (uint32)-1;
}

FAT_dirent* fat_find_file(const char* path) {
    kprint("fat_find_file: Searching for path: '%s'\n", path);
    uint32 current_cluster = 2;
    FAT_dirent* dirent = NULL;

    if (strcmp(path, "/") == 0) {
        dirent = kmalloc(sizeof(FAT_dirent));
        if (dirent) {
            memset(dirent, 0, sizeof(FAT_dirent));
            dirent->Attributes = 0x10;
            dirent->FirstClusterLow = 2;
            dirent->FirstClusterHigh = 0;
        }
        kprint("fat_find_file: Found root directory.\n");
        return dirent;
    }

    char* path_copy = kmalloc(strlen(path) + 1);
    if (!path_copy) return NULL;
    strcpy(path_copy, path);

    char* seg_start = path_copy;
    if (*seg_start == '/') seg_start++;

    char* seg_end = seg_start;
    while (*seg_start != '\0') {
        while (*seg_end != '\0' && *seg_end != '/') seg_end++;
        char temp = *seg_end;
        *seg_end = '\0';

        kprint("fat_find_file: Searching segment: '%s' in cluster %d\n", seg_start, current_cluster);
        if (dirent) {
            kfree(dirent);
            dirent = NULL;
        }
        dirent = fat_find_in_dir(seg_start, current_cluster);

        if (dirent == NULL) {
            kprint("fat_find_file: Segment '%s' NOT found.\n", seg_start);
            kfree(path_copy);
            return NULL;
        }

        kprint("fat_find_file: Segment '%s' found. Attributes: 0x%x\n", seg_start, dirent->Attributes);

        if ((dirent->Attributes & 0x10) && (temp != '\0')) {
            current_cluster = (dirent->FirstClusterHigh << 16) | dirent->FirstClusterLow;
            kprint("fat_find_file: Moving to next directory cluster: %d\n", current_cluster);
        } else if (!(dirent->Attributes & 0x10) && (temp != '\0')) {
            kprint("fat_find_file: Found file '%s' but more path segments remain.\n", seg_start);
            kfree(dirent);
            kfree(path_copy);
            return NULL;
        }

        *seg_end = temp;
        if (temp != '\0') {
            seg_start = seg_end + 1;
            seg_end = seg_start;
        } else { seg_start = seg_end; }
    }

    kfree(path_copy);
    kprint("fat_find_file: Returning dirent for '%s'\n", path);
    return dirent;
}

char* fat_read_file(char* fpath) {
    kprint("fat_read_file: Reading file: '%s'\n", fpath);
    FAT_dirent* dirent = fat_find_file(fpath);
    if (!dirent) {
        kprint("fat_read_file: dirent NULL for '%s'\n", fpath);
        return NULL;
    }

    uint32 file_clu = (dirent->FirstClusterHigh << 16) | dirent->FirstClusterLow;
    uint32 file_size = dirent->Size;
    kfree(dirent);

    kprint("fat_read_file: File '%s' (cluster: %d, size: %d)\n", fpath, file_clu, file_size);

    if (file_size == 0) {
        char* empty = kmalloc(1);
        if (!empty) return NULL;
        empty[0] = '\0';
        kprint("fat_read_file: File '%s' is empty.\n", fpath);
        return empty;
    }


    char* file_buf = kmalloc(file_size + 1);
    if (!file_buf) return NULL;
    file_buf[file_size] = '\0';

    uint32 cluster_size = g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector;
    char* current_pos = file_buf;
    uint32 bytes_left = file_size;

    uint8* cluster_temp = (uint8*)kmalloc(cluster_size);
    if (!cluster_temp) {
        kfree(file_buf);
        return NULL;
    }

    while (bytes_left > 0 && (int)file_clu != -1) {
        uint32 bytes_to_read = bytes_left > cluster_size ? cluster_size : bytes_left;
        
        if (bytes_to_read == cluster_size) {
            ide_read_cluster_into_buffer(file_clu, (uint8*)current_pos);
        } else { //partial !bufov
            ide_read_cluster_into_buffer(file_clu, cluster_temp);
            memcpy(current_pos, cluster_temp, bytes_to_read);
        }
        
        current_pos += bytes_to_read;
        bytes_left -= bytes_to_read;
        if (bytes_left > 0)
            file_clu = fat_get_next_clu(file_clu);
    }
    kfree(cluster_temp);
    kprint("fat_read_file: Finished reading '%s'.\n", fpath);
    return file_buf;
}

int fat_read_file_offset(uint32 start_cluster, uint32 offset, uint32 size, void* buffer) {
    if (start_cluster < 2) return -1;
    
    uint32 cluster_size = g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector;
    uint32 clusters_to_skip = offset / cluster_size;
    uint32 offset_in_cluster = offset % cluster_size;
    
    uint32 current_clu = start_cluster;
    for (uint32 i = 0; i < clusters_to_skip; i++) {
        current_clu = fat_get_next_clu(current_clu);
        if (current_clu == (uint32)-1) return -1;
    }
    
    uint32 bytes_read = 0;
    uint8* cluster_temp = (uint8*)kmalloc(cluster_size);
    if (!cluster_temp) return -1;
    
    while (bytes_read < size && current_clu != (uint32)-1) {
        ide_read_cluster_into_buffer(current_clu, cluster_temp);
        
        uint32 to_copy = cluster_size - offset_in_cluster;
        if (bytes_read + to_copy > size) to_copy = size - bytes_read;
        
        memcpy((uint8*)buffer + bytes_read, cluster_temp + offset_in_cluster, to_copy);
        
        bytes_read += to_copy;
        offset_in_cluster = 0; 
        if (bytes_read < size)
            current_clu = fat_get_next_clu(current_clu);
    }
    
    kfree(cluster_temp);
    return (int)bytes_read;
}

void fat_ls(const char* path) {
    FAT_dirent* dirent = fat_find_file(path);
    if (!dirent) {
        printf("\nDirectory not found: %s\n", path);
        return;
    }
    if (!(dirent->Attributes & 0x10)) {
        printf("\nNot a directory: %s\n", path);
        kfree(dirent);
        return;
    }
    uint32 dir_cluster = (dirent->FirstClusterHigh << 16) | dirent->FirstClusterLow;
    kfree(dirent);

    printf("\nDIR: %s\n", path);
    printf(">>>\n");
    fat_dir_ls(dir_cluster);
    printf("\n");
}

void fat_init() {
    g_fat_bpb = fat_read_bpb();
    if (!g_fat_bpb) {
        kprint("[FAT32] Failed to read BPB!\n");
        return;
    }

    g_bytes_per_sector = g_fat_bpb->bytes_per_sector;
    if (g_bytes_per_sector == 0) g_bytes_per_sector = 512;

    g_DataSectionLBA = g_fat_bpb->reserved_sector_count +
                       g_fat_bpb->table_size_32 * g_fat_bpb->table_count;

    fat_ls("/");
}

uint64 fat_get_free_space() {
    if (!g_fat_bpb || g_fat_bpb->sectors_per_cluster == 0) return 0;

    uint64 free_clusters = 0;
    if (g_fat_bpb->total_sectors_32 < g_DataSectionLBA) return 0;

    uint32 total_clusters = (g_fat_bpb->total_sectors_32 - g_DataSectionLBA) / g_fat_bpb->sectors_per_cluster;
    if (total_clusters > 0x1000000) total_clusters = 0x1000000; 

    for (uint32 i = 2; i < total_clusters; i++) {
        if (fat_get_entry(i) == 0) {
            free_clusters++;
        }
    }

    return free_clusters * (uint64)g_fat_bpb->sectors_per_cluster * (uint64)g_fat_bpb->bytes_per_sector;
}

#define MAX_DIRENTS 256

static inline char tolower(char c) {
    if (c >= 'A' && c <= 'Z') {
        return c + ('a' - 'A');
    }
    return c;
}

FAT_DirList* fat_get_dir_ls(uint32 cluster) {
    FAT_DirList* dir_list = (FAT_DirList*)kmalloc(sizeof(FAT_DirList));
    if (!dir_list) return NULL;

    dir_list->entries = (FAT_FileEntry*)kmalloc(sizeof(FAT_FileEntry) * MAX_DIRENTS);
    if (!dir_list->entries) {
        kprint("[FAT32] failed to allocate FAT_FileEntry\n");
        kfree(dir_list);
        return NULL;
    }
    dir_list->count = 0;

    uint8* cluster_buf = load_cluster(cluster);
    if (!cluster_buf) {
        kprint("[FAT32] failed to load cluster %d for directory listing.\n", cluster);
        kfree(dir_list->entries);
        kfree(dir_list);
        return NULL;
    }
    uint32 record_offset = 0;
    uint32 LFN_index = 0;
    char LFN[MAX_FILENAME_LEN + 1];
    char SFN[12];
    memset(LFN, 0, sizeof(LFN));
    memset(SFN, 0, sizeof(SFN));

    while(TRUE) {
        if(cluster_buf[record_offset] == 0) {
            kfree(cluster_buf);
            break;
        }
        if(cluster_buf[record_offset] != 0xE5) {
            if(dir_list->count >= MAX_DIRENTS) {
                kprint("[FAT32] Max file entries reached for directory listing.\n");
                kfree(cluster_buf);
                break;
            }

            if((cluster_buf[record_offset + OFFSET_ATTRIB] & FLAG_LFN) == FLAG_LFN) {
                if((cluster_buf[record_offset] & 0xF0) == 0x40) {
                    memset(LFN, 0, sizeof(LFN));
                    LFN_index = cluster_buf[record_offset] & 0x0F;
                }
                
                if((cluster_buf[record_offset] & 0x0F) == LFN_index) {
                    int part_idx = LFN_index - 1;
                    for(int i = 0; i < 13; i++) {
                        if (part_idx * 13 + i < MAX_FILENAME_LEN) {
                            uint16 char_part = *(uint16*)(cluster_buf + record_offset + LFN_idxs[i]);
                            if (char_part <= 0xFF) {
                                LFN[part_idx * 13 + i] = (char)char_part;
                            } else {
                                LFN[part_idx * 13 + i] = '?';
                            }
                        }
                    }
                    if (LFN_index > 0) LFN_index--;
                }
            } else {
                FAT_dirent* entry = (FAT_dirent*)(cluster_buf + record_offset);
                FAT_FileEntry* file_entry = &dir_list->entries[dir_list->count];

                if (LFN[0] != '\0') {
                    strncpy(file_entry->name, LFN, MAX_FILENAME_LEN);
                    file_entry->name[MAX_FILENAME_LEN - 1] = '\0';
                    memset(LFN, 0, sizeof(LFN));
                    LFN_index = 0;
                } else {
                    char sfn_name[9];
                    char sfn_ext[4];
                    memset(sfn_name, 0, sizeof(sfn_name));
                    memset(sfn_ext, 0, sizeof(sfn_ext));
                    
                    int name_len = 0;
                    for (int i = 0; i < 8 && entry->Name[i] != ' '; i++) {
                        sfn_name[name_len++] = tolower(entry->Name[i]);
                    }
                    sfn_name[name_len] = '\0';

                    int ext_len = 0;
                    for (int i = 8; i < 11 && entry->Name[i] != ' '; i++) {
                        sfn_ext[ext_len++] = tolower(entry->Name[i]);
                    }
                    sfn_ext[ext_len] = '\0';
                    
                    strncpy(file_entry->name, sfn_name, MAX_FILENAME_LEN);
                    if (ext_len > 0) {
                        strncat(file_entry->name, ".", MAX_FILENAME_LEN - strlen(file_entry->name) - 1);
                        strncat(file_entry->name, sfn_ext, MAX_FILENAME_LEN - strlen(file_entry->name) - 1);
                    }
                    file_entry->name[MAX_FILENAME_LEN - 1] = '\0';
                }
                
                file_entry->cluster = (entry->FirstClusterHigh << 16) | entry->FirstClusterLow;
                file_entry->size = entry->Size;
                file_entry->attributes = entry->Attributes;
                dir_list->count++;
            }
        } else {
            memset(LFN, 0, sizeof(LFN));
            LFN_index = 0;
        }
        record_offset += 32;
        if(record_offset == (g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector)) {
            uint32 next_cluster = fat_get_next_clu(cluster);
            if(next_cluster == (uint32)-1) {
                break;
            }
            kfree(cluster_buf);
            cluster = next_cluster;
            cluster_buf = load_cluster(cluster);
            if (!cluster_buf) {
                kprint("[FAT32] Failed to load next cluster %d for directory listing.\n", cluster);
                break;
            }
            record_offset = 0;
        }
    }

    if (cluster_buf) kfree(cluster_buf);
    return dir_list;
}

static uint8 lfn_chksum(const uint8* sfn) {
    uint8 sum = 0;
    for (int i = 11; i > 0; i--) {
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *sfn++;
    }
    return sum;
}

static uint32 fat_find_free_run(uint32 dir_cluster, int count, int* out_offset) {
    uint32 cluster_size = g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector;
    uint32 current_clu = dir_cluster;
    
    while (current_clu >= 2 && current_clu < 0x0FFFFFF8) {
        uint8* buf = load_cluster(current_clu);
        int consecutive = 0;
        int first_off = -1;

        for (uint32 off = 0; off < cluster_size; off += 32) {
            if (buf[off] == 0x00 || buf[off] == 0xE5) {
                if (consecutive == 0) first_off = off;
                consecutive++;
                if (consecutive == count) {
                    *out_offset = first_off;
                    kfree(buf);
                    return current_clu;
                }
            } else {
                consecutive = 0;
            }
        }
        kfree(buf);
        
        uint32 next = fat_get_next_clu(current_clu);
        if (next >= 0x0FFFFFF8) {
            // extend dir
            uint32 new_clu = fat_find_free_cluster();
            if (new_clu == (uint32)-1) return (uint32)-1;
            
            fat_set_entry(current_clu, new_clu);
            fat_set_entry(new_clu, 0x0FFFFFFF);
            
            uint8* empty = kmalloc(cluster_size);
            memset(empty, 0, cluster_size);
            ide_write_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(new_clu), empty);
            kfree(empty);
            
            current_clu = new_clu;
        } else {
            current_clu = next;
        }
    }
    return (uint32)-1;
}

static int fat_create_entry_lfn(uint32 parent_clu, const char* name, const char* sfn, uint8 attrib, uint32 cluster, uint32 size) {
    int name_len = strlen(name);
    int lfn_count = (name_len + 13) / 13;
    int total_slots = lfn_count + 1;

    int slot_off;
    uint32 slot_clu = fat_find_free_run(parent_clu, total_slots, &slot_off);
    if (slot_clu == (uint32)-1) return -1;

    uint32 cluster_size = g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector;
    uint8* buf = load_cluster(slot_clu);
    uint8 checksum = lfn_chksum((uint8*)sfn);

    //write reversed
    for (int i = 0; i < lfn_count; i++) {
        int entry_idx = lfn_count - 1 - i;
        FAT_LFN_entry* lfn = (FAT_LFN_entry*)(buf + slot_off + i * 32);
        memset(lfn, 0, 32);
        lfn->SequenceNumber = (i == 0) ? (0x40 | lfn_count) : (lfn_count - i);
        lfn->Attributes = 0x0F;
        lfn->Checksum = checksum;

        int start_char = (lfn_count - 1 - i) * 13;
        for (int j = 0; j < 13; j++) {
            int char_idx = start_char + j;
            uint16 c = (char_idx < name_len) ? (uint8)name[char_idx] : ((char_idx == name_len) ? 0x0000 : 0xFFFF);
            if (j < 5) lfn->NamePart1[j] = c;
            else if (j < 11) lfn->NamePart2[j - 5] = c;
            else lfn->NamePart3[j - 11] = c;
        }
    }

    FAT_dirent* de = (FAT_dirent*)(buf + slot_off + lfn_count * 32);
    memset(de, 0, sizeof(FAT_dirent));
    memcpy(de->Name, sfn, 11);
    de->Attributes = attrib;
    de->FirstClusterHigh = (cluster >> 16) & 0xFFFF;
    de->FirstClusterLow = cluster & 0xFFFF;
    de->Size = size;

    ide_write_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(slot_clu), buf);
    kfree(buf);
    return 0;
}

static int filename_to_sfn(const char* filename, char* sfn) {
    kprint("filename_to_sfn: input='%s'\n", filename);
    memset(sfn, ' ', 11);
    
    const char* dot = strchr(filename, '.');
    int name_len = dot ? (dot - filename) : strlen(filename);
    int ext_len = 0;
    if (dot) {
        const char* p = dot + 1;
        while (*p && *p != ' ' && *p != '\t' && *p != '\r' && *p != '\n') {
            ext_len++;
            p++;
        }
    }

    if (name_len == 0) return -1;
    

    //upper
    int to_copy_name = name_len > 8 ? 8 : name_len;
    for (int i = 0; i < to_copy_name; i++) {
        char c = filename[i];
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        }
        sfn[i] = c;
    }
    
    //upper ext
    if (dot) {
        int to_copy_ext = ext_len > 3 ? 3 : ext_len;
        for (int i = 0; i < to_copy_ext; i++) {
            char c = dot[1 + i];
            if (c >= 'a' && c <= 'z') {
                c = c - 'a' + 'A';
            }
            sfn[8 + i] = c;
        }
    }
    
    kprint("filename_to_sfn: SFN='%.11s'\n", sfn);
    return 0;
}

int fat_create_file(const char* path) {
    kprint("fat_create_file: path=%s\n", path);
    if (path[0] != '/') return -1;

    uint32 parent_clu = fat_get_parent_cluster(path);
    if (parent_clu == (uint32)-1) return -1;

    const char* filename = fat_get_last_segment(path);
    char sfn[11];
    
    if (filename_to_sfn(filename, sfn) != 0) {
        return -1;
    }

    FAT_dirent* existing = fat_find_in_dir((char*)filename, parent_clu);
    if (existing) {
        kfree(existing);
        return -1;
    }

    char sfn_test[12];
    make_sfn(filename, sfn_test);
    
    if (strcmp(filename, sfn_test) != 0 || strlen(filename) > 12) {
         return fat_create_entry_lfn(parent_clu, filename, sfn, 0x20, 0, 0);
    }

    int slot_off;
    uint32 slot_clu = fat_find_free_slot(parent_clu, &slot_off);
    if (slot_clu == (uint32)-1) return -1;

    uint8* buf = load_cluster(slot_clu);
    FAT_dirent new_dirent;
    memset(&new_dirent, 0, sizeof(FAT_dirent));
    memcpy(new_dirent.Name, sfn, 11);
    new_dirent.Attributes = 0x20;
    memcpy(buf + slot_off, &new_dirent, sizeof(FAT_dirent));
    ide_write_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(slot_clu), buf);
    kfree(buf);

    return 0;
}

static int fat_write_internal(const char* path, uint32 size, void* ctx, int (*fill_chunk)(void* ctx, uint32 offset, uint32 size, char* buffer)) {
    if (path[0] != '/') return -1;

    uint32 parent_clu = fat_get_parent_cluster(path);
    if (parent_clu == (uint32)-1) return -1;

    const char* filename = fat_get_last_segment(path);
    FAT_dirent* dirent_copy = fat_find_in_dir((char*)filename, parent_clu);
    if (!dirent_copy) return -1;

    uint32 original_total_size = size;

    // src cluster and offset
    uint32 dir_clu = parent_clu;
    int dirent_off = -1;
    uint32 dirent_clu = (uint32)-1;
    uint32 cluster_size = g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector;
    char sfn[11];
    filename_to_sfn(filename, sfn);

    while (dir_clu >= 2 && dir_clu < 0x0FFFFFF8) {
        uint8* buf = load_cluster(dir_clu);
        for (uint32 off = 0; off < cluster_size; off += 32) {
            // nondir match
            if (buf[off] != 0x00 && buf[off] != 0xE5 && 
                !(buf[off + 0x0B] & 0x10) && // not dir
                memcmp(buf + off, (uint8*)sfn, 11) == 0) {
                
                dirent_off = off;
                dirent_clu = dir_clu;
                kfree(buf);
                goto found_it;
            }
        }
        kfree(buf);
        dir_clu = fat_get_next_clu(dir_clu);
    }
    kfree(dirent_copy);
    return -1;

    found_it:;
    //free old
    uint32 current = (dirent_copy->FirstClusterHigh << 16) | dirent_copy->FirstClusterLow;
    while (current >= 2 && current < 0x0FFFFFF8) {
        uint32 nxt = fat_get_next_clu(current);
        fat_set_entry(current, 0);
        current = nxt;
    }

    uint32 needed = (original_total_size + cluster_size - 1) / cluster_size;
    uint32 first = 0, last = 0;
    uint32 bytes_remaining = original_total_size;
    uint32 offset = 0;

    uint8* wbuf = kmalloc(cluster_size);
    if (!wbuf) {
        kfree(dirent_copy);
        return -1;
    }

    for (uint32 i = 0; i < needed; i++) {
        uint32 nc = fat_find_free_cluster();
        if (nc == (uint32)-1) {
            kfree(wbuf);
            kfree(dirent_copy);
            return -1;
        }
        fat_set_entry(nc, 0x0FFFFFFF);
        if (i == 0) first = nc;
        else fat_set_entry(last, nc);
        last = nc;

        memset(wbuf, 0, cluster_size);
        uint32 to_w = bytes_remaining > cluster_size ? cluster_size : bytes_remaining;
        if (to_w > 0) {
            int read = fill_chunk(ctx, offset, to_w, (char*)wbuf);
            if (read < 0) {
                kfree(wbuf);
                kfree(dirent_copy);
                return -1;
            }
        }

        ide_write_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(nc), wbuf);
        offset += to_w;
        bytes_remaining -= to_w;
    }

    kfree(wbuf);

    // update dirent
    uint8* final_dir_buf = load_cluster(dirent_clu);
    FAT_dirent* de = (FAT_dirent*)(final_dir_buf + dirent_off);
    de->Size = original_total_size;
    de->FirstClusterHigh = (first >> 16) & 0xFFFF;
    de->FirstClusterLow = first & 0xFFFF;
    ide_write_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(dirent_clu), final_dir_buf);
    
    kfree(final_dir_buf);
    kfree(dirent_copy);
    return 0;
}

int fat_write_file_call(const char* path, uint32 size, void* ctx, int (*fill_chunk)(void* ctx, uint32 offset, uint32 size, char* buffer)) {
    return fat_write_internal(path, size, ctx, fill_chunk);
}

static int fat_write_file_datread(void* ctx, uint32 offset, uint32 size, char* buffer) {
    memcpy(buffer, (const char*)ctx + offset, size);
    return (int)size;
}

int fat_write_file(const char* path, const char* data, uint32 size) {
    return fat_write_file_call(path, size, (void*)data, fat_write_file_datread);
}

int fat_create_dir(const char* path) {
    if (path[0] != '/') return -1;
    FAT_dirent* ex = fat_find_file(path);
    if (ex) { kfree(ex); return 0; }

    uint32 parent_clu = fat_get_parent_cluster(path);
    if (parent_clu == (uint32)-1) return -1;

    const char* dirname = fat_get_last_segment(path);
    char sfn[11];
    if (filename_to_sfn(dirname, sfn) != 0) return -1;

    uint32 nc = fat_find_free_cluster();
    if (nc == (uint32)-1) return -1;
    fat_set_entry(nc, 0x0FFFFFFF);

    uint32 cluster_size = g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector;
    uint8* buf = kmalloc(cluster_size);
    memset(buf, 0, cluster_size);

    FAT_dirent dot; memset(&dot, 0, sizeof(dot));
    memcpy(dot.Name, ".          ", 11); dot.Attributes = 0x10;
    dot.FirstClusterHigh = (nc >> 16) & 0xFFFF; dot.FirstClusterLow = nc & 0xFFFF;
    memcpy(buf, &dot, 32);

    FAT_dirent ddot; memset(&ddot, 0, sizeof(ddot));
    memcpy(ddot.Name, "..         ", 11); ddot.Attributes = 0x10;
    ddot.FirstClusterHigh = (parent_clu >> 16) & 0xFFFF; ddot.FirstClusterLow = parent_clu & 0xFFFF;
    memcpy(buf + 32, &ddot, 32);

    ide_write_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(nc), buf);
    kfree(buf);


    char sfn_test[12];
    make_sfn(dirname, sfn_test);
    
    if (strcmp(dirname, sfn_test) != 0 || strlen(dirname) > 12) {
         return fat_create_entry_lfn(parent_clu, dirname, sfn, 0x10, nc, 0);
    }

    int slot_off;
    uint32 slot_clu = fat_find_free_slot(parent_clu, &slot_off);
    if (slot_clu == (uint32)-1) return -1;

    uint8* dbuf = load_cluster(slot_clu);
    FAT_dirent new_de; memset(&new_de, 0, sizeof(new_de));
    memcpy(new_de.Name, sfn, 11);
    new_de.Attributes = 0x10;
    new_de.FirstClusterHigh = (nc >> 16) & 0xFFFF;
    new_de.FirstClusterLow = nc & 0xFFFF;
    
    memcpy(dbuf + slot_off, &new_de, sizeof(FAT_dirent));
    ide_write_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(slot_clu), dbuf);
    kfree(dbuf);
    return 0;
}

int fat_rename_file(const char* old_path, const char* new_path) {
    kprint("fat_rename_file: old=%s, new=%s\n", old_path, new_path);
    if (old_path[0] != '/' || new_path[0] != '/') return -1;

    uint32 parent_clu = fat_get_parent_cluster(old_path);
    if (parent_clu == (uint32)-1) return -1;

    const char* old_filename = fat_get_last_segment(old_path);
    const char* new_filename = fat_get_last_segment(new_path);

    char old_sfn[11], new_sfn[11];
    if (filename_to_sfn(old_filename, old_sfn) != 0 || filename_to_sfn(new_filename, new_sfn) != 0) return -1;

    uint32 dir_clu = parent_clu;
    uint32 cluster_size = g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector;

    while (dir_clu >= 2 && dir_clu < 0x0FFFFFF8) {
        uint8* buf = load_cluster(dir_clu);
        for (uint32 off = 0; off < cluster_size; off += 32) {
            if (memcmp(buf + off, (uint8*)old_sfn, 11) == 0) {
                memcpy(buf + off, new_sfn, 11);
                ide_write_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(dir_clu), buf);
                kfree(buf);
                return 0;
            }
        }
        kfree(buf);
        dir_clu = fat_get_next_clu(dir_clu);
    }
    return -1;
}

int fat_append_file(const char* path, const char* data, uint32 size) {
    char* old_data = fat_read_file((char*)path);
    uint32 old_size = 0;
    
    FAT_dirent* de = fat_find_file(path);
    if (de) {
        old_size = de->Size;
        kfree(de);
    }

    uint32 new_size = old_size + size;
    char* new_buffer = kmalloc(new_size);
    if (!new_buffer) {
        if (old_data) kfree(old_data);
        return -1;
    }

    if (old_data) {
        memcpy(new_buffer, old_data, old_size);
        kfree(old_data);
    }
    memcpy(new_buffer + old_size, data, size);

    int res = fat_write_file(path, new_buffer, new_size);
    kfree(new_buffer);
    return res;
}

int fat_delete_file(const char* path) {
    if (path[0] != '/') return -1;

    uint32 parent_clu = fat_get_parent_cluster(path);
    if (parent_clu == (uint32)-1) return -1;

    const char* filename = fat_get_last_segment(path);
    char sfn[11];
    if (filename_to_sfn(filename, sfn) != 0) return -1;

    uint32 dir_clu = parent_clu;
    uint32 cluster_size = g_fat_bpb->sectors_per_cluster * g_fat_bpb->bytes_per_sector;

    while (dir_clu >= 2 && dir_clu < 0x0FFFFFF8) {
        uint8* buf = load_cluster(dir_clu);
        for (uint32 off = 0; off < cluster_size; off += 32) {
            if (buf[off] != 0x00 && buf[off] != 0xE5 && 
                memcmp(buf + off, (uint8*)sfn, 11) == 0) {
                
                FAT_dirent* de = (FAT_dirent*)(buf + off);
                uint32 file_clu = (de->FirstClusterHigh << 16) | de->FirstClusterLow;
                
                // free chain
                while (file_clu >= 2 && file_clu < 0x0FFFFFF8) {
                    uint32 next = fat_get_next_clu(file_clu);
                    fat_set_entry(file_clu, 0);
                    file_clu = next;
                }

                buf[off] = 0xE5;
                ide_write_sectors(DRIVE, g_fat_bpb->sectors_per_cluster, FAT_ClusterToLba(dir_clu), buf);
                kfree(buf);
                return 0;
            }
        }
        kfree(buf);
        dir_clu = fat_get_next_clu(dir_clu);
    }
    return -1;
}