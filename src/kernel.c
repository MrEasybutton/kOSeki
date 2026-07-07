#include "kernel.h"
#include "console.h"
#include "string.h"
#include "gdt.h"
#include "idt.h"
#include "keyboard.h"
#include "bae.h"
#include "multiboot.h"
#include "graphics.h"
#include "ide.h"
#include "pmm.h"
#include "kheap.h"
#include "vesa.h"
#include "bitmap.h"
#include "fat32.h"
#include "bmp.h"
#include "video.h"
#include "gui.h"
#include "ac97.h"
#include "apps/CLPlayer.h"
#include "synth.h"
#include "fat32.h"
#include "speaker.h"
#include "cmos.h"
#include "isr.h"
#include "kronii.h"
#include "pci.h"
#include "e1000.h"
#include "net.h"
#include "utils.h"
#include "procsys.h"
#include "timer.h"
#include "cmos.h"
#include "serial.h"
#include "lwip/timeouts.h"
#include "kmath.h"
#include "string.h"
#include "apps/Pebbleshell.h"
#include "apps/Reaper.h"
#include "apps/WAHtercolour.h"
#include "apps/Novella.h"
#include "apps/CLStudio.h"
#include "baux2/baux2.h"

#define PROMPT_TEXT "$BIBOO > "

int CLOCK_X = 880;
int CLOCK_Y = 3;

KERNEL_MEMORY_MAP g_kmap;
uint8 g_video_mode = VIDEO_MODE_TEXT;

extern char* fat_read_file(char* fpath);

int atoi(const char* str) {
    int result = 0;
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] < '0' || str[i] > '9') break;
        result = result * 10 + (str[i] - '0');
    }
    return result;
}

uint32 g_current_dir_cluster = 2;
char g_current_path[256] = "/";

char* g_wallpaper_path = "SYSTEM/bliss-wllp.bmp";
char* g_wallpaper_bmp_base_data = NULL;
char* g_wllp_data = NULL;
int g_wllp_width = 0;
int g_wllp_height = 0;
int g_wallpaper_row_padded = 0;

static uint32* g_wallpaper_cache = NULL;
static int g_wallpaper_cache_width = 0;
static int g_wallpaper_cache_height = 0;
static BOOL g_wallpaper_cache_valid = FALSE;

static void draw_bootfrm(const char* title, const char* message) {
    rect_grad(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, RGB(186, 158, 224), RGB(240, 182, 224));

    rect_grad(0, 0, SCREEN_WIDTH, 36, RGB(118, 90, 168), RGB(168, 132, 198));
    rect_grad(0, 0, SCREEN_WIDTH, 16, RGB(255, 250, 255), RGB(196, 168, 220));
    rect(0, 36, SCREEN_WIDTH, 1, RGB(240, 232, 250));

    int box_x = (SCREEN_WIDTH - 360) / 2;
    int box_y = (SCREEN_HEIGHT - 140) / 2;
    int box_w = 360;
    int box_h = 140;
    int pad_x = 16;
    int text_x = box_x + pad_x;

    rect(box_x + 4, box_y + 6, box_w, box_h, RGB(150, 130, 176));

    rect(box_x, box_y, box_w, box_h, RGB(255, 255, 255));
    rect(box_x + 2, box_y + 2, box_w - 4, box_h - 4, RGB(214, 198, 236));
    rect(box_x + 6, box_y + 6, box_w - 12, box_h - 12, RGB(247, 242, 255));

    rect_grad(box_x + 6, box_y + 6, box_w - 12, (box_h - 12) * 2 / 5, RGB(255, 255, 255), RGB(247, 242, 255));

    rect(box_x + 6, box_y + 6, box_w - 12, 2, RGB(255, 255, 255));

    if (title)
        text_ex(title, text_x, box_y + 12, RGB(74, 62, 104), FONT_DEFAULT, TRUE, 2);

    if (message) {
        char l1[96] = {0}, l2[96] = {0};

        if (strlen(message) > 34) {
            int split = 34;
            while (split > 0 && message[split] != ' ')
                split--;

            if (split > 0) {
                memcpy(l1, message, split);
                l1[split] = 0;
                strcpy(l2, message + split + 1);

                text_ex(l1, text_x, box_y + 50, RGB(94, 80, 120), FONT_KALNIA, FALSE, 1);
                text_ex(l2, text_x, box_y + 64, RGB(94, 80, 120), FONT_KALNIA, FALSE, 1);
            } else {
                text_ex(message, text_x, box_y + 57, RGB(94, 80, 120), FONT_KALNIA, FALSE, 1);
            }
        } else text_ex(message, text_x, box_y + 57, RGB(94, 80, 120), FONT_KALNIA, FALSE, 1);
    }

    text_ex("BIBOO is preparing...", text_x, box_y + 88, RGB(122, 108, 152), FONT_DEFAULT, TRUE, 1);
    text_ex("please wait warmly", text_x, box_y + 106, RGB(122, 108, 152), FONT_DEFAULT, TRUE, 1);

    swapbuf();
}

static void splash(const char* message) {
    if (vesa_init(SCREEN_WIDTH, SCREEN_HEIGHT, 32) < 0) return;
    draw_bootfrm("kOSeki", message ? message : "BOOTING...");
}

void set_text_mode() {
    g_video_mode = VIDEO_MODE_TEXT;
    console_init(COLOR_BRIGHT_MAGENTA, COLOR_BLACK);
    printf("text. \n");
}

void placeholdact() {
    printf("icon clicked\n");
}

void delay(uint32 value) {
    for (volatile uint32 i = 0; i < value;) {
        __asm__ __volatile__ (
            "nop\n"
        );
        i++;
    }
}

// really need a better way to handle this, adding registry or smth in v3.1
extern void launch_preferences();
extern void launch_casefiles();
extern void launch_SBG();
extern void launch_clp();
extern void launch_doom();
extern void launch_gawrculator();
extern void launch_baetracer();

void launch_novella_df() { launch_novella(NULL, 0); }
void launch_wahtercolour_df() { launch_wahtercolour(NULL); }
void launch_cls_df() { launch_cls(NULL); }

