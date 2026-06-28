#include "apps/Casefiles.h"
#include "apps/Novella.h"
#include "apps/WAHtercolour.h"
#include "apps/CLPlayer.h"
#include "apps/CLStudio.h"
#include "procsys.h"
#include "gui.h"
#include "string.h"
#include "graphics.h"
#include "console.h"
#include "serial.h"
#include "kheap.h"
#include "vesa.h"
#include "fat32.h"
#include "bmp.h"
#include "kronii.h"

#define WIN_BORDER 2
#define TITLEBAR_H 20
#define BUTTON_HEIGHT 28
#define BUTTON_COUNT 5
#define SCROLLBAR_WIDTH 12
#define PREVIEW_WIDTH 190
#define DIALOG_WIDTH 280
#define DIALOG_HEIGHT 140

#define COLOR_BG_DARK RGB(28, 20, 12)
#define COLOR_BG_MEDIUM RGB(42, 32, 22)
#define COLOR_BG_LIGHT RGB(58, 46, 32)
#define COLOR_BG_ELEVATED RGB(72, 58, 40)
#define COLOR_ACCENT_PRIMARY RGB(210, 170, 95)
#define COLOR_ACCENT_SECONDARY RGB(160, 120, 70)
#define COLOR_ACCENT_HOVER RGB(230, 190, 120)
#define COLOR_TEXT_PRIMARY RGB(245, 235, 220)
#define COLOR_TEXT_SECONDARY RGB(210, 190, 160)
#define COLOR_TEXT_MUTED RGB(150, 130, 110)
#define COLOR_BORDER RGB(50, 50, 58)
#define COLOR_SELECTION RGB(237, 197, 96)
#define COLOR_SELECTION_BG RGB(50, 46, 35)
#define COLOR_SCROLLBAR_TRACK RGB(28, 28, 32)
#define COLOR_SCROLLBAR_THUMB RGB(60, 60, 68)
#define COLOR_SCROLLBAR_HOVER RGB(80, 80, 90)
#define COLOR_DELETE_BTN RGB(180, 60, 60)
#define COLOR_DELETE_HOVER RGB(200, 80, 80)
#define COLOR_DISABLED RGB(50, 42, 40)
#define COLOR_OPEN_BTN RGB(171, 114, 22)

typedef enum {
    DIALOG_MODE_NONE,
    DIALOG_MODE_NEW_FILE,
    DIALOG_MODE_NEW_DIR
} DialogMode;

typedef struct {
    int y_offset;
    int max_y_offset;
    int content_height;
    int view_height;
    
    BOOL is_dragging;
    int drag_start_y;
    int drag_start_y_offset;
    BOOL is_hovered;

    int bar_x, bar_y, bar_width, bar_height;
    int thumb_x, thumb_y, thumb_width, thumb_height;
} ScrollView;

typedef struct {
    int x, y, width, height;
    char* text;
    uint32 normal_color;
    uint32 hover_color;
    void (*action)(Window* win);
    BOOL enabled;
} CaseBtn;

typedef struct {
    FAT_DirList* dir_list;
    int selected_file_index;
    Bitmap* preview_bmp;
    Bitmap* pebble_icon;
    char** pebble_names;
    CaseBtn buttons[BUTTON_COUNT];
    int mouse_hover_button_index;

    ScrollView scroll_view;
    
    DialogMode dialog_mode;
    char dialog_input_buffer[MAX_FILENAME_LEN];
    int dialog_input_pos;

    uint32 current_cluster;
    char current_path[256];

    //double click (scuffed)
    uint32 last_click_ticks;
    int last_click_index;
} CasefilesAppData;

void newfile(Window* win);
void newdir(Window* win);
void shred(Window* win);
void open_selected(Window* win);
void navigate_up(Window* win);

void draw_rounded_rect_outline(int x, int y, int width, int height, uint32 color) {
    rect(x + 1, y, width - 2, 1, color);
    rect(x + 1, y + height - 1, width - 2, 1, color);
    
    rect(x, y + 1, 1, height - 2, color);
    rect(x + width - 1, y + 1, 1, height - 2, color);
}

void draw_subtle_shadow(int x, int y, int width, int height) {
    rect(x + 2, y + height, width - 2, 1, RGB(8, 8, 10));
    rect(x + 3, y + height + 1, width - 4, 1, RGB(12, 12, 14));
    rect(x + width, y + 2, 1, height - 2, RGB(8, 8, 10));
}

void free_casefiles_preview_bmp(CasefilesAppData* app_data) {
    if (app_data && app_data->preview_bmp) {
        free_bmp(app_data->preview_bmp);
        app_data->preview_bmp = NULL;
    }
}

static char* get_jdon_field(const char* jdon, const char* field) {
    if (!jdon || !field) return NULL;
    char search[128];
    snprintf(search, 128, "\"%s\"", field);
    char* pos = strstr(jdon, search);
    if (!pos) return NULL;
    
    pos = strstr(pos + strlen(search), ":");
    if (!pos) return NULL;
    
    pos = strstr(pos + 1, "\"");
    if (!pos) return NULL;
    
    char* start = pos + 1;
    char* end = strstr(start, "\"");
    if (!end) return NULL;
    
    int len = end - start;
    char* res = kmalloc(len + 1);
    memcpy(res, start, len);
    res[len] = '\0';
    return res;
}

