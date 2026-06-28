#include "apps/Preferences.h"
#include "procsys.h"
#include "gui.h"
#include "pon.h"
#include "graphics.h"
#include "string.h"
#include "kheap.h"
#include "vesa.h"
#include "fonts.h"
#include "console.h"
#include "fat32.h"
#include "serial.h"
#include "kernel.h"
#include "pmm.h"
#include "ide.h"

#define FONT FONT_KALNIA
#define WIN_BORDER 2
#define TITLEBAR_H 20
#define ITEM_HEIGHT 24
#define PADDING 8

#define COLOR_BG_DARK RGB(18, 4, 12)
#define COLOR_BG_MEDIUM RGB(24, 24, 28)
#define COLOR_BG_LIGHT RGB(32, 32, 38)
#define COLOR_BG_ELEVATED RGB(38, 38, 45)
#define COLOR_JEM RGB(215, 140, 245)
#define COLOR_ACCENT_PRIMARY RGB(67, 64, 101)
#define COLOR_ACCENT_SECONDARY RGB(29, 29, 30)
#define COLOR_ACCENT_HOVER RGB(185, 180, 220)
#define COLOR_TEXT_PRIMARY RGB(240, 240, 245)
#define COLOR_TEXT_SECONDARY RGB(180, 180, 190)
#define COLOR_TEXT_MUTED RGB(120, 120, 130)
#define COLOR_BORDER RGB(50, 50, 58)
#define COLOR_SELECTION_BG RGB(50, 48, 62)

typedef enum {
    PAGE_ABOUT,
    PAGE_WALLPAPER,
    PAGE_SYSTECH
} PrefsPage;

#define MAX_STACK 8

typedef struct {
    int hover_index;
    int selected_index;
    PON_Comp* root;
    PON_Comp* root_to_free;

    BOOL scrollbar_dragging;
    int drag_y_start;
    int scroll_start_y;

    PrefsPage stack[MAX_STACK];
    int stack_depth;
} PrefsData;

static void process_deferred_free(PrefsData* app_data) {
    if (app_data->root_to_free) {
        PON_free(app_data->root_to_free);
        app_data->root_to_free = NULL;
    }
}

static void nav_to(int pid, PrefsPage page, BOOL push);

static void on_nav_wallpaper(PON_Comp* comp, int rel_x, int rel_y) {
    (void)rel_x; (void)rel_y; (void)comp;
    Window* win = get_active_win();
    if (win) nav_to(win->pid, PAGE_WALLPAPER, TRUE);
}

static void on_nav_systech(PON_Comp* comp, int rel_x, int rel_y) {
    (void)rel_x; (void)rel_y; (void)comp;
    Window* win = get_active_win();
    if (win) nav_to(win->pid, PAGE_SYSTECH, TRUE);
}

static void on_nav_back(PON_Comp* comp, int rel_x, int rel_y) {
    (void)rel_x; (void)rel_y; (void)comp;
    Window* win = get_active_win();
    if (win) {
        Process* p = get_process(win->pid);
        if (p) {
            PrefsData* app_data = (PrefsData*)p->data;
            if (app_data->stack_depth > 1) {
                app_data->stack_depth--; // pop curr
                PrefsPage prev = app_data->stack[app_data->stack_depth - 1];
                app_data->stack_depth--; // pop n repush
                nav_to(win->pid, prev, TRUE);
            }
        }
    }
}

static void san_name(char* dest, const char* src, int max_len) {
    if (!dest || !src) return;

    strncpy(dest, src, max_len - 1);
    dest[max_len - 1] = '\0';

    const char* suffix = "-wllp.bmp";
    int src_len = strlen(dest);
    int suffix_len = strlen(suffix);

    if (src_len >= suffix_len) {
        if (strcmp(dest + src_len - suffix_len, suffix) == 0) {
            dest[src_len - suffix_len] = '\0';
        }
    }
}


static void on_wallpaper_click(PON_Comp* comp, int rel_x, int rel_y) {
    (void)rel_x; (void)rel_y;
    char* filename = (char*)comp->appdata;
    if (filename) {
        set_wallpaper(filename);
        
        if (comp->parent) {
            for (int i = 0; i < comp->parent->child_count; i++) {
                comp->parent->children[i]->selected = FALSE;
            }
        }
        comp->selected = TRUE;
        
        is_dirty(TRUE);
    }
}