IconDef icons[] = {
    {"SYSTEM/prf-ic.bmp", launch_preferences},
    {"SYSTEM/peb-ic.bmp", launch_pbsh},
    {"SYSTEM/iry-ic.bmp", launch_cls_df},
    {"SYSTEM/ame-ic.bmp", launch_casefiles},
    {"SYSTEM/nov-ic.bmp", launch_novella_df},
    {"SYSTEM/wah-ic.bmp", launch_wahtercolour_df},
    {"SYSTEM/ggq-ic.bmp", launch_SBG},
    {"SYSTEM/rao-ic.bmp", launch_doom},
    {"SYSTEM/mor-ic.bmp", launch_reaper},
    {"SYSTEM/gur-ic.bmp", launch_gawrculator},
    {"SYSTEM/bae-ic.bmp", launch_baetracer}
};

const int icon_count = sizeof(icons) / sizeof(icons[0]);

int get_mm(KERNEL_MEMORY_MAP *kmap, MULTIBOOT_INFO *mboot_info) {
    uint32 i;

    if (kmap == NULL) return -1;
    kmap->kernel.k_start_addr = (uint32)&__kernel_section_start;
    kmap->kernel.k_end_addr = (uint32)&__kernel_section_end;
    kmap->kernel.k_len = ((uint32)&__kernel_section_end - (uint32)&__kernel_section_start);

    kmap->kernel.text_start_addr = (uint32)&__kernel_text_section_start;
    kmap->kernel.text_end_addr = (uint32)&__kernel_text_section_end;
    kmap->kernel.text_len = ((uint32)&__kernel_text_section_end - (uint32)&__kernel_text_section_start);

    kmap->kernel.data_start_addr = (uint32)&__kernel_data_section_start;
    kmap->kernel.data_end_addr = (uint32)&__kernel_data_section_end;
    kmap->kernel.data_len = ((uint32)&__kernel_data_section_end - (uint32)&__kernel_data_section_start);

    kmap->kernel.rodata_start_addr = (uint32)&__kernel_rodata_section_start;
    kmap->kernel.rodata_end_addr = (uint32)&__kernel_rodata_section_end;
    kmap->kernel.rodata_len = ((uint32)&__kernel_rodata_section_end - (uint32)&__kernel_rodata_section_start);

    kmap->kernel.bss_start_addr = (uint32)&__kernel_bss_section_start;
    kmap->kernel.bss_end_addr = (uint32)&__kernel_bss_section_end;
    kmap->kernel.bss_len = ((uint32)&__kernel_bss_section_end - (uint32)&__kernel_bss_section_start);

    kmap->system.total_memory = mboot_info->mem_low + mboot_info->mem_high;

    for (i = 0; i < mboot_info->mmap_length; i += sizeof(MULTIBOOT_MEMORY_MAP)) {
        MULTIBOOT_MEMORY_MAP *mmap = (MULTIBOOT_MEMORY_MAP *)(mboot_info->mmap_addr + i);
        if (mmap->type != MULTIBOOT_MEMORY_AVAILABLE) continue;
        if (mmap->addr_low == kmap->kernel.text_start_addr) {
            kmap->available.start_addr = kmap->kernel.k_end_addr + 1024 * 1024;
            kmap->available.end_addr = mmap->addr_low + mmap->len_low;
            kmap->available.size = kmap->available.end_addr - kmap->available.start_addr;
            return 0;
        }
    }

    return -1;
}