void casefiles_refresh_dirlist(CasefilesAppData* app_data) {
    if (app_data->dir_list) {
        if (app_data->pebble_names) {
            for (uint32 i = 0; i < app_data->dir_list->count; i++) {
                if (app_data->pebble_names[i]) kfree(app_data->pebble_names[i]);
            }
            kfree(app_data->pebble_names);
            app_data->pebble_names = NULL;
        }
        if (app_data->dir_list->entries) {
            kfree(app_data->dir_list->entries);
        }
        kfree(app_data->dir_list);
    }
    app_data->dir_list = fat_get_dir_ls(app_data->current_cluster);
    app_data->selected_file_index = -1;
    
    if (app_data->dir_list && app_data->dir_list->count > 0) {
        app_data->pebble_names = (char**)kmalloc(sizeof(char*) * app_data->dir_list->count);
        memset(app_data->pebble_names, 0, sizeof(char*) * app_data->dir_list->count);
        
        for (uint32 i = 0; i < app_data->dir_list->count; i++) {
            FAT_FileEntry* entry = &app_data->dir_list->entries[i];
            if ((entry->attributes & 0x10) && 
                (strstr(entry->name, ".pebble") != NULL || strstr(entry->name, ".PEBBLE") != NULL)) {
                
                char manifest_path[512];
                if (strcmp(app_data->current_path, "/") == 0) {
                    snprintf(manifest_path, 512, "/%s/manifest.jdon", entry->name);
                } else {
                    snprintf(manifest_path, 512, "%s/%s/manifest.jdon", app_data->current_path, entry->name);
                }
                
                char* content = fat_read_file(manifest_path);
                if (content) {
                    app_data->pebble_names[i] = get_jdon_field(content, "name");
                    kfree(content);
                }
            }
        }
    }

    free_casefiles_preview_bmp(app_data);
    is_dirty(TRUE);
}

void navigate_to(CasefilesAppData* app_data, uint32 cluster, const char* name) {
    if (cluster == 0) cluster = 2;
    app_data->current_cluster = cluster;
    
    if (cluster == 2) {
        strcpy(app_data->current_path, "/");
    } else if (name) {
        if (strcmp(name, "..") == 0) {
            char* last_slash = strrchr(app_data->current_path, '/');
            if (last_slash == app_data->current_path) {
                app_data->current_path[1] = '\0';
            } else if (last_slash) {
                *last_slash = '\0';
            }
        } else if (strcmp(name, ".") != 0) {
            if (app_data->current_path[strlen(app_data->current_path)-1] != '/') {
                strcat(app_data->current_path, "/");
            }
            strcat(app_data->current_path, name);
        }
    }
    
    app_data->scroll_view.y_offset = 0;
    casefiles_refresh_dirlist(app_data);
}

void open_selected(Window* win) {
    Process* p = get_process(win->pid);
    CasefilesAppData* app_data = (CasefilesAppData*)p->data;
    if (app_data->selected_file_index >= 0) {
        FAT_FileEntry* entry = &app_data->dir_list->entries[app_data->selected_file_index];
        char path[512];
        if (strcmp(app_data->current_path, "/") == 0) {
            strcpy(path, "/");
            strcat(path, entry->name);
        } else {
            strcpy(path, app_data->current_path);
            if (path[strlen(path)-1] != '/') strcat(path, "/");
            strcat(path, entry->name);
        }

        if (entry->attributes & 0x10) {
            if (strstr(entry->name, ".pebble") != NULL || strstr(entry->name, ".PEBBLE") != NULL) {
                launch_program(path);
            } else {
                navigate_to(app_data, entry->cluster, entry->name);
            }
        } else {
            char* ext = strrchr(entry->name, '.');
            if (ext) {
                if (strcmp(ext, ".TXT") == 0 || strcmp(ext, ".txt") == 0) {
                    launch_novella(path, 0);
                } else if (strcmp(ext, ".BX2") == 0 || strcmp(ext, ".bx2") == 0) {
                    launch_novella(path, NOVELLA_FLAG_BAUDOL);
                } else if (strcmp(ext, ".bmp") == 0 || strcmp(ext, ".wah") == 0 || strcmp(ext, ".BMP") == 0 || strcmp(ext, ".WAH") == 0) {
                    launch_wahtercolour(path);
                } else if (strcmp(ext, ".WAV") == 0 || strcmp(ext, ".wav") == 0 || strcmp(ext, ".MP3") == 0 || strcmp(ext, ".mp3") == 0) {
                    launch_clp(path);
                } else if (strcmp(ext, ".RYS") == 0 || strcmp(ext, ".rys") == 0) {
                    launch_cls(path);
                }
            }
        }
    }
}

