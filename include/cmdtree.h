#ifndef CMDTREE_H
#define CMDTREE_H

#include "vesa.h"
#include "pmm.h"
#include "apps/Pebbleshell.h"
#include "apps/Reaper.h"
#include "apps/WAHtercolour.h"
#include "apps/Novella.h"
#include "apps/CLStudio.h"

#define PROMPT_TEXT "$BIBOO > "

typedef struct {
    const char *label;
    const char *value;
    uint32 label_color;
    uint32 value_color;
} FetchInfo;

extern FetchInfo info[];
extern char mem_uf_str[64];

typedef struct {
    char* buf;
    int* len;
} pbshPrint;

static pbshPrint g_pbsh_print;

void pbsh_buf_print(char* pbsh_buf, int* pbsh_len, const char* str);
void pbsh_buf_print_ex(char* pbsh_buf, int* pbsh_len, const char* str, uint32 color);
static void pbsh_bx2_print(const char* text) { pbsh_buf_print(g_pbsh_print.buf, g_pbsh_print.len, text); }

typedef void (*IconAction)(void);

typedef struct {
    const char* bmp;
    IconAction action;
} IconDef;

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

void cmd(char* command_line, char* pbsh_buf, int* pbsh_len);

#endif