static void prefs(Window* win) {
    if (!win) return;

    Process* p = get_process(win->pid);
    if (!p) return;

    PrefsData* app_data = (PrefsData*)p->data;
    if (!app_data || !app_data->root) return;

    int content_x = win->x + WIN_BORDER;
    int content_y = win->y + TITLEBAR_H;
    int content_w = win->width - (2 * WIN_BORDER);
    int content_h = win->height - TITLEBAR_H - WIN_BORDER;
    
    rect(content_x, content_y, content_w, content_h, COLOR_BG_DARK);

    PON_render(app_data->root, content_x, content_y);

    int content_total_h = PON_get_content_height(app_data->root);
    if (content_total_h > app_data->root->height) { // scrollbar
        int sb_w = 12;
        int sb_x = content_x + content_w - sb_w - 4;
        int sb_y = content_y + 4;
        int sb_h = content_h - 8;

        rect(sb_x, sb_y, sb_w, sb_h, COLOR_BG_MEDIUM);

        int thumb_h = (sb_h * app_data->root->height) / content_total_h;
        if (thumb_h < 20) thumb_h = 20;

        int thumb_y = sb_y + (app_data->root->scroll_y * (sb_h - thumb_h)) / (content_total_h - app_data->root->height);

        rect(sb_x + 2, thumb_y + 2, sb_w - 4, thumb_h - 4, COLOR_ACCENT_PRIMARY);
    }
}

static void pref_m_down(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p) return;
    PrefsData* app_data = (PrefsData*)p->data;
    if (!app_data || !app_data->root) return;

    int content_total_h = PON_get_content_height(app_data->root);
    if (content_total_h > app_data->root->height) {
        int content_w = win->width - (2 * WIN_BORDER);
        int sb_w = 12;
        int sb_x = content_w - sb_w - 4;

        if (x >= sb_x && x < sb_x + sb_w) {
            app_data->scrollbar_dragging = TRUE;
            app_data->drag_y_start = y;
            app_data->scroll_start_y = app_data->root->scroll_y;
            return;
        }
    }

    PON_Comp* current_root = app_data->root;
    if (handle_mouse(current_root, win->x + WIN_BORDER, win->y + TITLEBAR_H, 
                    win->x + WIN_BORDER + x, win->y + TITLEBAR_H + y, PON_MOUSE_DOWN)) {
        is_dirty(TRUE);
    }
    process_deferred_free(app_data);
}

static void m_up(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p) return;
    PrefsData* app_data = (PrefsData*)p->data;
    if (!app_data || !app_data->root) return;

    if (app_data->scrollbar_dragging) {
        app_data->scrollbar_dragging = FALSE;
        return;
    }

    PON_Comp* current_root = app_data->root;
    if (handle_mouse(current_root, win->x + WIN_BORDER, win->y + TITLEBAR_H, 
                    win->x + WIN_BORDER + x, win->y + TITLEBAR_H + y, PON_MOUSE_UP)) {
        is_dirty(TRUE);
    }
    process_deferred_free(app_data);
}

static void m_move(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p) return;
    PrefsData* app_data = (PrefsData*)p->data;
    if (!app_data || !app_data->root) return;

    if (app_data->scrollbar_dragging) {
        int dy = y - app_data->drag_y_start;
        int content_h = app_data->root->height;
        int content_total_h = PON_get_content_height(app_data->root);
        int sb_h = win->height - TITLEBAR_H - WIN_BORDER - 8;
        
        int thumb_h = (sb_h * content_h) / content_total_h;
        if (thumb_h < 20) thumb_h = 20;

        int scroll_move = (dy * (content_total_h - content_h)) / (sb_h - thumb_h);
        app_data->root->scroll_y = app_data->scroll_start_y + scroll_move;

        int max_scroll = content_total_h - content_h + PADDING;
        if (max_scroll < 0) max_scroll = 0;
        if (app_data->root->scroll_y < 0) app_data->root->scroll_y = 0;
        if (app_data->root->scroll_y > max_scroll) app_data->root->scroll_y = max_scroll;

        is_dirty(TRUE);
        return;
    }

    PON_Comp* current_root = app_data->root;
    if (handle_mouse(current_root, win->x + WIN_BORDER, win->y + TITLEBAR_H, 
                    win->x + WIN_BORDER + x, win->y + TITLEBAR_H + y, PON_MOUSE_MOVE)) {
        is_dirty(TRUE);
    }
    process_deferred_free(app_data);
}