void report_mm(KERNEL_MEMORY_MAP *kmap) {
    printf("kernel:\n");
    printf("kernel-start: 0x%x, kernel-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.k_start_addr, kmap->kernel.k_end_addr, kmap->kernel.k_len);
    printf("text-start: 0x%x, text-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.text_start_addr, kmap->kernel.text_end_addr, kmap->kernel.text_len);
    printf("data-start: 0x%x, data-end: 0x%x, TOTAL: %d bytes\n",
           kmap->kernel.data_start_addr, kmap->kernel.data_end_addr, kmap->kernel.data_len);
    printf("rodata-start: 0x%x, rodata-end: 0x%x, TOTAL: %d\n",
           kmap->kernel.rodata_start_addr, kmap->kernel.rodata_end_addr, kmap->kernel.rodata_len);
    printf("bss-start: 0x%x, bss-end: 0x%x, TOTAL: %d\n",
           kmap->kernel.bss_start_addr, kmap->kernel.bss_end_addr, kmap->kernel.bss_len);

    printf("total_memory: %d KB\n", kmap->system.total_memory);
    printf("available:\n");
    printf("start_adddr: 0x%x\n end_addr: 0x%x\n size: %d\n",
           kmap->available.start_addr, kmap->available.end_addr, kmap->available.size);
}

BOOL is_cd(char *b) {
    if((b[0]=='c')&&(b[1]=='d'))
        if(b[2]==' '||b[2]=='\0')
            return TRUE;
    return FALSE;
}

BOOL is_cat(char *b) {
    if((b[0]=='c')&&(b[1]=='a')&&(b[2]=='t'))
        if(b[3]==' '||b[3]=='\0')
            return TRUE;
    return FALSE;
}

void trim_pbsh_buf(char* pbsh_buf, int* pbsh_len) {
    if (*pbsh_len > (MAX_SHBUF_SIZE * 3) / 4) {
        int keep_from = *pbsh_len / 2;
        
        for (int i = keep_from; i < *pbsh_len; i++) {
            if (pbsh_buf[i] == '\n') {
                keep_from = i + 1;
                break;
            }
        }
        
        int new_len = *pbsh_len - keep_from;
        for (int i = 0; i < new_len; i++) {
            pbsh_buf[i] = pbsh_buf[keep_from + i];
        }
        pbsh_buf[new_len] = '\0';
        *pbsh_len = new_len;
    }
}

void pbsh_buf_print(char* pbsh_buf, int* pbsh_len, const char* str) {
    int str_len = strlen(str);
    if (*pbsh_len + str_len < MAX_SHBUF_SIZE - 1) {
        strcpy(pbsh_buf + *pbsh_len, str);
        *pbsh_len += str_len;
        pbsh_buf[*pbsh_len] = '\0';
    }
}

void proc_bx2(char* filename, char* pbsh_buf, int* pbsh_len) {
    char path[256] = "/";
    strcat(path, filename);

    char* content = fat_read_file(path);
    if (!content) {
        pbsh_buf_print(pbsh_buf, pbsh_len, "Error: Could not read file '");
        pbsh_buf_print(pbsh_buf, pbsh_len, filename);
        pbsh_buf_print(pbsh_buf, pbsh_len, "'.\n");
        return;
    }

    Process* p = create_process(filename);
    if (!p) {
        pbsh_buf_print(pbsh_buf, pbsh_len, "Error: Could not create process for script.\n");
        kfree(content);
        return;
    }

    BAUx2_d* baux2_data = (BAUx2_d*)kmalloc(sizeof(BAUx2_d));
    memset(baux2_data, 0, sizeof(BAUx2_d));
    p->data = baux2_data;

    // save then swaps
    pbshPrint old_conx = g_pbsh_print;
    baux2_print_t old_handler = baux2_print_handler;
    extern int g_baux2_curr_pid;
    int old_pid = g_baux2_curr_pid;

    g_pbsh_print = (pbshPrint){ pbsh_buf, pbsh_len };
    baux2_print_handler = pbsh_bx2_print;
    g_baux2_curr_pid = p->pid;

    baux2_run(content);

    //restore
    baux2_print_handler = old_handler;
    g_baux2_curr_pid = old_pid;
    g_pbsh_print = old_conx;

    if (!p->window_exists) {
        baux2_cleanup_process(p);
        cleanup_process(p->pid);
    }
}

// rename the helps

void cmd(char* command_line, char* pbsh_buf, int* pbsh_len) {
    char* cmd_copy = (char*)malloc(strlen(command_line) + 1);
    if (!cmd_copy) {
        pbsh_buf_print(pbsh_buf, pbsh_len, "\nError: Malloc failed.\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, PROMPT_TEXT);
        return;
    }
    strcpy(cmd_copy, command_line);

    char* trimmed_cmd = cmd_copy;
    while (*trimmed_cmd == ' ' || *trimmed_cmd == '\t') { trimmed_cmd++; }

    char* end = trimmed_cmd + strlen(trimmed_cmd) - 1;
    while (end > trimmed_cmd && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        end--;
    }
    *(end + 1) = '\0';


    if (*trimmed_cmd == '\0') {
        kfree(cmd_copy);
        pbsh_buf_print(pbsh_buf, pbsh_len, PROMPT_TEXT);
        return;
    }

    char* command = strtok(trimmed_cmd, " \t");
    kprint("[CMD] received: '%s'\n", command);

    if (strcmp(command, "beep") == 0) {
        char* arg = strtok(NULL, "");
        if (arg) {
            while (*arg == ' ' || *arg == '\t') arg++;
        }
        
        if (arg && *arg != '\0') {
            pbsh_buf_print(pbsh_buf, pbsh_len, arg);
            pbsh_buf_print(pbsh_buf, pbsh_len, "\n");
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: beep {text}\n");
        }
    } 
    else if (strcmp(command, "clear") == 0) {
        *pbsh_len = 0;
        pbsh_buf[0] = '\0';
    }
    else if (strcmp(command, "ls") == 0) {
        char* output = get_ls("/");
        if (output) {
            pbsh_buf_print(pbsh_buf, pbsh_len, output);
            kfree(output);
        }
    }
    else if (strcmp(command, "baux2") == 0 || strcmp(command, "BAUx2") == 0) {
        char* filename = strtok(NULL, " \t");
        if (filename) {
            proc_bx2(filename, pbsh_buf, pbsh_len);
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: baux2 {file}\n");
        }
    }
    else if (strcmp(command, "fetch") == 0) {
        char* host = strtok(NULL, " \t");
        if (host) {
            extern void net_fetch(const char* host, char* pbsh_buf, int* pbsh_len);
            net_fetch(host, pbsh_buf, pbsh_len);
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: fetch {host}\n");
        }
    }
    else if (strcmp(command, "johncat") == 0) {
        char* filename = strtok(NULL, "");
        if (filename) {
            while (*filename == ' ' || *filename == '\t') filename++;
            
            if (*filename != '\0') {
                char path[256];
                if (filename[0] != '/') {
                    strcpy(path, "/");
                    strcat(path, filename);
                } else {
                    strcpy(path, filename);
                }

                char* file_content = fat_read_file(path);
                if (file_content) {
                    FAT_dirent* dirent = fat_find_file(path);
                    if (dirent) {
                        uint32 file_size = dirent->Size;
                        kfree(dirent);
                        
                        char* display_buffer = (char*)malloc(file_size + 1);
                        if (display_buffer) {
                            memcpy(display_buffer, file_content, file_size);
                            display_buffer[file_size] = '\0';
                            pbsh_buf_print(pbsh_buf, pbsh_len, display_buffer);
                            pbsh_buf_print(pbsh_buf, pbsh_len, "\n");
                            kfree(display_buffer);
                        }
                        kfree(file_content);
                    } else {
                        kfree(file_content);
                        pbsh_buf_print(pbsh_buf, pbsh_len, "There was an error reading the file.\n");
                    }
                } else {
                    pbsh_buf_print(pbsh_buf, pbsh_len, "Couldn't find ");
                    pbsh_buf_print(pbsh_buf, pbsh_len, filename);
                    pbsh_buf_print(pbsh_buf, pbsh_len, ", or there was an error reading it.\n");
                }
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: johncat {file}\n");
            }
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: johncat {file}\n");
        }
    }
    
    else if (strcmp(command, "rock") == 0) {
        char* filename = strtok(NULL, "");
        if (filename) {
            while (*filename == ' ' || *filename == '\t') filename++;
            
            if (*filename != '\0') {
                char path[256];
                if (filename[0] != '/') {
                    strcpy(path, "/");
                    strcat(path, filename);
                } else {
                    strcpy(path, filename);
                }
                
                int result = fat_create_file(path);
                if (result == 0) {
                    pbsh_buf_print(pbsh_buf, pbsh_len, "created ");
                    pbsh_buf_print(pbsh_buf, pbsh_len, filename);
                    pbsh_buf_print(pbsh_buf, pbsh_len, " successfully! :D\n");
                } else {
                    pbsh_buf_print(pbsh_buf, pbsh_len, "Couldn't create that file (it may already exist).\n");
                }
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: rock {file}\n");
            }
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: rock {file}\n");
        }
    }

    else if (strcmp(command, "stone") == 0) {
        char* dirname = strtok(NULL, "");
        if (dirname) {
            while (*dirname == ' ' || *dirname == '\t') dirname++;
            if (*dirname != '\0') {
                char path[256];
                if (dirname[0] != '/') {
                    strcpy(path, "/");
                    strcat(path, dirname);
                } else {
                    strcpy(path, dirname);
                }
                
                int result = fat_create_dir(path);
                if (result == 0) {
                    pbsh_buf_print(pbsh_buf, pbsh_len, "created ");
                    pbsh_buf_print(pbsh_buf, pbsh_len, dirname);
                    pbsh_buf_print(pbsh_buf, pbsh_len, " successfully! :D\n");
                } else {
                    pbsh_buf_print(pbsh_buf, pbsh_len, "Couldn't create that folder (it may already exist).\n");
                }
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: stone {dir}\n");
            }
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: stone {dir}\n");
        }
    }
    
    else if (strcmp(command, "peb") == 0) {
        char* pebname = strtok(NULL, "");
        if (pebname) {
            while (*pebname == ' ' || *pebname == '\t') pebname++;
            
            if (*pebname != '\0') {
                char path[256];
                if (pebname[0] != '/') {
                    strcpy(path, "/");
                    strcat(path, pebname);
                } else {
                    strcpy(path, pebname);
                }

                if (strstr(path, ".pebble") == NULL && strstr(path, ".PEBBLE") == NULL) {
                    strcat(path, ".pebble");
                }
                
                int result = fat_create_dir(path);
                if (result == 0) {
                    pbsh_buf_print(pbsh_buf, pbsh_len, "A Pebble has been spawned at ");
                    pbsh_buf_print(pbsh_buf, pbsh_len, path);
                    pbsh_buf_print(pbsh_buf, pbsh_len, " successfully! :D\n");
                    
                    char subpath[512];

                    strcpy(subpath, path);
                    strcat(subpath, "/run.bx2");
                    fat_create_file(subpath);

                    strcpy(subpath, path);
                    strcat(subpath, "/manifest.jdon");
                    fat_create_file(subpath);
                    
                } else {
                    pbsh_buf_print(pbsh_buf, pbsh_len, "Couldn't create this Pebble (it may already exist).\n");
                }
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: peb {name}\n");
            }
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: peb {name}\n");
        }
    }
    
    else if (strcmp(command, "chuuni") == 0 || strcmp(command, "agent47") == 0) {
        char* old_name = strtok(NULL, " \t");
        char* new_name = strtok(NULL, "");
        
        if (new_name) {
            while (*new_name == ' ' || *new_name == '\t') new_name++;
        }
        
        if (old_name && new_name && *old_name != '\0' && *new_name != '\0') {
            char old_path[256], new_path[256];
            
            if (old_name[0] != '/') {
                strcpy(old_path, "/");
                strcat(old_path, old_name);
            } else {
                strcpy(old_path, old_name);
            }
            
            if (new_name[0] != '/') {
                strcpy(new_path, "/");
                strcat(new_path, new_name);
            } else {
                strcpy(new_path, new_name);
            }
            
            int result = fat_rename_file(old_path, new_path);
            if (result == 0) {
                pbsh_buf_print(pbsh_buf, pbsh_len, "Biboo has renamed ");
                pbsh_buf_print(pbsh_buf, pbsh_len, old_name);
                pbsh_buf_print(pbsh_buf, pbsh_len, " to ");
                pbsh_buf_print(pbsh_buf, pbsh_len, new_name);
                pbsh_buf_print(pbsh_buf, pbsh_len, ".\n");
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "Biboo couldn't rename the file.\n");
            }
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: chuuni {current} {new}\n");
        }
    }
    
    else if (strcmp(command, "shiorin") == 0) {
        char* filename = strtok(NULL, " \t");
        char* text = strtok(NULL, "");
        
        if (text) { while (*text == ' ' || *text == '\t') text++; }
        
        if (filename && text && *filename != '\0' && *text != '\0') {
            char path[256];
            if (filename[0] != '/') {
                strcpy(path, "/");
                strcat(path, filename);
            } else {
                strcpy(path, filename);
            }
            
            int result = fat_write_file(path, text, strlen(text));
            if (result == 0) {
                pbsh_buf_print(pbsh_buf, pbsh_len, "Shiori has overwritten ");
                pbsh_buf_print(pbsh_buf, pbsh_len, filename);
                pbsh_buf_print(pbsh_buf, pbsh_len, ".\n");
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "Couldn't overwrite the file (it may not exist).\n");
            }
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: shiorin {file} {text}\n");
        }
    }
    
    else if (strcmp(command, "nov") == 0) {
        char* filename = strtok(NULL, " \t");
        char* text = strtok(NULL, "");
        
        if (text) {
            while (*text == ' ' || *text == '\t') text++;
        }
        
        if (filename && text && *filename != '\0' && *text != '\0') {
            char path[256];
            if (filename[0] != '/') {
                strcpy(path, "/");
                strcat(path, filename);
            } else {
                strcpy(path, filename);
            }
            
            int result = fat_append_file(path, text, strlen(text));
            if (result == 0) {
                pbsh_buf_print(pbsh_buf, pbsh_len, "Shiori appended ");
                pbsh_buf_print(pbsh_buf, pbsh_len, text);
                pbsh_buf_print(pbsh_buf, pbsh_len, " to ");
                pbsh_buf_print(pbsh_buf, pbsh_len, filename);
                pbsh_buf_print(pbsh_buf, pbsh_len, ".\n");
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "Couldn't append to that file (it may not exist).\n");
            }
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: nov {file} {text}\n");
        }
    }
    
    else if (strcmp(command, "pickaxe") == 0 || strcmp(command, "mine") == 0) {
        char* filename = strtok(NULL, "");
        if (filename) {
            while (*filename == ' ' || *filename == '\t') filename++;
            
            if (*filename != '\0') {
                char path[256];
                if (filename[0] != '/') {
                    strcpy(path, "/");
                    strcat(path, filename);
                } else {
                    strcpy(path, filename);
                }
                
                int result = fat_delete_file(path);
                if (result == 0) {
                    pbsh_buf_print(pbsh_buf, pbsh_len, "Kaela has deleted ");
                    pbsh_buf_print(pbsh_buf, pbsh_len, filename);
                    pbsh_buf_print(pbsh_buf, pbsh_len, "! bwehhh\n");
                } else {
                    pbsh_buf_print(pbsh_buf, pbsh_len, "Couldn't delete that file (it may not exist).\n");
                }
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: pickaxe {file}\n");
            }
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: pickaxe {file}\n");
        }
    }
    
    else if (strcmp(command, "doom") == 0) {
        launch_doom();
    }
    else if (strcmp(command, "prefs") == 0) {
        launch_preferences();
    }
    else if (strcmp(command, "pbsh") == 0) {
        launch_pbsh();
    }
    else if (strcmp(command, "casefiles") == 0) {
        launch_casefiles();
    }
    else if (strcmp(command, "novella") == 0) {
        launch_novella(NULL, 0);
    }
    else if (strcmp(command, "cl_player") == 0) {
        launch_cls_df();
    }
    else if (strcmp(command, "gura") == 0) {
        launch_gawrculator();
    }
    else if (strcmp(command, "help") == 0) {
        pbsh_buf_print(pbsh_buf, pbsh_len, "COMMANDS:\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "beep > echo text to shell\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "ls > list root\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "johncat > read a file\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "rock > create a file\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "stone > create a folder\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "nov > append text to a file\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "shiorin > overwrite a file\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "chuuni/agent47 > rename file\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "pickaxe/mine > delete a file\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "peb > create a Pebble\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "clear > clear this shell\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "karaoke > play audio file\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "synth > play a synth\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "br > play with BloodRaven\n");
        pbsh_buf_print(pbsh_buf, pbsh_len, "launch an app by typing its name!\n");
    }
    else if (strcmp(command, "karaoke") == 0) {
        char* filename = strtok(NULL, "");
        if (filename) {
            while (*filename == ' ' || *filename == '\t') filename++;
            if (*filename != '\0') {
                if (fat_find_file(filename)) {
                    ac97_play(filename);
                } else {
                    pbsh_buf_print(pbsh_buf, pbsh_len, "Couldn't find ");
                    pbsh_buf_print(pbsh_buf, pbsh_len, filename);
                    pbsh_buf_print(pbsh_buf, pbsh_len, "!\n");
                }
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: karaoke {file}\n");
            }
        } else {
            pbsh_buf_print(pbsh_buf, pbsh_len, "USAGE: karaoke {file}\n");
        }
    }
    else if (strcmp(command, "synth") == 0) {
        wave_type_t wave = WAVE_SINE;
        uint32 freq_hz = 440;
        uint32 duration_ms = 300;
        uint8 volume = 70;
        uint32 sample_rate = 48000;
        BOOL got_args = FALSE;

        char* token = strtok(NULL, " \t");

        if (token && strcmp(token, "beep") == 0) {
            ac97_synth_beep(); goto synth_done;
        } else if (token && strcmp(token, "dangit") == 0) {
            ac97_synth_dangit(); goto synth_done;
        } else if (token && strcmp(token, "candyfeet") == 0) {
            ac97_synth_tink(); goto synth_done;
        } else if (token && strcmp(token, "laser") == 0) {
            ac97_synth_laser(); goto synth_done;
        }

        while (token != NULL) {
            if (strcmp(token, "--wave") == 0 || strcmp(token, "-w") == 0) {
                token = strtok(NULL, " \t");
                if (!token) break;
                if (strcmp(token, "sine") == 0) wave = WAVE_SINE;
                else if (strcmp(token, "square") == 0) wave = WAVE_SQUARE;
                else if (strcmp(token, "saw") == 0) wave = WAVE_SAW;
                else if (strcmp(token, "tri") == 0) wave = WAVE_TRIANGLE;
                else if (strcmp(token, "noise") == 0) wave = WAVE_NOISE;
                else {
                    pbsh_buf_print(pbsh_buf, pbsh_len,
                        "i don't recognise that wave\n");
                    goto synth_done;
                }
                got_args = TRUE;

            } else if (strcmp(token, "--freq") == 0 || strcmp(token, "-f") == 0) {
                token = strtok(NULL, " \t");
                if (!token) break;
                freq_hz = (uint32)atoi(token);
                got_args = TRUE;

            } else if (strcmp(token, "--dur") == 0 || strcmp(token, "-d") == 0) {
                token = strtok(NULL, " \t");
                if (!token) break;
                duration_ms = (uint32)atoi(token);
                got_args = TRUE;

            } else if (strcmp(token, "--vol") == 0 || strcmp(token, "-v") == 0) {
                token = strtok(NULL, " \t");
                if (!token) break;
                volume = (uint8)atoi(token);
                got_args = TRUE;

            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "idk what ");
                pbsh_buf_print(pbsh_buf, pbsh_len, token);
                pbsh_buf_print(pbsh_buf, pbsh_len, " is, try another flag\n");
                goto synth_done;
            }

            token = strtok(NULL, " \t");
        }

        if (!got_args) {
            pbsh_buf_print(pbsh_buf, pbsh_len,
                "USAGE: synth {demo/flags}\n"
                "demos: beep/dangit/candyfeet/laser\n"
                "flags:\n"
                "-wave/-w sine/square/saw/tri/noise\n"
                "-freq/-f <hz> (default is 440)\n"
                "-dur/-d <ms> (default is 300)\n"
                "-vol/-v <0-100>(default is 70)\n"
                "e.g. synth -w sine -f 670 -d 360 -v 67\n");
            goto synth_done;
        }

        {
            synth_buf_t* buf = ac97_synth_generate(wave, freq_hz, duration_ms, volume, sample_rate);
            if (buf) {
                ac97_play_synth(buf);
                kfree(buf);
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "synth: buffer allocation failed\n");
            }
        }

        synth_done:;
    }
    else if (strcmp(command, "br") == 0) {
        float f0_hz = 440.0f;
        vowel_t vowel = VOWEL_A;
        uint32 duration_ms = 500;
        uint8 volume = 75;
        uint32 sample_rate = 48000;
        BOOL got_args = FALSE;

        char* token = strtok(NULL, " \t");
        
        if (token && strcmp(token, "sing") == 0) {
            ac97_synth_opera(); goto opera_done;
        }

        while (token != NULL) {
            if (strcmp(token, "--freq") == 0 || strcmp(token, "-f") == 0) {
                token = strtok(NULL, " \t");
                if (!token) break;
                f0_hz = (float)atoi(token);
                got_args = TRUE;

            } else if (strcmp(token, "--vowel") == 0 || strcmp(token, "-V") == 0) {
                token = strtok(NULL, " \t");
                if (!token) break;
                if (strcmp(token, "a") == 0) vowel = VOWEL_A;
                else if (strcmp(token, "e") == 0) vowel = VOWEL_E;
                else if (strcmp(token, "i") == 0) vowel = VOWEL_I;
                else if (strcmp(token, "o") == 0) vowel = VOWEL_O;
                else if (strcmp(token, "u") == 0) vowel = VOWEL_U;
                else {
                    pbsh_buf_print(pbsh_buf, pbsh_len,
                        "that aint a vowel dawg. pick again: a e i o u\n");
                    goto opera_done;
                }
                got_args = TRUE;

            } else if (strcmp(token, "--dur") == 0 || strcmp(token, "-d") == 0) {
                token = strtok(NULL, " \t");
                if (!token) break;
                duration_ms = (uint32)atoi(token);
                got_args = TRUE;

            } else if (strcmp(token, "--vol") == 0 || strcmp(token, "-v") == 0) {
                token = strtok(NULL, " \t");
                if (!token) break;
                volume = (uint8)atoi(token);
                got_args = TRUE;

            } else if (strcmp(token, "--rate") == 0 || strcmp(token, "-r") == 0) {
                token = strtok(NULL, " \t");
                if (!token) break;
                sample_rate = (uint32)atoi(token);
                got_args = TRUE;

            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "I don't recognise ");
                pbsh_buf_print(pbsh_buf, pbsh_len, token);
                pbsh_buf_print(pbsh_buf, pbsh_len, " as a flag.\n");
                goto opera_done;
            }
            token = strtok(NULL, " \t");
        }

        if (!got_args) {
            pbsh_buf_print(pbsh_buf, pbsh_len,
                "USAGE: br { sing / (flags) }\n"
                "flags:\n" 
                "-freq/-f <hz> (default is 440)\n"
                "-vowel/-V a/e/i/o/u (default is a)\n"
                "-dur/-d <ms> (default is 500)\n"
                "-vol/-v <0-100> (default is 75)\n"
                "-rate/-r <hz> (default is 48000)\n"
                "e.g. br -f 392 -V o -d 800\n");
            goto opera_done;
        }

        {
            synth_buf_t* buf = ac97_synth_opera_note(f0_hz, vowel, duration_ms, volume, sample_rate);
            if (buf) {
                ac97_play_synth(buf);
                ac97_synth_free(buf);
            } else {
                pbsh_buf_print(pbsh_buf, pbsh_len, "opera buffer allocation failed\n");
            }
        }

        opera_done:;
    }
    else if (strcmp(command, "\n") == 0) {
        //pbsh_buf_print(pbsh_buf, pbsh_len, "\n");
    }
    else {
        pbsh_buf_print(pbsh_buf, pbsh_len, command);
        pbsh_buf_print(pbsh_buf, pbsh_len, ": command not found");
        pbsh_buf_print(pbsh_buf, pbsh_len, "\nType 'help' for cmds\n");
    }

    kfree(cmd_copy);
    trim_pbsh_buf(pbsh_buf, pbsh_len);
    pbsh_buf_print(pbsh_buf, pbsh_len, PROMPT_TEXT);
}

