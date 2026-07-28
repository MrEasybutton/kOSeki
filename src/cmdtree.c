#include "cmdtree.h"
#include "console.h"
#include "power.h"
#include "../include/string.h"
#include "minimp3.h"
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
#include "baux2/baux2.h"

#define LOGO_COLOR RGB(232,120,250)
#define TEXT_COLOR RGB(255,255,255)

char mem_uf_str[64];
char usage_str[64];
char cpu_info_str[128] = "???";

const char *logo[10] = {
    "            ",
    "  ------_   ",
    " / ^    %\\  ",
    " (@  @   >  ",
    " | ~     )  ",
    " \\_______/  ",
    "            ",
    " kOSeki v3  ",
    "            ",
    "            "
};

FetchInfo info[] = {
    { "", "", 0, 0 },
    { "user", "BIBOO", RGB(255,220,80), RGB(245,75,255) },
    { "system", "kOSeki", RGB(255,220,80), RGB(205,155,225) },
    { "ver", "v3.0.2", RGB(255,220,80), RGB(205,175,225) },
    { "shell", "pbsh-3", RGB(255,220,80), RGB(155,245,145) },
    { "", "", 0, 0 },
    { "RAM", mem_uf_str, RGB(255,220,80), RGB(155,155,255) },
    { "DISK", usage_str, RGB(255,220,80), RGB(155,155,255) },
    { "CPU", cpu_info_str, RGB(255,220,80), RGB(155,155,255) },
    { "", "", 0, 0 }
};

typedef struct {
    const char* command;
    const char* description;
} pbsh_help;

static const pbsh_help help_commands[] = {
    { "beep", "echo text to shell" },
    { "geowfetch", "about kOSeki" },
    { "ls", "list root" },
    { "johncat", "read a file" },
    { "rock", "create a file" },
    { "stone", "create a folder" },
    { "nov", "append text to a file" },
    { "shiorin", "overwrite a file" },
    { "chuuni/agent47","rename file" },
    { "pickaxe/mine", "delete a file" },
    { "peb", "create a Pebble" },
    { "clear", "clear this shell" },
    { "karaoke", "play audio file" },
    { "synth", "play a synth" },
    { "br", "play with BloodRaven" },
};