void navigate_up(Window* win) {
    Process* p = get_process(win->pid);
    CasefilesAppData* app_data = (CasefilesAppData*)p->data;
    if (app_data->current_cluster != 2) {
        for (uint32 i = 0; i < app_data->dir_list->count; i++) {
            if (strcmp(app_data->dir_list->entries[i].name, "..") == 0) {
                navigate_to(app_data, app_data->dir_list->entries[i].cluster, "..");
                return;
            }
        }
        navigate_to(app_data, 2, NULL);
    }
}

void update_scrollbar_geometry(CasefilesAppData* app_data, int list_width, int list_height) {
    ScrollView* sv = &app_data->scroll_view;
    sv->view_height = list_height - 36 - 32;
    
    int line_height = 20;
    sv->content_height = app_data->dir_list->count * line_height;
    sv->max_y_offset = (sv->content_height > sv->view_height) ? (sv->content_height - sv->view_height) : 0;
    
    if (sv->y_offset > sv->max_y_offset) {
        sv->y_offset = sv->max_y_offset;
    }
    
    int up_btn_reserved = 36;
    sv->bar_x = list_width - SCROLLBAR_WIDTH - 8;
    sv->bar_y = 8;
    sv->bar_width = SCROLLBAR_WIDTH;
    sv->bar_height = list_height - sv->bar_y - up_btn_reserved;

    if (sv->content_height > 0) {
        sv->thumb_height = (sv->bar_height * sv->view_height) / sv->content_height;
        if (sv->thumb_height < 30) sv->thumb_height = 30;
        if (sv->thumb_height > sv->bar_height) sv->thumb_height = sv->bar_height;
    } else {
        sv->thumb_height = sv->bar_height;
    }
    
    if (sv->max_y_offset > 0) {
        sv->thumb_y = sv->bar_y + (sv->y_offset * (sv->bar_height - sv->thumb_height)) / sv->max_y_offset;
    } else {
        sv->thumb_y = sv->bar_y;
    }
    sv->thumb_x = sv->bar_x;
    sv->thumb_width = sv->bar_width;
}

// i gotta replace this bullshit with PON API someday, do in v3.1
void casefiles_draw_dialog(Window* win, CasefilesAppData* app_data) {
    int dialog_x = win->x + (win->width - DIALOG_WIDTH) / 2;
    int dialog_y = win->y + (win->height - DIALOG_HEIGHT) / 2;
    
    rect(dialog_x + 3, dialog_y + 3, DIALOG_WIDTH, DIALOG_HEIGHT, RGB(8, 8, 10));
    
    rect(dialog_x, dialog_y, DIALOG_WIDTH, DIALOG_HEIGHT, COLOR_BG_ELEVATED);
    draw_rounded_rect_outline(dialog_x, dialog_y, DIALOG_WIDTH, DIALOG_HEIGHT, COLOR_BORDER);
    
    rect(dialog_x + 1, dialog_y + 1, DIALOG_WIDTH - 2, 35, COLOR_BG_LIGHT);
    rect(dialog_x + 1, dialog_y + 1, 4, 35, COLOR_ACCENT_PRIMARY);

    char* title = (app_data->dialog_mode == DIALOG_MODE_NEW_FILE) ? "create a file" : "create a folder";
    text(title, dialog_x + 15, dialog_y + 12, COLOR_TEXT_PRIMARY, FONT_DEFAULT, FALSE);
    
    char* label = (app_data->dialog_mode == DIALOG_MODE_NEW_FILE) ? "filename:" : "foldername:";
    text(label, dialog_x + 20, dialog_y + 50, COLOR_TEXT_SECONDARY, FONT_DEFAULT, FALSE);
    
    rect(dialog_x + 20, dialog_y + 68, DIALOG_WIDTH - 40, 26, COLOR_BG_DARK);
    draw_rounded_rect_outline(dialog_x + 20, dialog_y + 68, DIALOG_WIDTH - 40, 26, COLOR_BORDER);
    
    text(app_data->dialog_input_buffer, dialog_x + 26, dialog_y + 75, COLOR_TEXT_PRIMARY, FONT_DEFAULT, FALSE);
    
    if (app_data->dialog_input_pos >= 0) {
        int cursor_x = dialog_x + 26 + (app_data->dialog_input_pos * 8);
        rect(cursor_x, dialog_y + 75, 1, 12, COLOR_ACCENT_PRIMARY);
    }
    
    int btn_y = dialog_y + DIALOG_HEIGHT - 38;
    
    rect(dialog_x + 20, btn_y, 80, 26, COLOR_BG_LIGHT);
    draw_rounded_rect_outline(dialog_x + 20, btn_y, 80, 26, COLOR_BORDER);
    text("Cancel", dialog_x + 38, btn_y + 8, COLOR_TEXT_SECONDARY, FONT_DEFAULT, FALSE);
    
    rect(dialog_x + DIALOG_WIDTH - 100, btn_y, 80, 26, COLOR_ACCENT_SECONDARY);
    draw_rounded_rect_outline(dialog_x + DIALOG_WIDTH - 100, btn_y, 80, 26, COLOR_ACCENT_PRIMARY);
    text("Create", dialog_x + DIALOG_WIDTH - 82, btn_y + 8, COLOR_BG_DARK, FONT_DEFAULT, FALSE);
}