void wllp_rendcache(void) {
    int screen_width = vbe_get_width();
    int screen_height = vbe_get_height();
    
    if (g_wallpaper_cache_valid && g_wallpaper_cache) {
        uint32* dst = g_back_buffer;
        uint32* src = g_wallpaper_cache;
        
        int pixels = screen_width * screen_height;
        memcpy(dst, src, pixels * sizeof(uint32));
        return;
    }
    
    if (!g_wllp_data) {
        rect_grad(0, 0, screen_width, screen_height, 
                  RGB(222, 200, 219), RGB(227, 161, 218));
        return;
    }
    
    // this should not happen
    rect_grad(0, 0, screen_width, screen_height, RGB(222, 200, 219), RGB(227, 161, 218));
}

void wllp_asmcache(void) {
    int screen_width = vbe_get_width();
    int screen_height = vbe_get_height();
    
    if (g_wallpaper_cache) {
        kfree(g_wallpaper_cache);
        g_wallpaper_cache = NULL;
    }
    
    g_wallpaper_cache_valid = FALSE;
    
    int cache_size = screen_width * screen_height * sizeof(uint32);
    g_wallpaper_cache = (uint32*)kmalloc(cache_size);
    
    if (!g_wallpaper_cache) {
        kprint("ERROR: Failed to allocate wallpaper cache!\n");
        return;
    }
    
    g_wallpaper_cache_width = screen_width;
    g_wallpaper_cache_height = screen_height;
    
    if (g_wllp_data) {
        int dst_w = 1024;
        int dst_h = 788;
        int offset_y = -20;
        
        bool bottom_up = g_wllp_height > 0;
        int abs_src_h = bottom_up ? g_wllp_height : -g_wllp_height;
        
        int scale_x = (g_wllp_width << 16) / dst_w;
        int scale_y = (abs_src_h << 16) / dst_h;
        
        for (int dy = 0; dy < dst_h; dy++) {
            int screen_y = offset_y + dy;
            if (screen_y < 0 || screen_y >= screen_height) continue;
            
            int sy = (dy * scale_y) >> 16;
            if (sy >= abs_src_h) sy = abs_src_h - 1;
            
            int row_index = bottom_up ? (abs_src_h - 1 - sy) : sy;
            char* src_row = g_wllp_data + row_index * g_wallpaper_row_padded;
            
            uint32* cache_row = g_wallpaper_cache + screen_y * screen_width;
            
            for (int dx = 0; dx < dst_w && dx < screen_width; dx++) {
                int sx = (dx * scale_x) >> 16;
                if (sx >= g_wllp_width) sx = g_wllp_width - 1;
                
                uint8 b = src_row[sx * 3 + 0];
                uint8 g = src_row[sx * 3 + 1];
                uint8 r = src_row[sx * 3 + 2];
                
                cache_row[dx] = RGB(r, g, b);
            }
        }
    } else {
        uint32 color1 = RGB(222, 200, 219);
        uint32 color2 = RGB(227, 161, 218);
        
        uint8 r1 = (color1 >> 16) & 0xFF;
        uint8 g1 = (color1 >> 8) & 0xFF;
        uint8 b1 = color1 & 0xFF;
        
        uint8 r2 = (color2 >> 16) & 0xFF;
        uint8 g2 = (color2 >> 8) & 0xFF;
        uint8 b2 = color2 & 0xFF;
        
        for (int y = 0; y < screen_height; y++) {
            int blend = (y * 255) / screen_height;
            
            uint8 r = r1 + ((r2 - r1) * blend) / 255;
            uint8 g = g1 + ((g2 - g1) * blend) / 255;
            uint8 b = b1 + ((b2 - b1) * blend) / 255;
            
            uint32 color = RGB(r, g, b);
            uint32* row = g_wallpaper_cache + y * screen_width;
            
            for (int x = 0; x < screen_width; x++) {
                row[x] = color;
            }
        }
    }
    
    g_wallpaper_cache_valid = TRUE;
    kprint("built wallpaper cache: %dx%d\n", screen_width, screen_height);
}

