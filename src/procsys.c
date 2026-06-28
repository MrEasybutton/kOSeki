#include "procsys.h"
#include "gui.h"
#include "console.h"
#include "vesa.h"
#include "kheap.h"
#include "serial.h"
#include "string.h"
#include "fat32.h"
#include "graphics.h"
#include "apps/Pebbleshell.h"
#include "baux2/baux2.h"

Process* processes[MAX_PROCESSES];

static int count_proc = 0; //dbg

#define WIN_OFS_X 30
#define WIN_OFS_Y 30 

// ts kinda hardcoded hahahahahahh
#define MAX_WINDOW_WIDTH 400
#define MAX_WINDOW_HEIGHT 300

#define MIN_WINDOW_X 0
#define MIN_WINDOW_Y 0
#define MAX_WINDOW_X (SCREEN_WIDTH - 100)
#define MAX_WINDOW_Y (SCREEN_HEIGHT - 100)

void init_procsys() {
    memset(processes, 0, sizeof(Process*) * MAX_PROCESSES);
    kprint("procsys init done\n");
}

BOOL isvalidproc(int pid) {
    if (pid < 0 || pid >= MAX_PROCESSES || !processes[pid]) {
        return FALSE;
    }
    
    Process* p = processes[pid];
    return p && p->is_valid && p->state != PROCESS_STATE_TERMINATED && p->state != PROCESS_STATE_ERROR;
}

BOOL isvalidwin(int pid, struct Window* window) {
    if (!isvalidproc(pid) || !window) {
        return FALSE;
    }
    
    Process* p = processes[pid];
    return p->window == window && p->window_exists;
}

void valid_refs() {
    for (int pid = 0; pid < MAX_PROCESSES; pid++) {
        if (!processes[pid]) continue;
        
        Process* p = processes[pid];
        if (!p->is_valid) continue;
        
        if (p->window_exists && !p->window) {
            p->window_exists = FALSE;
        }
    }
}

Process* create_process(const char* name) {
    int pid = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i] == NULL) {
            pid = i;
            break;
        }
    }

    if (pid == -1) {
        kprint("ERROR: ! process budget is exhausted !\n");
        return NULL;
    }

    Process* p = (Process*)kmalloc(sizeof(Process));
    if (!p) {
        kprint("ERROR: failed to allocate memory for new process\n");
        return NULL;
    }

    memset(p, 0, sizeof(Process));
    
    p->pid = pid;
    p->data = NULL;
    p->proc_isowner = FALSE;
    p->curr_buf_len = 0;
    p->buf[0] = '\0';
    p->scroll_ofs = 0;
    
    p->is_scrolled = 0;
    p->window_exists = 0;
    p->is_valid = 1;
    
    printf("create_process: After init - is_valid=%d, window_exists=%d\n", 
           p->is_valid, p->window_exists);
    
    count_proc++;
    
    if (name) {
        char* name_copy = (char*)kmalloc(strlen(name) + 1);
        if (!name_copy) {
            kfree(p);
            return NULL;
        }
        strcpy(name_copy, name);
        p->name = name_copy;
    } else {
        char* default_name = (char*)kmalloc(strlen("unnamed") + 1);
        if (!default_name) {
            kfree(p);
            return NULL;
        }
        strcpy(default_name, "unnamed");
        p->name = default_name;
    }
    
    p->state = PROCESS_STATE_RUNNING;
    processes[pid] = p;
    
    printf("Process created: %s (PID: %d)\n", p->name, p->pid);
    return p;
}

void register_for_process(int pid, struct Window* window) {
    if (!isvalidproc(pid) || !window) {
        return;
    }
    
    Process* p = processes[pid];
    p->window = window;
    p->window_exists = TRUE;
}

void unregister_for_process(int pid) {
    if (!isvalidproc(pid)) {
        return;
    }
    
    Process* p = processes[pid];
    p->window = NULL;
    p->window_exists = FALSE;
}

Process* get_process(int pid) {
    if (!isvalidproc(pid)) {
        return NULL;
    }
    return processes[pid];
}

Process* get_process_by_name(const char* name) {
    if (!name) return NULL;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        Process* p = processes[i];

        if (!p) continue;
        if (!p->is_valid) continue;

        if (p->name && strcmp(p->name, name) == 0) {
            return p;
        }
    }

    return NULL;
}