static void cleanup(Window* win) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (p) {
        if (p->wallpaper_files) {
            if (p->wallpaper_files->entries) {
                kfree(p->wallpaper_files->entries);
            }
            kfree(p->wallpaper_files);
            p->wallpaper_files = NULL;
        }
        PrefsData* app_data = (PrefsData*)p->data;
        if (app_data) {
            if (app_data->root) PON_free(app_data->root);
            if (app_data->root_to_free) PON_free(app_data->root_to_free);
            kfree(app_data);
            p->data = NULL;
        }
    }
}

static void headerP(PON_Comp* comp, int ax, int ay) {
    PON_Panel_d* data = (PON_Panel_d*)comp->data;
    rect(ax, ay, comp->width, comp->height, data->color);
}

static void on_scroll(Window* win, int delta) {
    Process* p = get_process(win->pid);
    if (!p) return;
    PrefsData* app_data = (PrefsData*)p->data;
    if (!app_data || !app_data->root) return;

    app_data->root->scroll_y += delta;

    int content_h = PON_get_content_height(app_data->root);
    int visible_h = app_data->root->height;
    int max_scroll = content_h - visible_h + PADDING;
    if (max_scroll < 0) max_scroll = 0;

    if (app_data->root->scroll_y < 0) app_data->root->scroll_y = 0;
    if (app_data->root->scroll_y > max_scroll) app_data->root->scroll_y = max_scroll;

    is_dirty(TRUE);
}

static void manual_scroll(Window* win, unsigned int key) {
    if (key == '`') on_scroll(win, -15);
    else if (key == '|') on_scroll(win, 15);
}

static uint32 s=0x12345678;

static uint32 r(void){
    s^=s<<13; s^=s>>17; s^=s<<5;
    return s;
}

const char *gacha(const char **a, int n) {
    int i, rare_idx[128], norm_idx[128];
    int rare_n = 0, norm_n = 0;

    for (i = 0; i < n && i < 128; i++) {
        if (a[i][0] == '*') rare_idx[rare_n++] = i;
        else norm_idx[norm_n++] = i;
    }

    if (rare_n && r() < 35132870u)
        return a[rare_idx[r() % rare_n]] + 1;

    if (norm_n)
        return a[norm_idx[r() % norm_n]];

    return n > 0 ? a[0] : "";
}

static void page_about(Window* win, PrefsData* app_data) {
    (void)win;
    int y_cursor = PADDING;

    const char* gacha_lines[] = {
        "magic num: 0x81800144",
        "ROCK IN!",
        ":D",
        "gacha_lines[] !!",
        "zero bloatware!",
        "well, here we are again!",
        "CODE 81800",
        "*lucky pebble~"
    };

    PON_Comp* about = PANEL(PADDING, y_cursor, app_data->root->width - (2 * PADDING), 90, COLOR_BG_LIGHT);
    if (about) {
        about->draw = headerP;
        PON_child(app_data->root, about);

        PON_Comp* t1 = TEXT(15, 10, "*kOSeki v3.0*", COLOR_TEXT_PRIMARY);
        if (t1) PON_child(about, t1);
        PON_Comp* t2 = TEXT(15, 30, "The unofficial Biboo OS", COLOR_TEXT_SECONDARY);
        if (t2) PON_child(about, t2);
        PON_Comp* t3 = TEXT(15, 50, "by easybuttondev", COLOR_TEXT_MUTED);
        if (t3) PON_child(about, t3);
        PON_Comp* t4 = TEXT(15, 70, gacha(gacha_lines, sizeof(gacha_lines)/sizeof(gacha_lines[0])), COLOR_ACCENT_HOVER);
        if (t4) PON_child(about, t4);
        y_cursor += 90 + PADDING;
    }

    struct {
        const char* text;
        event_cb_t action;
    } navstuffs[] = {
        { "WALLPAPERS >", on_nav_wallpaper },
        { "SYSTEM SPECS >", on_nav_systech }
    };

    size_t nav_cnt = sizeof(navstuffs)/sizeof(navstuffs[0]);
    for (size_t i = 0; i < nav_cnt; i++) {
        PON_Comp* btn = BUTTON(
            PADDING,
            y_cursor,
            app_data->root->width - (2 * PADDING),
            ITEM_HEIGHT + 4,
            navstuffs[i].text,
            navstuffs[i].action
        );

        if (btn) {
            PON_child(app_data->root, btn);
            y_cursor += ITEM_HEIGHT + 4 + PADDING;
        }
    }
}