void kmain(unsigned long magic __attribute__((unused)), unsigned long addr) {
    g_video_mode = VIDEO_MODE_TEXT;
    console_init(COLOR_BRIGHT_MAGENTA, COLOR_BLUE);
    serial_init();
    MULTIBOOT_INFO *mboot_info;
    gdt_init();
    idt_init();
    register_interrupt_handler(IRQ_BASE + IRQ0_TIMER, timer_handler);
    printf("Biboo is preparing...\n");
    printf("please wait warmly :D\n");

    printf("*cue the obligatory ASCII art*\n");
    printf("\n\n\n\n");
    printf(
        " /$$           /$$$$$$      /$$$$$$            /$$         /$$\n"
        "| $$          /$$__  $$    /$$__  $$           | $$        |__/\n"
        "| $$   /$$   | $$  \\ $$  | $$  \\__/  /$$$$$$  | $$  /$$   /$$\n"
        "| $$  /$$/   | $$  | $$   |  $$$$$$  /$$__  $$  | $$ /$$/  | $$\n"
        "| $$$$$$/    | $$  | $$   \\____  $$ | $$$$$$$$  | $$$$$$/  | $$\n"
        "| $$_  $$    | $$  | $$   /$$  \\ $$| $$_____ |  | $$_$$|   | $$\n"
        "| $$ \\  $$  |  $$$$$$$/  | $$$$$$/ | $$$$$$$| $ | $ \\$$|  | $$\n"
        "|__/  \\__/   \\______/    \\______/  \\_______/ | $_/ \\__/|__/\n"
    );
    printf("\n\n");
    
    mboot_info = (MULTIBOOT_INFO *)addr;
    if (get_mm(&g_kmap, mboot_info) < 0) return;
    pmm_init(g_kmap.available.start_addr, g_kmap.available.size);
    pmm_init_region(g_kmap.available.start_addr, g_kmap.available.size);

    uint32_t free_blocks = pmm_get_max_blocks() - pmm_get_used_blocks();

    uint32_t heap_blocks = (free_blocks > 512) ? (free_blocks - 512) : (free_blocks / 2);
    void *start = pmm_alloc_blocks(heap_blocks);
    
    if (start == NULL) {
        printf("CRITICAL ERROR: Failed to allocate memory for heap!\n");
        printf("Available size: %d, Free blocks: %d\n", g_kmap.available.size, free_blocks);
        for(;;);
    }

    void *end = (void*)((uint32)start + (heap_blocks * PMM_BLOCK_SIZE));
    k_init(start, end);
    init_procsys();
    keyboard_init();
    timer_init(67);
    sti();

    splash("preparing ur storage");

    ata_init();
    printf("\n\n\n\n\n\n\n\n\n");
    fat_init();
    printf("Scanning storage left...\n");
    uint64 free_space = fat_get_free_space();
    printf("REMAINING SPACE: %llu bytes\n", free_space);
    printf("--------------|> %llu MB\n", free_space / (1024 * 1024));
    printf("available blocks: %d\n", free_blocks);
    
    //delay(1000);
    printf("ROCK IN\n");
    
    init_gui();
    mouse_init();

    pci_init();
    
    extern void tester();
    tester();
    ac97_play("/SYSTEM/startup.wav");

    net_send_arp_req(htonl((10 << 24) | (0 << 16) | (2 << 8) | 2));
    net_ping(htonl((10 << 24) | (0 << 16) | (2 << 8) | 2));

    for (;;) {
        unsigned char c = (unsigned char)kb_getchar_nb();
        if (c != 0) {
            //kprint("pressed %c (0x%x)\n", c, c);
            Window* active_window = get_active_win();
            if (active_window) {
                //kprint("%s in focus\n", active_window->title);
                if (active_window->on_key_press) {
                    active_window->on_key_press(active_window, c);
                    is_dirty(TRUE);
                } else {
                    Process* p = get_process(active_window->pid);
                    if (p) kprint(" Process name: %s\n", p->name);
                    if (p && p->name && strcmp(p->name, "pbsh") == 0) {
                        if (c == '`' || c == '|') {
                            pbsh_handle_scroll(active_window, c == '`' ? -5 : 5);
                            is_dirty(TRUE);
                            continue;
                        }
                        char* pbsh_buf = p->buf;
                        int* pbsh_len = &p->curr_buf_len;

                        if (c == '\b') {
                            char* last_prompt = NULL;
                            char* search = pbsh_buf;
                            while (1) {
                                char* found = strstr(search, PROMPT_TEXT);
                                if (!found) break;
                                last_prompt = found;
                                search = found + 1;
                            }
                            
                            int min_len = last_prompt ? (last_prompt - pbsh_buf) + strlen(PROMPT_TEXT) : 0;
                            
                            if (!last_prompt && *pbsh_len > 0) {
                                min_len = *pbsh_len; 
                            }

                            if (*pbsh_len > min_len && *pbsh_len > 0) {
                                (*pbsh_len)--;
                                pbsh_buf[*pbsh_len] = '\0';
                            }
                        } else if (c == '\n') {
                            char* last_prompt = NULL;
                            char* search = pbsh_buf;
                            while (1) {
                                char* found = strstr(search, PROMPT_TEXT);
                                if (!found) break;
                                last_prompt = found;
                                search = found + 1;
                            }
                            
                            int min_len = last_prompt ? (last_prompt - pbsh_buf) + strlen(PROMPT_TEXT) : 0;
                            char *command_start = pbsh_buf + min_len;
                            pbsh_buf_print(pbsh_buf, pbsh_len, "\n");
                            cmd(command_start, pbsh_buf, pbsh_len);

                        } else if (c >= 32 && c < 127) {
                            if (*pbsh_len < MAX_SHBUF_SIZE -1) {
                                pbsh_buf[*pbsh_len] = c;
                                (*pbsh_len)++;
                                pbsh_buf[*pbsh_len] = '\0';
                            }
                        }
                        p->is_scrolled = FALSE;
                        is_dirty(TRUE);
                    }
                }
            } else {
                kprint(" No active window.\n");
            }
        }

        if (g_video_mode == VIDEO_MODE_GRAPHICS) {
            m_update();
            redraw_all();
        }

        ac97_update_streams();
        net_poll();
        reaper_check_redirect();
    }
}