void casefiles(Window* win) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    CasefilesAppData* app_data = (CasefilesAppData*)p->data;
    if (!app_data || !app_data->dir_list) return;

    int content_x = win->x + WIN_BORDER;
    int content_y = win->y + TITLEBAR_H;
    int content_width = win->width - (2 * WIN_BORDER);
    int content_height = win->height - TITLEBAR_H - WIN_BORDER;

    int list_width = content_width - PREVIEW_WIDTH - 8;
    int list_height = content_height;

    rect(content_x, content_y, content_width, content_height, COLOR_BG_DARK);

    rect(content_x + 4, content_y + 4, list_width - 8, list_height - 8, COLOR_BG_MEDIUM);
    draw_rounded_rect_outline(content_x + 4, content_y + 4, list_width - 8, list_height - 8, COLOR_BORDER);

    int preview_x = content_x + list_width;
    rect(preview_x + 4, content_y + 4, PREVIEW_WIDTH - 8, list_height - 8, COLOR_BG_ELEVATED);
    draw_rounded_rect_outline(preview_x + 4, content_y + 4, PREVIEW_WIDTH - 8, list_height - 8, COLOR_BORDER);

    char path_label[300] = "PATH > ";
    strcat(path_label, app_data->current_path);
    text(path_label, content_x + 12, content_y + 8, COLOR_TEXT_SECONDARY, FONT_DEFAULT, FALSE);

    int line_height = 20;
    int start_y = content_y + 36 - app_data->scroll_view.y_offset;
    int list_area_x = content_x + 8;
    int list_area_width = list_width - SCROLLBAR_WIDTH - 24;

    for (uint32 i = 0; i < app_data->dir_list->count; i++) {
        int current_y = start_y + (i * line_height);
        if (current_y + line_height < content_y + 32 || current_y + line_height > content_y + list_height - 32) {
            continue;
        }

        FAT_FileEntry* entry = &app_data->dir_list->entries[i];
        uint32 text_color = COLOR_TEXT_PRIMARY;
        uint32 icon_color = COLOR_TEXT_MUTED;

        if (i == (uint32)app_data->selected_file_index) {
            rect(list_area_x, current_y - 2, list_area_width, line_height - 2, COLOR_SELECTION_BG);
            rect(list_area_x, current_y - 2, 3, line_height - 2, COLOR_ACCENT_PRIMARY);
            text_color = COLOR_SELECTION;
        }

        BOOL is_pebble = FALSE;
        if (entry->attributes & 0x10) {
            is_pebble = (strstr(entry->name, ".pebble") != NULL || strstr(entry->name, ".PEBBLE") != NULL);
        }

        if (is_pebble && app_data->pebble_icon) {
            draw_objbmp(app_data->pebble_icon, list_area_x + 12, current_y - 1, 16, 16);
            char* display_name = (app_data->pebble_names && app_data->pebble_names[i]) ? app_data->pebble_names[i] : entry->name;
            text(display_name, list_area_x + 38, current_y - 2, text_color, FONT_DEFAULT, FALSE);
        } else if (entry->attributes & 0x10) {
            text_color = (i == (uint32)app_data->selected_file_index) ? COLOR_ACCENT_HOVER : COLOR_ACCENT_SECONDARY;
            icon_color = text_color;
            text("[d]", list_area_x + 8, current_y, icon_color, FONT_DEFAULT, TRUE); // TODO: replace this bs with icons
            text(entry->name, list_area_x + 38, current_y - 2, text_color, FONT_DEFAULT, FALSE);
        } else {
            text("[f]", list_area_x + 8, current_y, icon_color, FONT_DEFAULT, TRUE);
            text(entry->name, list_area_x + 38, current_y - 2, text_color, FONT_DEFAULT, FALSE);
        }
    }
    
    update_scrollbar_geometry(app_data, list_width, list_height);
    ScrollView* sv = &app_data->scroll_view;

    rect(content_x + sv->bar_x, content_y + sv->bar_y, sv->bar_width, sv->bar_height, COLOR_SCROLLBAR_TRACK);
    
    uint32 thumb_color = sv->is_hovered ? COLOR_SCROLLBAR_HOVER : COLOR_SCROLLBAR_THUMB;
    rect(content_x + sv->thumb_x + 2, content_y + sv->thumb_y + 2, sv->thumb_width - 4, sv->thumb_height - 4, thumb_color);

    int preview_content_x = preview_x + 12;
    int preview_content_y = content_y + 12;
    
    char fn_display[MAX_FILENAME_LEN + 1];
    if (app_data->selected_file_index >= 0 && (uint32)app_data->selected_file_index < app_data->dir_list->count) {
        if (app_data->pebble_names && app_data->pebble_names[app_data->selected_file_index]) {
            strncpy(fn_display, app_data->pebble_names[app_data->selected_file_index], MAX_FILENAME_LEN);
        } else {
            FAT_FileEntry* entry = &app_data->dir_list->entries[app_data->selected_file_index];
            strncpy(fn_display, entry->name, MAX_FILENAME_LEN);
            char* dot = strrchr(fn_display, '.');
            if (dot) {
                *dot = '\0';
            }
        }
        fn_display[MAX_FILENAME_LEN] = '\0';
    } else {
        strcpy(fn_display, "PREVIEW");
    }

    text(fn_display, preview_content_x, preview_content_y, COLOR_TEXT_SECONDARY, FONT_DEFAULT, TRUE);
    
    int preview_img_y = preview_content_y + 24;
    int preview_img_size = PREVIEW_WIDTH - 24;
    
    if (app_data->preview_bmp) {
        int bmp_w = app_data->preview_bmp->width;
        int bmp_h = app_data->preview_bmp->height;
        int target_w = bmp_w;
        int target_h = bmp_h;

        // scale
        if (target_w > preview_img_size || target_h > preview_img_size) {
            if (bmp_w > bmp_h) {
                target_w = preview_img_size;
                target_h = (bmp_h * preview_img_size) / bmp_w;
            } else {
                target_h = preview_img_size;
                target_w = (bmp_w * preview_img_size) / bmp_h;
            }
        }

        int draw_x = preview_content_x + (preview_img_size - target_w) / 2;
        int draw_y = preview_img_y + (preview_img_size - target_h) / 2;

        rect(preview_content_x, preview_img_y, preview_img_size, preview_img_size, COLOR_BG_DARK);
        draw_objbmp(app_data->preview_bmp, draw_x, draw_y, target_w, target_h);
        draw_rounded_rect_outline(preview_content_x, preview_img_y, preview_img_size, preview_img_size, COLOR_BORDER);
    } else {
        rect(preview_content_x, preview_img_y, preview_img_size, preview_img_size, COLOR_BG_DARK);
        draw_rounded_rect_outline(preview_content_x, preview_img_y, preview_img_size, preview_img_size, COLOR_BORDER);
        
        text("No Preview", preview_content_x + 28, preview_img_y + preview_img_size / 2 - 6, COLOR_TEXT_MUTED, FONT_DEFAULT, FALSE);
    }

    int up_btn_w = 60;
    app_data->buttons[1].x = content_x + 180;
    app_data->buttons[1].y = content_y + list_height - 32;
    app_data->buttons[1].width = up_btn_w;
    app_data->buttons[1].height = 24;

    CaseBtn* ubtn = &app_data->buttons[1];
    uint32 ubg = ubtn->normal_color;
    if (app_data->mouse_hover_button_index == 1) ubg = ubtn->hover_color;
    rect(ubtn->x - 175, ubtn->y - 2, 240, ubtn->height + 4, COLOR_ACCENT_SECONDARY);
    rect(ubtn->x, ubtn->y, ubtn->width, ubtn->height, ubg);
    draw_rounded_rect_outline(ubtn->x, ubtn->y, ubtn->width, ubtn->height, COLOR_BORDER);
    text(ubtn->text, ubtn->x + 22, ubtn->y + 4, COLOR_TEXT_PRIMARY, FONT_DEFAULT, FALSE);

    int button_start_y = preview_img_y + preview_img_size + 6;
    int button_width = PREVIEW_WIDTH - 24;
    int rendered_btn_idx = 0;

    for (int i = 0; i < BUTTON_COUNT; i++) {
        if (i == 1) continue;

        CaseBtn* btn = &app_data->buttons[i];
        btn->x = preview_content_x;
        btn->y = button_start_y + (rendered_btn_idx * (BUTTON_HEIGHT + 2));
        btn->width = button_width;
        btn->height = BUTTON_HEIGHT;
        rendered_btn_idx++;

        uint32 bg_color = btn->normal_color;
        uint32 border_color = COLOR_BORDER;
        uint32 text_color = COLOR_TEXT_PRIMARY;
        
        if (!btn->enabled) {
            bg_color = COLOR_DISABLED;
            text_color = COLOR_TEXT_MUTED;
        } else if (i == app_data->mouse_hover_button_index) {
            bg_color = btn->hover_color;
            border_color = (i == 2) ? COLOR_DELETE_HOVER : COLOR_ACCENT_PRIMARY;
        }

        rect(btn->x, btn->y, btn->width, btn->height, bg_color);
        draw_rounded_rect_outline(btn->x, btn->y, btn->width, btn->height, border_color);
        
        int text_offset = (btn->width - (strlen(btn->text) * 8)) / 2;
        text(btn->text, btn->x + text_offset, btn->y + 8, text_color, FONT_DEFAULT, FALSE);
    }
    
    if (app_data->dialog_mode != DIALOG_MODE_NONE) {
        casefiles_draw_dialog(win, app_data);
    }
}