extern char* g_wallpaper_path;

static void page_wllp(Window* win, PrefsData* app_data) {
    int y_cursor = PADDING;
    
    PON_Comp* header = PANEL(PADDING, y_cursor, app_data->root->width - (2 * PADDING), 40, COLOR_BG_LIGHT);
    if (header) {
        header->draw = headerP;
        PON_child(app_data->root, header);

        PON_Comp* header_text = TEXT(15, 10, "*WALLPAPERS*", COLOR_TEXT_PRIMARY);
        if (header_text) PON_child(header, header_text);
        
        PON_Comp* back_btn = BUTTON(header->width - 40, y_cursor, 36, ITEM_HEIGHT, "<", on_nav_back);
        if (back_btn) PON_child(header, back_btn);

        y_cursor += 40 + PADDING;
    }

    Process* p = get_process(win->pid);
    if (p && p->wallpaper_files) {
        for (uint32 i = 0; i < p->wallpaper_files->count; i++) {
            FAT_FileEntry* entry = &p->wallpaper_files->entries[i];
            if (strstr(entry->name, "-wllp.bmp")) {
                char display_name[64];
                san_name(display_name, entry->name, sizeof(display_name));

                PON_Comp* btn = BUTTON(PADDING, y_cursor, app_data->root->width - (2 * PADDING), ITEM_HEIGHT, display_name, on_wallpaper_click);
                if (btn) {
                    btn->appdata = entry->name;

                    if (g_wallpaper_path && strstr(g_wallpaper_path, entry->name)) {
                        btn->selected = TRUE;
                    }

                    PON_child(app_data->root, btn);
                    y_cursor += ITEM_HEIGHT + 2;
                }
            }
        }
    }
}

static void page_systech(Window* win, PrefsData* app_data) {
    int y_cursor = PADDING;
    
    PON_Comp* header = PANEL(PADDING, y_cursor, app_data->root->width - (2 * PADDING), 40, COLOR_BG_LIGHT);
    if (header) {
        header->draw = headerP;
        PON_child(app_data->root, header);

        PON_Comp* header_text = TEXT(15, 10, "*SPECS*", COLOR_TEXT_PRIMARY);
        if (header_text) PON_child(header, header_text);
        
        PON_Comp* back_btn = BUTTON(header->width - 40, y_cursor, 36, ITEM_HEIGHT, "<", on_nav_back);
        if (back_btn) PON_child(header, back_btn);

        y_cursor += 40 + PADDING;
    }

    uint32 total_blocks = pmm_get_max_blocks();
    uint32 used_blocks = pmm_get_used_blocks();
    uint32 total_mem_mb = total_blocks / 256;
    uint32 used_mem_mb = used_blocks / 256;
    uint32 free_mem_mb = total_mem_mb - used_mem_mb;

    char mem_str[64], mem_uf_str[64];
    sprintf(mem_str, "RAM: %u MB", total_mem_mb);
    sprintf(mem_uf_str, "(%u MB used | %u MB free)", used_mem_mb, free_mem_mb);
    
    PON_Comp* mem_text = TEXT(PADDING * 2, y_cursor, mem_str, COLOR_TEXT_PRIMARY);
    if (mem_text) PON_child(app_data->root, mem_text);
    y_cursor += ITEM_HEIGHT;

    PON_Comp* mem_uf_text = TEXT(PADDING * 2, y_cursor, mem_uf_str, COLOR_ACCENT_HOVER);
    if (mem_uf_text) PON_child(app_data->root, mem_uf_text);
    y_cursor += ITEM_HEIGHT;

    y_cursor += PADDING - 4;

    PON_Comp* disk_header = TEXT(PADDING * 2, y_cursor, "*Storage Devices*", COLOR_TEXT_PRIMARY);
    if (disk_header) PON_child(app_data->root, disk_header);
    y_cursor += ITEM_HEIGHT;

    for (int i = 0; i < MAXIMUM_IDE_DEVICES; i++) {
        if (g_ide_devices[i].reserved) {
            char disk_str[128];
            uint32 disk_size_mb = g_ide_devices[i].size / 2048; // sectors * 512 / 1024 / 1024
            sprintf(disk_str, " - %s (%u MB)", g_ide_devices[i].model, disk_size_mb);
            
            PON_Comp* disk_text = TEXT(PADDING * 2, y_cursor, disk_str, COLOR_JEM);
            if (disk_text) PON_child(app_data->root, disk_text);
            y_cursor += ITEM_HEIGHT;
        }
    }

    y_cursor += PADDING / 2;

    uint64 total_disk_bytes = (uint64)g_ide_devices[0].size * 512;
    uint64 free_disk_bytes = fat_get_free_space();
    uint64 used_disk_bytes = total_disk_bytes - free_disk_bytes;

    int meter_w = app_data->root->width - (4 * PADDING);
    int meter_h = 8;
    PON_Comp* meter_bg = PANEL(PADDING * 2, y_cursor, meter_w, meter_h, COLOR_BG_DARK);
    if (meter_bg) {
        PON_child(app_data->root, meter_bg);
        int fill_w = (int)((used_disk_bytes * (uint64)meter_w) / total_disk_bytes);
        if (fill_w > meter_w) fill_w = meter_w;

        PON_Comp* meter_fill = PANEL(0, 0, fill_w, meter_h, COLOR_JEM);
        if (meter_fill) PON_child(meter_bg, meter_fill);
        y_cursor += meter_h + PADDING;
    }

    char usage_str[64];
    uint32 used_mb = (uint32)(used_disk_bytes / (1024 * 1024));
    uint32 total_mb = (uint32)(total_disk_bytes / (1024 * 1024));
    sprintf(usage_str, "%u MB / %u MB", used_mb, total_mb);
    PON_Comp* usage_text = TEXT(meter_w - strlen(usage_str) * 9, y_cursor - 6, usage_str, COLOR_TEXT_MUTED);
    if (usage_text) PON_child(app_data->root, usage_text);
}