void render_icons(void) {
    const int icon_width = 34;
    const int icon_height = 34;
    const int start_x = 2;
    const int start_y = 1;
    const int spacing = 4;

    for (int i = 0; i < icon_count; i++) {
        int x = start_x + i * (icon_width + spacing);
        int y = start_y;
        icon(x, y, icon_width, icon_height, icons[i].bmp, icons[i].action);

        if (i == icon_count / 2) {
            splash("loading system assets..."); //after this everything is done so I am keeping this msg generic for now
        }
    }
}

void set_wallpaper(const char* filename) {
    static char new_path[256];
    if (filename) {
        strcpy(new_path, "SYSTEM/");
        strcat(new_path, filename);
        
        if (g_wallpaper_bmp_base_data) {
            kfree(g_wallpaper_bmp_base_data);
            g_wallpaper_bmp_base_data = NULL;
            g_wllp_data = NULL;
        }

        g_wallpaper_path = new_path;

        g_wallpaper_bmp_base_data = fat_read_file(g_wallpaper_path);
        if (g_wallpaper_bmp_base_data) {
            BMP_FILE_H *fh = (BMP_FILE_H *)g_wallpaper_bmp_base_data;
            BMP_INFO_H *ih = (BMP_INFO_H *)(g_wallpaper_bmp_base_data + sizeof(*fh));

            if (fh->type == 0x4D42 && ih->bpp == 24 && ih->width > 0 && ih->height > 0) {
                g_wllp_width = ih->width;
                g_wllp_height = ih->height;
                g_wllp_data = g_wallpaper_bmp_base_data + fh->offset;
                g_wallpaper_row_padded = (g_wllp_width * 3 + 3) & ~3;
                
                wllp_asmcache();
            } else {
                kfree(g_wallpaper_bmp_base_data);
                g_wallpaper_bmp_base_data = NULL;
                g_wllp_data = NULL;
            }
        }
        
        is_bg_dirty(TRUE);
    }
}