void preview(CasefilesAppData* app_data) {
    free_casefiles_preview_bmp(app_data);
    if (app_data->selected_file_index < 0 || (uint32)app_data->selected_file_index >= app_data->dir_list->count) {
        return;
    }

    FAT_FileEntry* entry = &app_data->dir_list->entries[app_data->selected_file_index];
    char* ext = strrchr(entry->name, '.');
    if (ext && ((strcmp(ext, ".BMP") == 0 || strcmp(ext, ".bmp") == 0) ||
                (strcmp(ext, ".WAH") == 0 || strcmp(ext, ".wah") == 0) ||
                (strcmp(ext, ".RYS") == 0 || strcmp(ext, ".rys") == 0))) {
        char path[512];
        if (strcmp(app_data->current_path, "/") == 0) {
            strcpy(path, "/");
            strcat(path, entry->name);
        } else {
            strcpy(path, app_data->current_path);
            if (path[strlen(path)-1] != '/') strcat(path, "/");
            strcat(path, entry->name);
        }
        if (strcmp(ext, ".BMP") == 0 || strcmp(ext, ".bmp") == 0) {
            app_data->preview_bmp = load_bmp(path);
        } else if (strcmp(ext, ".WAH") == 0 || strcmp(ext, ".wah") == 0) {
            app_data->preview_bmp = load_wah_image(path);
        } else {
            app_data->preview_bmp = NULL;
        }
    }
}