void cleanup_process(int pid) {
    if (pid < 0 || pid >= MAX_PROCESSES || !processes[pid]) {
        return;
    }

    kprint("Begin cleanup for process (PID: %d\n)", pid);
    
    Process* p = processes[pid];
    
    p->is_valid = FALSE;
    p->state = PROCESS_STATE_TERMINATED;
    
    if (p->data && p->proc_isowner) {
        kfree(p->data);
        p->data = NULL;
        p->proc_isowner = FALSE;
    }

    p->window_exists = FALSE;
    p->window = NULL;
    
    if (p->name) {
        kfree((void*)p->name);
        p->name = NULL;
    }
    
    kfree(p);
    processes[pid] = NULL;
}

void render_win(struct Window* win) {
    if (!win) {
        printf("win pointer is null\n");
        return;
    }

    Process* p = get_process(win->pid);
    if (!p) {
        printf("there is no current process for PID %d\n", win->pid);
        return;
    }

    if (!p->data) {
        printf("there is no data for the process (PID %d)\n", win->pid);
        return;
    }

    uint32 screen_width = vbe_get_width();
    uint32 screen_height = vbe_get_height();

    if (win->width <= 0 || win->height <= 0) {
        printf("dimensions are invalid for the window (ID %d)\n", win->id);
        return;
    }

    // offscreen skip
    if (win->x >= (int)screen_width || win->y >= (int)screen_height ||
        win->x + win->width <= 0 || win->y + win->height <= 0) {
        return;
    }

    int content_x = win->x + WIN_BORDER + 2;
    int content_y = win->y + TITLEBAR_H + WIN_BORDER + 2;

    int content_width = win->width - (2 * WIN_BORDER) - 4;
    int content_height = win->height - TITLEBAR_H - (2 * WIN_BORDER) - 4;

    if (content_width <= 0 || content_height <= 0) return;

    const int line_height = 10;
    const int max_lines = content_height / line_height;

    if (max_lines <= 0) return;

    // above/below (say that again)
    int firstvis = 0;
    if (win->y < 0) {
        firstvis = (-(win->y) + content_y - WIN_BORDER) / line_height;
        if (firstvis < 0) firstvis = 0;
    }

    int lastvis = max_lines;
    if (win->y + win->height > (int)screen_height) {
        lastvis = ((int)screen_height - content_y) / line_height + 1;
        if (lastvis > max_lines) lastvis = max_lines;
    }

    const int MAX_LINEBUF = 255;
    char line_buffer[MAX_LINEBUF + 1];

    char* current_char = p->data;
    int char_index = 0;
    int line_num = 0;
    const int MAX_CHARS_TO_PROCESS = 100000;
    int chars_processed = 0;

    while (line_num < firstvis && *current_char && chars_processed < MAX_CHARS_TO_PROCESS) {
        if (*current_char == '\n') {
            line_num++;
        }
        current_char++;
        chars_processed++;
    }

    char_index = 0;
    
    while (*current_char != '\0' && line_num < lastvis && chars_processed < MAX_CHARS_TO_PROCESS) {
        if (*current_char == '\n' || char_index >= MAX_LINEBUF) {
            line_buffer[char_index] = '\0';

            int draw_y = content_y + (line_num * line_height);
            text(line_buffer, content_x, draw_y, RGB(0, 0, 0), FONT_DEFAULT, FALSE);

            char_index = 0;
            line_num++;
            if (*current_char == '\n') {
                current_char++;
                chars_processed++;
            }
        } else {
            line_buffer[char_index++] = *current_char++;
            chars_processed++;

            if (line_buffer[char_index-1] == 0) { // jic
                line_buffer[char_index-1] = ' ';
            }
        }
    }

    if (char_index > 0 && line_num < lastvis) {
        line_buffer[char_index] = '\0';
        int draw_y = content_y + (line_num * line_height);

        text(line_buffer, content_x, draw_y, RGB(0, 0, 0), FONT_DEFAULT, FALSE);
    }

    if (chars_processed >= MAX_CHARS_TO_PROCESS) {
        char truncated_msg[] = "...";
        int msg_y = win->y + win->height - WIN_BORDER - line_height - 2;

        if (msg_y > 0 && msg_y < (int)screen_height) {
            text(truncated_msg, content_x, msg_y, RGB(150, 0, 0), FONT_DEFAULT, FALSE);
        }
    }
}

