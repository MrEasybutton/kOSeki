#include "../lib_include.h"
#include "../../input.h"
#include "../../graphics.h"
#include "../../filesystem.h"
#include "../../text_input.h"
#include "../../utilities.h"
#include <stdbool.h>

#define MAX_FILES 32
#define WIN_X         0
#define WIN_Y         1
#define WIN_WIDTH     2
#define WIN_HEIGHT    3

typedef struct {
    char filename[13];
    uint32_t size;
    uint8_t attributes;
} FileEntry;

typedef struct {
    FileEntry files[MAX_FILES];
    int file_count;
    int selected_index;
    int scroll_offset;
} CaseFilesState;

static CaseFilesState browser = {0};
static TextInputField* new_filename_input = 0;
static TextInputField* rename_input = 0;

void format_size(uint32_t size, char* buffer) {
    char temp[15];
    int idx = 0;

    if (size == 0) {
        temp[idx++] = '0';
    } else {
        while (size > 0) {
            temp[idx++] = '0' + (size % 10);
            size /= 10;
        }
    }
    int out_idx = 0;
    for (int i = 0; i < idx; i++) {
        if (i > 0 && i % 3 == 0) {
            buffer[out_idx++] = ',';
        }
        buffer[out_idx++] = temp[idx - 1 - i];
    }
    
    buffer[out_idx] = '\0';
}

int refresh_file_list() {
    browser.file_count = 0;
    
    FAT_BPB bpb;
    uint8_t sector_buffer[512];

    if (disk_read_sector(0, sector_buffer) != 0) return -1;

    for (int i = 0; i < sizeof(FAT_BPB); i++) {
        ((uint8_t*)&bpb)[i] = sector_buffer[i];
    }

    uint32_t fat_start_sector = bpb.reserved_sectors;
    uint32_t root_dir_start_sector = fat_start_sector + (bpb.fat_count * bpb.sectors_per_fat);
    uint16_t root_dir_entries = bpb.root_entry_count;

    int entries_per_sector = 512 / sizeof(FAT_DirectoryEntry);
    int sectors_needed = (root_dir_entries + entries_per_sector - 1) / entries_per_sector;
    
    for (int sector_idx = 0; sector_idx < sectors_needed; sector_idx++) {
        if (disk_read_sector(root_dir_start_sector + sector_idx, sector_buffer) != 0) return -1;
        
        FAT_DirectoryEntry* entries = (FAT_DirectoryEntry*)sector_buffer;
        
        for (int entry_idx = 0; entry_idx < entries_per_sector; entry_idx++) {
            FAT_DirectoryEntry* entry = &entries[entry_idx];
            
            if (entry->filename[0] == 0) break;
            if (entry->filename[0] == 0xE5) continue;
            if ((entry->attributes & FAT_ATTR_VOLUME_ID) || (entry->attributes & FAT_ATTR_DIRECTORY)) continue;
            
            if (browser.file_count < MAX_FILES) {
                int name_len = 0;
                for (int i = 0; i < 8 && entry->filename[i] != ' '; i++) {
                    browser.files[browser.file_count].filename[name_len++] = entry->filename[i];
                }

                if (entry->extension[0] != ' ') {
                    browser.files[browser.file_count].filename[name_len++] = '.';
                    for (int i = 0; i < 3 && entry->extension[i] != ' '; i++) {
                        browser.files[browser.file_count].filename[name_len++] = entry->extension[i];
                    }
                }
                
                browser.files[browser.file_count].filename[name_len] = '\0';
                browser.files[browser.file_count].size = entry->file_size;
                browser.files[browser.file_count].attributes = entry->attributes;
                browser.file_count++;
            }
        }
    }
    
    return 0;
}

void get_attribute_string(uint8_t attributes, char* buffer) {
    buffer[0] = (attributes & FAT_ATTR_READ_ONLY) ? 'R' : '-';
    buffer[1] = (attributes & FAT_ATTR_HIDDEN) ? 'H' : '-';
    buffer[2] = (attributes & FAT_ATTR_SYSTEM) ? 'S' : '-';
    buffer[3] = (attributes & FAT_ATTR_ARCHIVE) ? 'A' : '-';
    buffer[4] = '\0';
}