static void nav_to(int pid, PrefsPage page, BOOL push) {
    Process* p = get_process(pid);
    if (!p) return;
    PrefsData* app_data = (PrefsData*)p->data;
    if (!app_data) return;

    Window* win = get_win_by_pid(pid);
    if (!win) return;

    if (app_data->root) {
        if (app_data->root_to_free) {
            PON_free(app_data->root_to_free);
            app_data->root_to_free = NULL;
        }
        app_data->root_to_free = app_data->root;
        app_data->root = NULL;
    }

    if (push && app_data->stack_depth < MAX_STACK) {
        app_data->stack[app_data->stack_depth++] = page;
    }

    app_data->root = PANEL(4, 4, win->width - 8 - (2 * WIN_BORDER), win->height - 8 - TITLEBAR_H - WIN_BORDER, COLOR_BG_MEDIUM);
    
    switch(page) {
        case PAGE_ABOUT: page_about(win, app_data); break;
        case PAGE_WALLPAPER: page_wllp(win, app_data); break;
        case PAGE_SYSTECH: page_systech(win, app_data); break;
    }

    is_dirty(TRUE);
}

void launch_preferences() {
    Process* p = create_process("sysprefs");
    if (!p) return;

    PrefsData* app_data = (PrefsData*)kmalloc(sizeof(PrefsData));
    if (!app_data) {
        cleanup_process(p->pid);
        return;
    }
    memset(app_data, 0, sizeof(PrefsData));
    p->data = (char*)app_data;

    FAT_dirent* dir_entry = fat_find_file("/SYSTEM/");
    if (dir_entry) {
        uint32 cluster = (dir_entry->FirstClusterHigh << 16) | dir_entry->FirstClusterLow;
        p->wallpaper_files = fat_get_dir_ls(cluster);
        kfree(dir_entry);
    }

    int win_w = 320;
    int win_h = 250;
    Window* w = window(p->pid, "Preferences", -1, -1, win_w, win_h);
    if (!w) {
        cleanup(NULL);
        cleanup_process(p->pid);
        return;
    }

    w->content_renderer = prefs;
    w->on_click = NULL;
    w->on_mouse_down = pref_m_down;
    w->on_mouse_up = m_up;
    w->on_mouse_move = m_move;
    w->on_scroll = on_scroll;
    w->on_key_press = manual_scroll;
    w->on_close = cleanup;

    nav_to(p->pid, PAGE_ABOUT, TRUE);
}