void sys_cleanup() {
    gui_cleanup();
    vesa_cleanup();

    if (g_wallpaper_bmp_base_data) {
        kfree(g_wallpaper_bmp_base_data);
        g_wallpaper_bmp_base_data = NULL;
        g_wllp_data = NULL;
        g_wllp_width = 0;
        g_wllp_height = 0;
        g_wallpaper_row_padded = 0;
    }
    
    if (g_wallpaper_cache) {
        kfree(g_wallpaper_cache);
        g_wallpaper_cache = NULL;
        g_wallpaper_cache_valid = FALSE;
    }

    kprint("CLEANUP DONE\n");
}

void init_gui() {
    g_video_mode = VIDEO_MODE_GRAPHICS;
    vesa_init(SCREEN_WIDTH, SCREEN_HEIGHT, 32);
    
    splash("preparing desktop...");

    g_init();

    if (g_wallpaper_bmp_base_data == NULL && g_wallpaper_path != NULL) {
        g_wallpaper_bmp_base_data = fat_read_file(g_wallpaper_path);
        if (g_wallpaper_bmp_base_data) {
            BMP_FILE_H *fh = (BMP_FILE_H *)g_wallpaper_bmp_base_data;
            BMP_INFO_H *ih = (BMP_INFO_H *)(g_wallpaper_bmp_base_data + sizeof(*fh));

            if (fh->type == 0x4D42 && ih->bpp == 24 && ih->width > 0 && ih->height > 0) {
                g_wllp_width = ih->width;
                g_wllp_height = ih->height;
                g_wllp_data = g_wallpaper_bmp_base_data + fh->offset;
                g_wallpaper_row_padded = (g_wllp_width * 3 + 3) & ~3;
            } else {
                kfree(g_wallpaper_bmp_base_data);
                g_wallpaper_bmp_base_data = NULL;
                g_wllp_data = NULL;
            }
        }
    }

    wllp_asmcache();

    desktop();
    render_icons();
    swapbuf();
}