int delete_selected_file() {
    if (browser.selected_index >= 0 && browser.selected_index < browser.file_count) {
        int result = fs_delete(browser.files[browser.selected_index].filename);
        if (result == 0) {
            refresh_file_list();
            if (browser.selected_index >= browser.file_count && browser.file_count > 0) {
                browser.selected_index = browser.file_count - 1;
            }
            return 0;
        }
        return result;
    }
    return -1;
}

int create_new_file(const char* filename) {
    int fd = fs_open(filename, FS_MODE_WRITE | FS_MODE_CREATE);
    if (fd < 0) {
        return -1;
    }
    
    const char* initial_content = "shiorin or something";
    fs_write(fd, initial_content, 31);
    fs_close(fd);
    
    refresh_file_list();
    
    return 0;
}

int rename_file(const char* old_filename, const char* new_filename) {
    // ok i just copied the old file contents, if it works it works
    int src_fd = fs_open(old_filename, FS_MODE_READ);
    if (src_fd < 0) {
        return -1;
    }

    int dst_fd = fs_open(new_filename, FS_MODE_WRITE | FS_MODE_CREATE);
    if (dst_fd < 0) {
        fs_close(src_fd);
        return -2;
    }

    char buffer[512];
    int bytes_read;
    while ((bytes_read = fs_read(src_fd, buffer, 512)) > 0) {
        fs_write(dst_fd, buffer, bytes_read);
    }

    fs_close(src_fd);
    fs_close(dst_fd);
    fs_delete(old_filename);

    refresh_file_list();
    
    return 0;
}