void casefiles_on_click(Window* win, int click_x, int click_y) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    CasefilesAppData* app_data = (CasefilesAppData*)p->data;
    if (!app_data) return;

    if (app_data->dialog_mode != DIALOG_MODE_NONE) {
        int dialog_x_abs = win->x + (win->width - DIALOG_WIDTH) / 2;
        int dialog_y_abs = win->y + (win->height - DIALOG_HEIGHT) / 2;
        int click_x_rel = click_x + win->x + WIN_BORDER;
        int click_y_rel = click_y + win->y + TITLEBAR_H + WIN_BORDER;
        
        int btn_y = dialog_y_abs + DIALOG_HEIGHT - 38;
        
        if (click_x_rel > dialog_x_abs + DIALOG_WIDTH - 100 && click_x_rel < dialog_x_abs + DIALOG_WIDTH - 20 &&
            click_y_rel > btn_y && click_y_rel < btn_y + 26) {
            
            if (app_data->dialog_input_pos > 0) {
                char path[512];
                if (strcmp(app_data->current_path, "/") == 0) {
                    strcpy(path, "/");
                    strcat(path, app_data->dialog_input_buffer);
                } else {
                    strcpy(path, app_data->current_path);
                    strcat(path, "/");
                    strcat(path, app_data->dialog_input_buffer);
                }

                if (app_data->dialog_mode == DIALOG_MODE_NEW_FILE) {
                    fat_create_file(path);
                } else if (app_data->dialog_mode == DIALOG_MODE_NEW_DIR) {
                    fat_create_dir(path);
                }
                casefiles_refresh_dirlist(app_data);
            }
            app_data->dialog_mode = DIALOG_MODE_NONE;
            win->on_key_press = NULL;
        }
        else if (click_x_rel > dialog_x_abs + 20 && click_x_rel < dialog_x_abs + 100 &&
            click_y_rel > btn_y && click_y_rel < btn_y + 26) {
            app_data->dialog_mode = DIALOG_MODE_NONE;
            win->on_key_press = NULL;
        }
        return;
    }

    int content_width = win->width - (2 * WIN_BORDER);
    int list_width = content_width - PREVIEW_WIDTH - 8;
    
    for (int i = 0; i < BUTTON_COUNT; i++) {
       CaseBtn* btn = &app_data->buttons[i];
       if (click_x + win->x + WIN_BORDER >= btn->x && 
           click_x + win->x + WIN_BORDER < btn->x + btn->width &&
           click_y + win->y + TITLEBAR_H + WIN_BORDER >= btn->y && 
           click_y + win->y + TITLEBAR_H + WIN_BORDER < btn->y + btn->height) {
           if (btn->enabled && btn->action) {
               btn->action(win);
               return;
           }
       }
    }
    
    if (click_x < list_width - SCROLLBAR_WIDTH - 16) {
        int line_height = 20;
        int y_in_list = click_y - 36;

        if (y_in_list >= 0) {
            int clicked_line_index = (y_in_list + app_data->scroll_view.y_offset) / line_height;

            if (clicked_line_index >= 0 && (uint32)clicked_line_index < app_data->dir_list->count) {
                uint32 current_ticks = timer_ticks;
                if (clicked_line_index == app_data->last_click_index && 
                    (current_ticks - app_data->last_click_ticks) < 20) {
                    app_data->selected_file_index = clicked_line_index;
                    open_selected(win);
                    app_data->last_click_ticks = 0;
                    return;
                }
                
                app_data->last_click_index = clicked_line_index;
                app_data->last_click_ticks = current_ticks;

                app_data->selected_file_index = clicked_line_index;
                FAT_FileEntry* entry = &app_data->dir_list->entries[clicked_line_index];
                BOOL is_system = strcmp(app_data->current_path, "/SYSTEM") == 0;
                if (entry->attributes & 0x10) {
                    app_data->buttons[0].enabled = TRUE;
                    app_data->buttons[4].enabled = !is_system;
                } else {
                    char* ext = strrchr(entry->name, '.');
                    BOOL supported = FALSE;
                    if (ext) {
                        if (strcmp(ext, ".TXT") == 0 || strcmp(ext, ".txt") == 0 ||
                            strcmp(ext, ".bx2") == 0 || strcmp(ext, ".BX2") == 0 ||
                            strcmp(ext, ".bmp") == 0 || strcmp(ext, ".wah") == 0 ||
                            strcmp(ext, ".WAV") == 0 || strcmp(ext, ".wav") == 0 ||
                            strcmp(ext, ".MP3") == 0 || strcmp(ext, ".mp3") == 0 ||
                            strcmp(ext, ".RYS") == 0 || strcmp(ext, ".rys") == 0) {
                            supported = TRUE;
                        }
                    }
                    app_data->buttons[0].enabled = supported;
                    
                    // nuh uh
                    BOOL is_system = strcmp(app_data->current_path, "/SYSTEM") == 0;
                    app_data->buttons[4].enabled = !is_system;
                }
                preview(app_data);
                is_dirty(TRUE);
            }
        }
    }
}

