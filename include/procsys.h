#ifndef PROCSYS_H
#define PROCSYS_H

#include "types.h"
#include "gui.h"
#include "fat32.h"

#define MAX_PROCESSES 67

#define MAX_SHBUF_SIZE 4096

typedef enum {
    PROCESS_STATE_RUNNING,
    PROCESS_STATE_TERMINATED,
    PROCESS_STATE_ERROR
} ProcessState;

typedef struct Process {
    int pid;
    const char* name;
    ProcessState state;
    struct Window* window;
    void* data; //uspc
    BOOL proc_isowner;
    FAT_DirList* wallpaper_files;

    char buf[MAX_SHBUF_SIZE];
    int curr_buf_len;
    int scroll_ofs;
    BOOL is_scrolled;
    int cursor_pos;

    BOOL window_exists;
    BOOL is_valid;
} Process;

extern Process* processes[MAX_PROCESSES];

void init_procsys();
BOOL launch_program(const char* name);

Process* create_process(const char* name);
Process* get_process(int pid);
Process* get_process_by_name(const char* name);
void cleanup_process(int pid);
BOOL isvalidproc(int pid);
void valid_refs();
BOOL isvalidwin(int pid, struct Window* window);
void register_for_process(int pid, struct Window* window);
void unregister_for_process(int pid);

void render_win(struct Window* win);

#endif