int CaseFiles(int process_inst) {
    int *params = &iparams[process_inst * procparamlen];

    static int initialized = 0;
    static int refresh_timer = 0;
    
    int closeClicked = DrawWindow(
        &params[WIN_X], &params[WIN_Y], &params[WIN_WIDTH], &params[WIN_HEIGHT],
        50, 50, 70, &params[9], process_inst
    );

    if (closeClicked) CloseProcess(process_inst);

    char title[] = "Watson's Case Files";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             title, params[WIN_X] + 5, params[WIN_Y], 0, 0, 0);

    if (!initialized) {
        refresh_file_list();
        initialized = 1;
    }
    
    refresh_timer++;
    if (refresh_timer > 100) {
        refresh_file_list();
        refresh_timer = 0;
    }

    static int last_click_time = 0;
    static int last_clicked_idx = -1;
    
    if (left_clicked) {
        int file_area_y = params[WIN_Y] + 30;
        int file_area_height = params[WIN_HEIGHT] - 100;
        int item_height = 20;
        int max_visible_items = file_area_height / item_height;
        
        if (mx >= params[WIN_X] + 10 && mx <= params[WIN_X] + params[WIN_WIDTH] - 10 &&
            my >= file_area_y && my < file_area_y + file_area_height) {
            
            int clicked_idx = (my - file_area_y) / item_height + browser.scroll_offset;
            if (clicked_idx >= 0 && clicked_idx < browser.file_count) {
                int current_time = refresh_timer;

                last_click_time = current_time;
                last_clicked_idx = clicked_idx;
                browser.selected_index = clicked_idx;
            }
        }
    }

    DrawRect(
        params[WIN_X] + 10,
        params[WIN_Y] + 30,
        params[WIN_WIDTH] - 20,
        params[WIN_HEIGHT] - 100,
        201, 147, 81
    );

    int file_area_height = params[WIN_HEIGHT] - 100;
    int item_height = 20;
    int max_visible_items = file_area_height / item_height;

    if (browser.selected_index < browser.scroll_offset) {
        browser.scroll_offset = browser.selected_index;
    } else if (browser.selected_index >= browser.scroll_offset + max_visible_items) {
        browser.scroll_offset = browser.selected_index - max_visible_items + 1;
    }
    
    if (browser.scroll_offset < 0) {
        browser.scroll_offset = 0;
    }

    for (int i = 0; i < max_visible_items && i + browser.scroll_offset < browser.file_count; i++) {
        int file_idx = i + browser.scroll_offset;
        int y_pos = params[WIN_Y] + 30 + (i * item_height);
        
        if (file_idx == browser.selected_index) {
            DrawRect(
                params[WIN_X] + 10,
                y_pos,
                params[WIN_WIDTH] - 20,
                item_height,
                253, 228, 148
            );
            DrawRect(
                params[WIN_X] + 10,
                y_pos,
                params[WIN_WIDTH] - 22,
                item_height - 2,
                201, 147, 81
            );
        }
        
        DrawText(getFontCharacter, font_font_width, font_font_height, 
                browser.files[file_idx].filename, 
                params[WIN_X] + 15, 
                y_pos + 5, 
                240, 240, 240);

        char size_buffer[20];
        format_size(browser.files[file_idx].size, size_buffer);
        
        DrawText(getFontCharacter, font_font_width, font_font_height, 
                size_buffer, 
                params[WIN_X] + params[WIN_WIDTH] - 150, 
                y_pos + 5, 
                220, 220, 240);

        char attr_buffer[5];
        get_attribute_string(browser.files[file_idx].attributes, attr_buffer);
        
        DrawText(getFontCharacter, font_font_width, font_font_height, 
                attr_buffer, 
                params[WIN_X] + params[WIN_WIDTH] - 70, 
                y_pos + 5, 
                200, 200, 220);
    }

    DrawRect(
        params[WIN_X] + 10,
        params[WIN_Y] + params[WIN_HEIGHT] - 80,
        params[WIN_WIDTH] - 20,
        25,
        201, 147, 81
    );

    char info_text[30];
    int j = 0;

    int temp_count = browser.file_count;
    if (temp_count == 0) {
        info_text[j++] = '0';
    } else {
        char temp[10];
        int temp_idx = 0;
        
        while (temp_count > 0) {
            temp[temp_idx++] = '0' + (temp_count % 10);
            temp_count /= 10;
        }
        
        while (temp_idx > 0) {
            info_text[j++] = temp[--temp_idx];
        }
    }

    info_text[j++] = ' ';
    info_text[j++] = 'f';
    info_text[j++] = 'i';
    info_text[j++] = 'l';
    info_text[j++] = 'e';
    info_text[j++] = 's';
    info_text[j++] = '\0';
    
    DrawText(getFontCharacter, font_font_width, font_font_height, 
            info_text, 
            params[WIN_X] + 15, 
            params[WIN_Y] + params[WIN_HEIGHT] - 52, 
            220, 220, 240);

    DrawRect(
        params[WIN_X] + 5,
        params[WIN_Y] + params[WIN_HEIGHT] - 30,
        params[WIN_WIDTH] - 8,
        40,
        201, 147, 81
    );

    char refreshText[] = "REFRESH";
    if (DrawButton(
            params[WIN_X] + 10, 
            params[WIN_Y] + params[WIN_HEIGHT] - 25, 
            80, 30, 
            53, 28, 48,
            refreshText, 220, 220, 240, 
            process_inst, 201, 147, 81) == TRUE) {
        refresh_file_list();
    }

    char deleteText[] = "SHRED";
    if (DrawButton(
            params[WIN_X] + 100, 
            params[WIN_Y] + params[WIN_HEIGHT] - 25, 
            80, 30, 
            53, 28, 48,
            deleteText, 220, 220, 240, 
            process_inst, 201, 147, 81) == TRUE) {
        if (browser.selected_index >= 0 && browser.selected_index < browser.file_count) {
            delete_selected_file();
        }
    }

    if (browser.selected_index >= 0 && browser.selected_index < browser.file_count) {
        char* selected_filename = browser.files[browser.selected_index].filename;

        if (!rename_input) {
            rename_input = register_text_input_field(
                process_inst, 2,
                params[WIN_X] + 10, params[WIN_Y] + params[WIN_HEIGHT] - 105,
                params[WIN_WIDTH] - 120, 20,
                240, 240, 240,
                201, 147, 81
            );

            set_text_input_value(rename_input, selected_filename);
        } else {
            rename_input->input_x = params[WIN_X] + 10;
            rename_input->input_y = params[WIN_Y] + params[WIN_HEIGHT] - 105;
            rename_input->input_width = params[WIN_WIDTH] - 120;

            if (str_compare(rename_input->buffer, selected_filename) != 0) {
                set_text_input_value(rename_input, selected_filename);
            }
        }
    }

    return 0;
}