void casefiles_on_mouse_down(Window* win, int x, int y) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    CasefilesAppData* app_data = (CasefilesAppData*)p->data;

    ScrollView* sv = &app_data->scroll_view;
    if (x >= sv->thumb_x && x < sv->thumb_x + sv->thumb_width &&
        y >= sv->thumb_y && y < sv->thumb_y + sv->thumb_height) {
        
        sv->is_dragging = TRUE;
        sv->drag_start_y = y;
        sv->drag_start_y_offset = sv->y_offset;
    }
}

void casefiles_on_mouse_up(Window* win, int x __attribute__((unused)), int y __attribute__((unused))) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    CasefilesAppData* app_data = (CasefilesAppData*)p->data;
    app_data->scroll_view.is_dragging = FALSE;
}

void casefiles_on_mouse_move(Window* win, int x, int y) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    CasefilesAppData* app_data = (CasefilesAppData*)p->data;
    ScrollView* sv = &app_data->scroll_view;
    
    BOOL was_hovered = sv->is_hovered;
    sv->is_hovered = (x >= sv->thumb_x && x < sv->thumb_x + sv->thumb_width &&
                      y >= sv->thumb_y && y < sv->thumb_y + sv->thumb_height);
    
    if (was_hovered != sv->is_hovered) {
        is_dirty(TRUE);
    }

    int old_hover = app_data->mouse_hover_button_index;
    app_data->mouse_hover_button_index = -1;
    
    for (int i = 0; i < BUTTON_COUNT; i++) {
        CaseBtn* btn = &app_data->buttons[i];
        if (btn->enabled && x >= btn->x - win->x - WIN_BORDER && 
            x < btn->x + btn->width - win->x - WIN_BORDER &&
            y >= btn->y - win->y - TITLEBAR_H - WIN_BORDER && 
            y < btn->y + btn->height - win->y - TITLEBAR_H - WIN_BORDER) {
            app_data->mouse_hover_button_index = i;
            break;
        }
    }
    
    if (old_hover != app_data->mouse_hover_button_index) {
        is_dirty(TRUE);
    }

    if (sv->is_dragging) {
        int dy = y - sv->drag_start_y;
        int thumb_travel = sv->bar_height - sv->thumb_height;
        int offset_delta = (thumb_travel > 0) ? (dy * sv->max_y_offset) / thumb_travel : 0;
        sv->y_offset = sv->drag_start_y_offset + offset_delta;

        if (sv->y_offset < 0) sv->y_offset = 0;
        if (sv->y_offset > sv->max_y_offset) sv->y_offset = sv->max_y_offset;
        
        is_dirty(TRUE);
    }
}

void casefiles_on_key_press(Window* win, unsigned int key) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    CasefilesAppData* app_data = (CasefilesAppData*)p->data;

    if (app_data->dialog_mode == DIALOG_MODE_NEW_FILE || app_data->dialog_mode == DIALOG_MODE_NEW_DIR) {
        if (key == '\n') {
            if (app_data->dialog_input_pos > 0) {
                char path[512];
                if (strcmp(app_data->current_path, "/") == 0) {
                    strcpy(path, "/");
                    strcat(path, app_data->dialog_input_buffer);
                } else {
                    strcpy(path, app_data->current_path);
                    strcat(path, "/");
                    strcat(path, app_data->dialog_input_buffer);
                }

                if (app_data->dialog_mode == DIALOG_MODE_NEW_FILE) {
                    fat_create_file(path);
                } else if (app_data->dialog_mode == DIALOG_MODE_NEW_DIR) {
                    fat_create_dir(path);
                }
                casefiles_refresh_dirlist(app_data);
            }
            app_data->dialog_mode = DIALOG_MODE_NONE;
            win->on_key_press = NULL;
        } else if (key == '\b') {
            if (app_data->dialog_input_pos > 0) {
                app_data->dialog_input_pos--;
                app_data->dialog_input_buffer[app_data->dialog_input_pos] = '\0';
                is_dirty(TRUE);
            }
        } else if (app_data->dialog_input_pos < MAX_FILENAME_LEN - 1 && key >= 32 && key <= 126) {
            app_data->dialog_input_buffer[app_data->dialog_input_pos++] = key;
            app_data->dialog_input_buffer[app_data->dialog_input_pos] = '\0';
            is_dirty(TRUE);
        }
    }
}