// launch pebbles
BOOL yeet_pebble(const char* name) {
    char bundle_path[256];
    if (strstr(name, ".pebble") != NULL || strstr(name, ".PEBBLE") != NULL) {
        if (name[0] == '/') {
            strncpy(bundle_path, name, sizeof(bundle_path));
        } else {
            snprintf(bundle_path, sizeof(bundle_path), "/%s", name);
        }
    } else {
        snprintf(bundle_path, sizeof(bundle_path), "/%s.pebble", name);
    }

    FAT_dirent* dirent = fat_find_file(bundle_path);
    if (!dirent) return FALSE;

    if (!(dirent->Attributes & 0x10)) {
        kfree(dirent);
        return FALSE;
    }
    kfree(dirent);

    const char* proc_name = name;
    const char* last_slash = strrchr(name, '/');
    if (last_slash) proc_name = last_slash + 1;

    char manifest_path[256];
    snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.jdon", bundle_path);
    char* manifest = fat_read_file(manifest_path);
    if (!manifest) {
        kprint("[pebble] This pebble has no manifest.jdon (searched in %s)\n", bundle_path);
        return FALSE;
    }
    kfree(manifest);

    char run_path[256];
    snprintf(run_path, sizeof(run_path), "%s/run.bx2", bundle_path);
    char* run_script = fat_read_file(run_path);
    if (!run_script) {
        kprint("[pebble] This pebble has no run script (searched in %s)\n", bundle_path);
        return FALSE;
    }

    Process* p = create_process(proc_name);
    if (!p) {
        kfree(run_script);
        return FALSE;
    }

    BAUx2_d* app_data = (BAUx2_d*)kmalloc(sizeof(BAUx2_d));
    if (!app_data) {
        cleanup_process(p->pid);
        kfree(run_script);
        return FALSE;
    }
    memset(app_data, 0, sizeof(BAUx2_d));
    p->data = app_data;

    extern int g_baux2_curr_pid;
    int old_pid = g_baux2_curr_pid;
    g_baux2_curr_pid = p->pid;
    baux2_run(run_script);
    g_baux2_curr_pid = old_pid;

    if (!p->window_exists) {
        baux2_cleanup_process(p);
        cleanup_process(p->pid);
    }
    
    return TRUE;
}

// for kernelspace stuff (holo-grams)
BOOL launch_program(const char* name) {
    if (!name || strlen(name) == 0) {
        printf("Program name is invalid\n");
        return FALSE;
    }

    if (yeet_pebble(name)) return TRUE;
    
    if (strcmp(name, "pbsh") == 0) {
        launch_pbsh();
        return TRUE;
    }

    char path[256];
    if (name[0] == '/') {
        strncpy(path, name, 250);
        path[250] = '\0';
    } else {
        strcpy(path, "/");
        strncat(path, name, 240);
    }
    strcat(path, ".c");

    FAT_dirent* dirent = fat_find_file(path);
    if (!dirent || (dirent->Attributes & 0x10)) {
        if (dirent) kfree(dirent);
        char error_msg[256];
        strcpy(error_msg, "Could not find program: ");
        strcat(error_msg, path);
        text(error_msg, 10, 50, RGB(255, 0, 0), FONT_DEFAULT, false);
        return FALSE;
    }
    
    kfree(dirent);
    dirent = NULL;

    char* file_content = fat_read_file(path);
    if (!file_content) {
        char error_msg[256];
        strcpy(error_msg, "Could not read program: ");
        strcat(error_msg, path);
        text(error_msg, 10, 50, RGB(255, 0, 0), FONT_DEFAULT, false);
        return FALSE;
    }

    Process* p = create_process(name);
    if (!p) {
        kfree(file_content);
        text("Failed to create process.", 10, 50, RGB(255, 0, 0), FONT_DEFAULT, false);
        return FALSE;
    }

    p->data = file_content;
    p->proc_isowner = TRUE;

    Window* w = window(p->pid, name, -1, -1, 300, 200);
    if (!w) {
        cleanup_process(p->pid);
        text("Failed to create window.", 10, 50, RGB(255, 0, 0), FONT_DEFAULT, false);
        return FALSE;
    }

    w->content_renderer = render_win;
    p->window = w;
    p->window_exists = TRUE;
    
    return TRUE;
}