extern void cpu_cpuid(int CPUInfo[4], int InfoType);
void geowfetch() {
    //ram
    uint32 total_blocks = pmm_get_max_blocks();
    uint32 used_blocks = pmm_get_used_blocks();
    uint32 total_mem_mb = total_blocks / 256;
    uint32 used_mem_mb = used_blocks / 256;
    sprintf(mem_uf_str, "%u / %u MB", used_mem_mb, total_mem_mb);
    
    //disk
    uint64 total_disk_bytes = (uint64)g_ide_devices[0].size * 512;
    uint64 free_disk_bytes = fat_get_free_space();
    uint64 used_disk_bytes = total_disk_bytes - free_disk_bytes;
    
    uint32 used_mb = (uint32)(used_disk_bytes / (1024 * 1024));
    uint32 total_mb = (uint32)(total_disk_bytes / (1024 * 1024));
    sprintf(usage_str, "%u / %u MB", used_mb, total_mb);

    //cpu
    int regs[4];
    char brand[128] = {0};

    cpu_cpuid(regs, 0x80000000);
    unsigned int max_ext = (unsigned int)regs[0];
    if (max_ext >= 0x80000004u) {
        char *p = brand;
        for (unsigned int leaf = 0x80000002u; leaf <= 0x80000004u; leaf++) {
            cpu_cpuid(regs, leaf);
            memcpy(p, &regs[0], 4); p += 4;
            memcpy(p, &regs[1], 4); p += 4;
            memcpy(p, &regs[2], 4); p += 4;
            memcpy(p, &regs[3], 4); p += 4;
        }
        brand[127] = '\0';
        int len = strlen(brand);
        while (len > 0 && brand[len - 1] == ' ') brand[--len] = '\0';
    } else {
        cpu_cpuid(regs, 0);
        char vendor[13] = {0};
        memcpy(&vendor[0], &regs[1], 4);
        memcpy(&vendor[4], &regs[3], 4);
        memcpy(&vendor[8], &regs[2], 4);
        snprintf(brand, sizeof(brand), "%s CPU", vendor);
    }

    strncpy(cpu_info_str, brand, sizeof(cpu_info_str) - 1);
    cpu_info_str[sizeof(cpu_info_str) - 1] = '\0';
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

//so I can spam the help properly
void pbsh_delim_print(char* pbsh_buf, int* pbsh_len, const char* command, const char* desc, const char* delim, uint32 color) {
    pbsh_buf_print_ex(pbsh_buf, pbsh_len, command, color);
    pbsh_buf_print(pbsh_buf, pbsh_len, delim);
    pbsh_buf_print(pbsh_buf, pbsh_len, desc);
    pbsh_buf_print(pbsh_buf, pbsh_len, "\n");
}

void pbsh_buf_print(char* pbsh_buf, int* pbsh_len, const char* str) {
    int str_len = strlen(str);
    if (*pbsh_len + str_len < MAX_SHBUF_SIZE - 1) {
        strcpy(pbsh_buf + *pbsh_len, str);
        *pbsh_len += str_len;
        pbsh_buf[*pbsh_len] = '\0';
    }
}

void pbsh_buf_print_ex(char* pbsh_buf, int* pbsh_len, const char* str, uint32 col) {
    char tag[16];
    const char hex[] = "0123456789ABCDEF";
    int v = (int)(col & 0xFFFFFF);
    char hexstr[7];
    for (int i = 0; i < 6; i++) {
        int shift = (5 - i) * 4;
        hexstr[i] = hex[(v >> shift) & 0xF];
    }
    hexstr[6] = '\0';

    strcpy(tag, "<C:");
    strcat(tag, hexstr);
    strcat(tag, ">");

    pbsh_buf_print(pbsh_buf, pbsh_len, tag);
    pbsh_buf_print(pbsh_buf, pbsh_len, str);
    pbsh_buf_print(pbsh_buf, pbsh_len, "</C>");
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

extern uint32 timer_ticks;
extern uint32 g_timer_hz;

extern void redraw_all(void);

void countdown_to(void (*action)(void), const char* label, char* pbsh_buf, int* pbsh_len) {
    for (int s = 3; s > 0; s--) {
        char msg[64];
        strcpy(msg, label);
        strcat(msg, " [");
        char n[2] = { '0' + s, '\0' };
        strcat(msg, n);
        strcat(msg, "]\n");

        pbsh_buf_print(pbsh_buf, pbsh_len, msg);
        is_dirty(TRUE);
        redraw_all();

        uint32 target = timer_ticks + g_timer_hz;
        while (timer_ticks < target) asm volatile("hlt");
    }
    action();
}
static void _shutdown(void) { shutdown(PM1A_CNT_BLK, PM1B_CNT_BLK); }

void cmd(char* command_line, char* pbsh_buf, int* pbsh_len) {
    char* cmd_copy = (char*)malloc(strlen(command_line) + 1);
    if (!cmd_copy) {
        pbsh_buf_print(pbsh_buf, pbsh_len, "\nerr: malloc failed!\n");
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
    else if (strcmp(command, "geowfetch") == 0) {
        geowfetch();

        int lines = sizeof(info) / sizeof(info[0]);
        int logo_width = 10;

        for (int i = 0; i < lines; i++) {
            pbsh_buf_print_ex(pbsh_buf, pbsh_len, logo[i], LOGO_COLOR);

            // pad to logo w
            int pad = logo_width - strlen(logo[i]);
            while (pad-- > 0) pbsh_buf_print_ex(pbsh_buf, pbsh_len, " ", TEXT_COLOR);

            pbsh_buf_print_ex(pbsh_buf, pbsh_len, "  ", TEXT_COLOR);

            if (info[i].label[0]) {
                pbsh_buf_print_ex(pbsh_buf, pbsh_len, info[i].label, info[i].label_color);
                pbsh_buf_print_ex(pbsh_buf, pbsh_len, ": ", TEXT_COLOR);
                pbsh_buf_print_ex(pbsh_buf, pbsh_len, info[i].value, info[i].value_color);
            }

            pbsh_buf_print_ex(pbsh_buf, pbsh_len, "\n", TEXT_COLOR);
        }
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
    else if (strcmp(command, "oyasumi") == 0) {
        //screeeeeeensaver :D
    }
    else if (strcmp(command, "bweh") == 0) {
        countdown_to(_shutdown, "til next time...", pbsh_buf, pbsh_len);
    }
    else if (strcmp(command, "eepy") == 0) {
        countdown_to(reboot, "eepy biboo needs a nap...", pbsh_buf, pbsh_len);
    }
    else if (strcmp(command, "help") == 0) {
        const uint32 command_color = 0xc074db;

        pbsh_buf_print(pbsh_buf, pbsh_len, "\n");
        pbsh_buf_print_ex(pbsh_buf, pbsh_len, "COMMANDS:\n", RGB(240, 220, 40));

        for (int i = 0; i < sizeof(help_commands) / sizeof(help_commands[0]); i++) {
            pbsh_buf_print_ex(pbsh_buf, pbsh_len, help_commands[i].command, command_color);

            pbsh_buf_print(pbsh_buf, pbsh_len, " > ");
            pbsh_buf_print(pbsh_buf, pbsh_len, help_commands[i].description);
            pbsh_buf_print(pbsh_buf, pbsh_len, "\n");
        }
        pbsh_buf_print_ex(pbsh_buf, pbsh_len, "launch an app by typing its \n", RGB(220, 180, 240));
        pbsh_buf_print_ex(pbsh_buf, pbsh_len, "name!\n\n", RGB(220, 180, 240));
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