void casefiles_cleanup(Window* win) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (p && p->data) {
        CasefilesAppData* app_data = (CasefilesAppData*)p->data;
        if (app_data) {
            free_casefiles_preview_bmp(app_data);
            if (app_data->pebble_icon) {
                free_bmp(app_data->pebble_icon);
            }
            if (app_data->dir_list) {
                if (app_data->pebble_names) {
                    for (uint32 i = 0; i < app_data->dir_list->count; i++) {
                        if (app_data->pebble_names[i]) kfree(app_data->pebble_names[i]);
                    }
                    kfree(app_data->pebble_names);
                }
                if (app_data->dir_list->entries) {
                    kfree(app_data->dir_list->entries);
                }
                kfree(app_data->dir_list);
            }
            kfree(app_data);
            p->data = NULL;
        }
    }
    printf("Casefiles app resources cleaned up!\n");
}

void launch_casefiles() {
    Process* p = create_process("Casefiles");
    if (!p) return;

    CasefilesAppData* app_data = (CasefilesAppData*)kmalloc(sizeof(CasefilesAppData));
    if (!app_data) {
        cleanup_process(p->pid);
        return;
    }
    memset(app_data, 0, sizeof(CasefilesAppData));
    app_data->selected_file_index = -1;
    app_data->mouse_hover_button_index = -1;
    app_data->current_cluster = 2;
    strcpy(app_data->current_path, "/");
    app_data->pebble_icon = load_bmp("/SYSTEM/peb-ic.bmp");
    p->data = (char*)app_data;

    casefiles_refresh_dirlist(app_data);
    if (!app_data->dir_list) {
        kfree(app_data);
        p->data = NULL;
        cleanup_process(p->pid);
        return;
    }

    Window* w = window(p->pid, "Casefiles", -1, -1, 452, 360);
    if (!w) {
        kfree(app_data);
        p->data = NULL;
        cleanup_process(p->pid);
        return;
    }
    
    app_data->buttons[0] = (CaseBtn){0, 0, 0, BUTTON_HEIGHT, "OPEN", 
        COLOR_OPEN_BTN, COLOR_ACCENT_HOVER, open_selected, FALSE};
    app_data->buttons[1] = (CaseBtn){0, 0, 0, BUTTON_HEIGHT, "UP", 
        COLOR_BG_ELEVATED, COLOR_ACCENT_HOVER, navigate_up, TRUE};
    app_data->buttons[2] = (CaseBtn){0, 0, 0, BUTTON_HEIGHT, "NEW FILE", 
        COLOR_BG_ELEVATED, COLOR_ACCENT_SECONDARY, newfile, TRUE};
    app_data->buttons[3] = (CaseBtn){0, 0, 0, BUTTON_HEIGHT, "NEW FOLDER", 
        COLOR_BG_ELEVATED, COLOR_ACCENT_SECONDARY, newdir, TRUE};
    app_data->buttons[4] = (CaseBtn){0, 0, 0, BUTTON_HEIGHT, "shred", 
        COLOR_DELETE_BTN, COLOR_DELETE_HOVER, shred, FALSE};

    w->content_renderer = casefiles;
    w->on_close = casefiles_cleanup;
    w->on_click = casefiles_on_click;
    w->on_mouse_down = casefiles_on_mouse_down;
    w->on_mouse_up = casefiles_on_mouse_up;
    w->on_mouse_move = casefiles_on_mouse_move;

    kprint("[CASEFILES] launch with PID %d\n", p->pid);
}

void newfile(Window* win) {
    CasefilesAppData* app_data = (CasefilesAppData*)get_process(win->pid)->data;
    app_data->dialog_mode = DIALOG_MODE_NEW_FILE;
    app_data->dialog_input_pos = 0;
    memset(app_data->dialog_input_buffer, 0, MAX_FILENAME_LEN);
    win->on_key_press = casefiles_on_key_press;
    is_dirty(TRUE);
}

void newdir(Window* win) {
    CasefilesAppData* app_data = (CasefilesAppData*)get_process(win->pid)->data;
    app_data->dialog_mode = DIALOG_MODE_NEW_DIR;
    app_data->dialog_input_pos = 0;
    memset(app_data->dialog_input_buffer, 0, MAX_FILENAME_LEN);
    win->on_key_press = casefiles_on_key_press;
    is_dirty(TRUE);
}

void shred(Window* win) {
    CasefilesAppData* app_data = (CasefilesAppData*)get_process(win->pid)->data;
    if (app_data->selected_file_index != -1) {
        FAT_FileEntry* entry = &app_data->dir_list->entries[app_data->selected_file_index];
        char path[512];
        if (strcmp(app_data->current_path, "/") == 0) {
            strcpy(path, "/");
            strcat(path, entry->name);
        } else {
            strcpy(path, app_data->current_path);
            if (path[strlen(path)-1] != '/') strcat(path, "/");
            strcat(path, entry->name);
        }

        BOOL is_system = strcmp(app_data->current_path, "/SYSTEM") == 0;
        if (!is_system) {
            fat_delete_file(path);
            kprint("deleted %s\n", path);
        }
        casefiles_refresh_dirlist(app_data);
    }
}