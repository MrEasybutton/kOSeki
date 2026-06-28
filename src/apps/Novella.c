#include "apps/Novella.h"
#include "procsys.h"
#include "gui.h"
#include "pon.h"
#include "string.h"
#include "graphics.h"
#include "kheap.h"
#include "vesa.h"
#include "fonts.h"
#include "console.h"
#include "serial.h"
#include "keyboard.h"
#include "fat32.h"
#include "baux2/baux2.h"

#define WIN_BORDER 2
#define TITLEBAR_H 20
#define FONT FONT_KALNIA // we gonna config this maybe
#define DIALOG_WIDTH 300
#define DIALOG_HEIGHT 160
#define BUTTON_HEIGHT 32

#define NOTEBOOK_A RGB(167, 155, 186)
#define NOTEBOOK_B RGB(210,201,212)
#define INK_COLOR RGB(25,20,40)

#define COLOR_BG_DARK RGB(18, 4, 12)
#define COLOR_BG_ELEVATED RGB(38, 38, 45)
#define COLOR_BG_LIGHT RGB(32, 32, 38)
#define COLOR_ACCENT_PRIMARY RGB(160, 154, 165)
#define COLOR_ACCENT_SECONDARY RGB(200, 187, 216)
#define COLOR_ACCENT_HOVER RGB(220, 215, 228)
#define COLOR_TEXT_PRIMARY RGB(240, 240, 245)
#define COLOR_TEXT_SECONDARY RGB(180, 180, 190)
#define COLOR_BORDER RGB(50, 50, 58)

#define IDE_PANEL_WIDTH 240
#define MAX_SCRIPT_LOG_SIZE 4096

typedef enum {
    DIALOG_MODE_NONE,
    DIALOG_MODE_SAVE,
    DIALOG_MODE_LOAD
} DialogMode;

#define MAX_VISIBLE_LINES_CACHE 512

typedef struct {
    PON_Comp* root;
    PON_Comp* notebook;
    PON_Comp* status_bar;
    PON_Comp* status_text;
    PON_Comp* dialog;
    PON_Comp* dialog_input;
    PON_Comp* ide_panel;
    PON_Comp* ide_table;
    PON_Comp* ide_log;
    PON_Comp* btn_save;
    PON_Comp* btn_load;

    DialogMode dialog_mode;
    int pid;
    int base_window_width;
    char current_filename[MAX_FILENAME_LEN];
    BOOL file_modified;
    BOOL ide_mode;
    Env* debug_env;
    char run_output[MAX_SCRIPT_LOG_SIZE];
    int run_output_len;
    int log_scroll_offset;
    BOOL log_user_scrolled;

    BOOL is_dragging;
    int drag_start_y;
    int drag_start_y_offset;
    BOOL is_hovered;
    BOOL is_log_hovered;
    BOOL is_log_dragging;
    int log_drag_start_y;
    int log_drag_start_offset;
    int bar_x, bar_y, bar_width, bar_height;
    int thumb_x, thumb_y, thumb_width, thumb_height;
    int log_bar_x, log_bar_y, log_bar_width, log_bar_height;
    int log_thumb_x, log_thumb_y, log_thumb_width, log_thumb_height;

    int line_starts[MAX_VISIBLE_LINES_CACHE];
} NovellaAppData;

static void nov_key_press(Window* win, unsigned int c);
static void handle_scroll(Window* win, int scroll_delta);
static void execute_script(Window* win);
static void update_layout(NovellaAppData* app_data);
static void draw_ide_table(PON_Comp* comp, int ax, int ay);
static void draw_ide_log(PON_Comp* comp, int ax, int ay);
static void nov_baux2_print(const char* text);
static void append_output_log(NovellaAppData* app_data, const char* text);

static void sanfl(char* dest, const char* src) {
    if (!dest || !src) return;
    strcpy(dest, src);
    int len = strlen(dest);
    
    if (len >= 4 && strcmp(dest + len - 4, ".bx2") == 0) {
        return;
    }
    
    if (len < 4 || strcmp(dest + len - 4, ".txt") != 0) {
        strcat(dest, ".txt");
    }
}

static void append_output_log(NovellaAppData* app_data, const char* text) {
    if (!app_data || !text) return;

    int len = strlen(text);
    if (len <= 0) return;

    if (app_data->run_output_len + len >= MAX_SCRIPT_LOG_SIZE) {
        int overflow = (app_data->run_output_len + len) - (MAX_SCRIPT_LOG_SIZE - 1);
        if (overflow >= app_data->run_output_len) {
            app_data->run_output_len = 0;
            app_data->run_output[0] = '\0';
        } else {
            memmove(app_data->run_output, app_data->run_output + overflow, app_data->run_output_len - overflow);
            app_data->run_output_len -= overflow;
            app_data->run_output[app_data->run_output_len] = '\0';
        }
    }

    memcpy(app_data->run_output + app_data->run_output_len, text, len);
    app_data->run_output_len += len;
    app_data->run_output[app_data->run_output_len] = '\0';

    if (!app_data->log_user_scrolled) {
        int total_lines = 0;
        const char* tmp = app_data->run_output;
        if (*tmp) total_lines = 1;
        while (*tmp) {
            if (*tmp == '\n') total_lines++;
            tmp++;
        }

        const FontInfo* font_info = get_font_info(FONT);
        if (font_info && app_data->ide_log) {
            int max_visible_lines = (app_data->ide_log->height - 24) / font_info->height;
            if (max_visible_lines < 1) max_visible_lines = 1;
            int max_scroll = total_lines - max_visible_lines;
            if (max_scroll < 0) max_scroll = 0;
            app_data->log_scroll_offset = max_scroll;
        }
    }
}

static NovellaAppData* g_novella_active_debug_app = NULL;

static void nov_baux2_print(const char *text) {
    if (g_novella_active_debug_app) {
        append_output_log(g_novella_active_debug_app, text);
    }
}

static void status_update(NovellaAppData* app_data) {
    if (!app_data || !app_data->status_text) return;
    char status[128];
    strcpy(status, "> ");
    if (app_data->current_filename[0] == '\0') {
        strcat(status, "untitled");
    } else {
        strcat(status, app_data->current_filename);
        if (app_data->file_modified) strcat(status, " *");
    }
    update_str(app_data->status_text, status);
}

static void update_layout(NovellaAppData* app_data) {
    if (!app_data || !app_data->root || !app_data->notebook || !app_data->ide_panel) return;

    Process* p = get_process(app_data->pid);
    if (!p || !p->window) return;
    Window* win = p->window;

    if (app_data->ide_mode) {
        win->width = app_data->base_window_width + IDE_PANEL_WIDTH;
    } else {
        win->width = app_data->base_window_width;
    }

    int screen_w = vbe_get_width();
    if (win->x + win->width > screen_w) {
        win->x = screen_w - win->width;
        if (win->x < 0) win->x = 0;
    }
    win->frame_cache_dirty = TRUE;

    app_data->root->width = win->width - (2 * WIN_BORDER);
    app_data->status_bar->width = app_data->root->width;

    int panel_width = app_data->ide_mode ? IDE_PANEL_WIDTH : 0;
    int notebook_width = app_data->root->width - panel_width - (panel_width > 0 ? 4 : 0);
    if (notebook_width < 0) notebook_width = app_data->root->width;

    app_data->notebook->width = notebook_width;
    app_data->ide_panel->visible = app_data->ide_mode;

    if (app_data->btn_save) app_data->btn_save->x = app_data->root->width - 110;
    if (app_data->btn_load) app_data->btn_load->x = app_data->root->width - 55;

    if (app_data->ide_mode) {
        app_data->ide_panel->x = notebook_width + 4;
        app_data->ide_panel->y = 2;
        app_data->ide_panel->width = panel_width;
        app_data->ide_panel->height = app_data->root->height - 50;

        int inner_width = panel_width - 8;
        int inner_height = app_data->ide_panel->height - 12;
        int top_height = inner_height / 2;

        app_data->ide_table->x = 4;
        app_data->ide_table->y = 4;
        app_data->ide_table->width = inner_width;
        app_data->ide_table->height = top_height;

        app_data->ide_log->x = 4;
        app_data->ide_log->y = 8 + top_height;
        app_data->ide_log->width = inner_width;
        app_data->ide_log->height = inner_height - top_height;
    }
}

static void execute_script(Window* win) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    NovellaAppData* app_data = (NovellaAppData*)p->data;

    if (app_data->debug_env) {
        free_env(app_data->debug_env);
        app_data->debug_env = NULL;
    }
    app_data->run_output_len = 0;
    app_data->run_output[0] = '\0';

    int count = 0;
    Stmt **statements = parse(p->buf, &count);
    if (statements == NULL) {
        append_output_log(app_data, "[BAUx2] there was an error parsing.\n");
        is_dirty(TRUE);
        return;
    }

    Process* script_proc = create_process("BAUx2 DAEMON");
    if (!script_proc) {
        append_output_log(app_data, "[BAUx2] there was an error creating the script process.\n");
        for (int i = 0; i < count; i++) {
            free_stmt(statements[i]);
        }
        kfree(statements);
        is_dirty(TRUE);
        return;
    }

    BAUx2_d* baux2_data = (BAUx2_d*)kmalloc(sizeof(BAUx2_d));
    if (!baux2_data) {
        append_output_log(app_data, "[BAUx2] Out of memory for script execution.\n");
        for (int i = 0; i < count; i++) {
            free_stmt(statements[i]);
        }
        kfree(statements);
        cleanup_process(script_proc->pid);
        is_dirty(TRUE);
        return;
    }
    memset(baux2_data, 0, sizeof(BAUx2_d));
    script_proc->data = baux2_data;

    baux2_print_t old_handler = baux2_print_handler;
    int old_pid = g_baux2_curr_pid;
    g_baux2_curr_pid = script_proc->pid;
    g_novella_active_debug_app = app_data;
    baux2_print_handler = nov_baux2_print;

    Env* globals = interpret(statements, count);

    g_baux2_curr_pid = old_pid;
    baux2_print_handler = old_handler;
    g_novella_active_debug_app = NULL;

    for (int i = 0; i < count; i++) {
        free_stmt(statements[i]);
    }
    kfree(statements);

    app_data->debug_env = globals;

    if (!script_proc->window_exists) {
        kfree(script_proc->data);
        script_proc->data = NULL;
        cleanup_process(script_proc->pid);
    }

    is_dirty(TRUE);
}

static void save(Window* win) {
    Process* p = get_process(win->pid);
    NovellaAppData* app_data = (NovellaAppData*)p->data;
    PON_TextField_d* input_data = (PON_TextField_d*)app_data->dialog_input->data;
    
    if (!input_data->buffer || input_data->buffer[0] == '\0') return;

    char clean_name[MAX_FILENAME_LEN];
    sanfl(clean_name, input_data->buffer);

    char path[MAX_FILENAME_LEN + 2] = "/";
    strcat(path, clean_name);
    
    int result = fat_write_file(path, p->buf, p->curr_buf_len);
    if (result != 0) {
        fat_create_file(path);
        fat_write_file(path, p->buf, p->curr_buf_len);
    }
    
    strcpy(app_data->current_filename, clean_name);
    app_data->file_modified = FALSE;
    status_update(app_data);
    
    app_data->dialog->visible = FALSE;
    win->on_key_press = nov_key_press;
    is_dirty(TRUE);
}

static void load(Window* win) {
    Process* p = get_process(win->pid);
    NovellaAppData* app_data = (NovellaAppData*)p->data;
    PON_TextField_d* input_data = (PON_TextField_d*)app_data->dialog_input->data;
    
    if (!input_data->buffer || input_data->buffer[0] == '\0') return;

    char clean_name[MAX_FILENAME_LEN];
    sanfl(clean_name, input_data->buffer);

    char path[MAX_FILENAME_LEN + 2] = "/";
    strcat(path, clean_name);
    
    char* content = fat_read_file(path);
    if (content) {
        strcpy(p->buf, content);
        p->curr_buf_len = strlen(content);
        p->cursor_pos = p->curr_buf_len;
        strcpy(app_data->current_filename, clean_name);
        app_data->file_modified = FALSE;
        status_update(app_data);
        kfree(content);
    }
    
    app_data->dialog->visible = FALSE;
    win->on_key_press = nov_key_press;
    is_dirty(TRUE);
}


static void on_dlg_act(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    Window* win = get_active_win();
    NovellaAppData* app_data = (NovellaAppData*)get_process(win->pid)->data;
    if (app_data->dialog_mode == DIALOG_MODE_SAVE) save(win);
    else if (app_data->dialog_mode == DIALOG_MODE_LOAD) load(win);
}

static void on_dlg_cancel(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    Window* win = get_active_win();
    NovellaAppData* app_data = (NovellaAppData*)get_process(win->pid)->data;
    app_data->dialog->visible = FALSE;
    win->on_key_press = nov_key_press;
    is_dirty(TRUE);
}

#define DL_BG RGB(18, 12, 24)
#define DL_BORDER RGB(58, 46, 72)
#define DL_ACCENT RGB(115, 95, 145)
#define DL_TEXT RGB(238, 235, 245)

static void nov_dlg(PON_Comp* comp, int ax, int ay) {
    PON_Dialog_d* data = (PON_Dialog_d*)comp->data;

    int dx = ax + 16;
    int dy = ay;

    int dw = comp->width - 32;
    int dh = comp->height;

    rect(dx + 4, dy + 4, dw, dh, RGB(0, 0, 0));
    rect(dx, dy, dw, dh, DL_BG);

    rect(dx, dy, dw, 1, DL_BORDER);
    rect(dx, dy + dh - 1, dw, 1, DL_BORDER);
    rect(dx, dy, 1, dh, DL_BORDER);
    rect(dx + dw - 1, dy, 1, dh, DL_BORDER);

    rect(dx + 1, dy + 1, dw - 2, 35, DL_BORDER);
    rect(dx + 1, dy + 1, 4, 35, DL_ACCENT);

    if (data->title) text(data->title, dx + 15, dy + 12, DL_TEXT, FONT_KALNIA, TRUE);
}

static void nov_handle_keydlg(Window* win, unsigned int key) {
    Process* p = get_process(win->pid);
    NovellaAppData* app_data = (NovellaAppData*)p->data;
    
    if (key == '\n') on_dlg_act(NULL, 0, 0);
    else if (key == 27) on_dlg_cancel(NULL, 0, 0);
    else handle_key(app_data->dialog, key);
    
    is_dirty(TRUE);
}

typedef struct {
    const char* word;
    uint32 color;
} KeyCol;

static const KeyCol baux2_keywords[] = {
    { "FUWA", RGB(1, 77, 112) },
    { "MOCO", RGB(145, 0, 121) },
    { "PERO", RGB(47, 4, 64) },
    { "PONDE", RGB(1, 77, 112) },
    { "RING", RGB(145, 0, 121) },

    { "BAU", RGB(28, 10, 74) },
    { "OFFCOLLAB", RGB(35, 32, 38) },

    { "RUFF", RGB(65, 103, 181) },
    { "RUFFIAN", RGB(72, 36, 122) },

    { "FLUFFY", RGB(1, 77, 112) },
    { "FUZZY", RGB(145, 0, 121) },

    { "CHIHUAHUA", RGB(97, 2, 7) },

    { "PON", RGB(51, 3, 87) },
    { "PON.rgb", RGB(51, 3, 87) },
    { "PON.rect", RGB(51, 3, 87) },
    { "PON.button", RGB(51, 3, 87) },
    { "PON.text", RGB(51, 3, 87) },
    { "PON.window", RGB(51, 3, 87) },
    { "PON.field", RGB(51, 3, 87) },
    { NULL, 0 }
};

static uint32 get_keyword_color(const char* word) {
    for (int i = 0; baux2_keywords[i].word != NULL; i++) {
        if (strcmp(word, baux2_keywords[i].word) == 0)
            return baux2_keywords[i].color;
    }
    return 0;
}

static int sum_txtlns_px(const char* buf, int max_line_px, int normal_w, int bold_w) {
    if (!buf || !*buf) return 1;
    int lines = 1;
    int px = 0;
    const char* p = buf;

    while (*p) {
        if (*p == '\n') {
            lines++;
            px = 0;
            p++;
            continue;
        }

        char word_buf[64];
        int i = 0;
        while (i < 63 && p[i] && p[i] != ' ' && p[i] != '\n' && p[i] != '\t'
               && p[i] != '(' && p[i] != ')' && p[i] != ';') {
            word_buf[i] = p[i]; i++;
        }
        word_buf[i] = '\0';

        uint32 kw_color = get_keyword_color(word_buf);

        int advance_px, advance_chars;
        if (i > 0 && kw_color != 0) {
            advance_px = i * bold_w;
            advance_chars = i;
        } else {
            advance_px = normal_w;
            advance_chars = 1;
        }

        if (px + advance_px > max_line_px) {
            lines++;
            px = 0;
        }
        px += advance_px;
        p += advance_chars;
    }
    return lines;
}

static void draw_notebook(PON_Comp* comp, int ax, int ay) {
    Process* p = get_process((int)comp->appdata);
    if (!p) return;

    int content_width = comp->width;
    int content_height = comp->height;
    const FontInfo* font_info = get_font_info(FONT);
    if (!font_info) return;

    int font_width = font_info->width;
    int font_height = font_info->height;
    int bold_width = font_width + 1;

    int max_line_px = content_width - 5 - 16;
    int max_draw_x = ax + 5 + max_line_px;

    int max_visible_lines = (content_height / font_height) + 1;
    int total_lines = sum_txtlns_px(p->buf, max_line_px,
                                          font_width, bold_width);

    int max_scroll_offset = total_lines - max_visible_lines;
    if (max_scroll_offset < 0) max_scroll_offset = 0;
    if (p->scroll_ofs > max_scroll_offset) p->scroll_ofs = max_scroll_offset;
    if (p->scroll_ofs < 0) p->scroll_ofs = 0;

    char* render_ptr = p->buf;
    int current_line = 0;
    while (*render_ptr && current_line < p->scroll_ofs) {
        int px = 0;
        while (*render_ptr && *render_ptr != '\n') {
            char word_buf[64];
            int i = 0;
            while (i < 63 && render_ptr[i] && render_ptr[i] != ' ' && render_ptr[i] != '\n'
                   && render_ptr[i] != '\t' && render_ptr[i] != '(' && render_ptr[i] != ')'
                   && render_ptr[i] != ';') {
                word_buf[i] = render_ptr[i]; i++;
            }
            word_buf[i] = '\0';

            uint32 kw_color = get_keyword_color(word_buf);
            int advance_px, advance_chars;
            if (i > 0 && kw_color != 0) {
                advance_px = i * bold_width;
                advance_chars = i;
            } else {
                advance_px = font_width;
                advance_chars = 1;
            }

            if (px + advance_px > max_line_px) break;
            px += advance_px;
            render_ptr += advance_chars;
        }
        if (*render_ptr == '\n') render_ptr++;
        current_line++;
    }

    int draw_y = ay;
    int buffer_index = render_ptr - p->buf;
    BOOL buffer_ended = FALSE;

    do {
        int draw_x = ax + 5;
        int this_line_start = buffer_index;
        uint32 paper = ((current_line % 2) == 0) ? NOTEBOOK_A : NOTEBOOK_B;
        rect(ax, draw_y, content_width, font_height, paper);

        int cursor_draw_x = -1;
        if (buffer_index == p->cursor_pos)
            cursor_draw_x = draw_x;

        while (*render_ptr && *render_ptr != '\n' && draw_x < max_draw_x) {
            char word_buf[64];
            int i = 0;
            while (i < 63 && render_ptr[i] != ' ' && render_ptr[i] != '\n'
                   && render_ptr[i] != '\t' && render_ptr[i] != '(' && render_ptr[i] != ')'
                   && render_ptr[i] != ';' && render_ptr[i] != '\0') {
                word_buf[i] = render_ptr[i]; i++;
            }
            word_buf[i] = '\0';

            uint32 kw_color = get_keyword_color(word_buf);

            // wrap ts
            if (i > 0 && kw_color != 0) {
                int kw_px = i * bold_width;
                if (draw_x + kw_px > max_draw_x) break;

                for (int k = 0; k <= i; k++) {
                    if (buffer_index + k == p->cursor_pos) {
                        cursor_draw_x = draw_x + k * bold_width;
                        break;
                    }
                }
                text(word_buf, draw_x, draw_y, kw_color, FONT, TRUE);
                draw_x += kw_px + 2;
                render_ptr += i;
                buffer_index += i;
            } else {
                if (buffer_index == p->cursor_pos)
                    cursor_draw_x = draw_x;
                char ch[2] = {*render_ptr, 0};
                text(ch, draw_x, draw_y, INK_COLOR, FONT, FALSE);
                draw_x += font_width;
                render_ptr++;
                buffer_index++;
            }
        }

        if (buffer_index == p->cursor_pos) {
            cursor_draw_x = draw_x;
        }

        if (cursor_draw_x >= 0)
            rect(cursor_draw_x, draw_y, 2, font_height, RGB(145, 55, 0));

        if (*render_ptr == '\n') {
            render_ptr++;
            buffer_index++;
        } else if (*render_ptr == '\0') {
            buffer_ended = TRUE;
        }
        draw_y += font_height;
        current_line++;
    } while (!buffer_ended && current_line < p->scroll_ofs + max_visible_lines);

    while (current_line < p->scroll_ofs + max_visible_lines) {
        uint32 paper = ((current_line % 2) == 0) ? NOTEBOOK_A : NOTEBOOK_B;
        rect(ax, draw_y, content_width, font_height, paper);
        draw_y += font_height;
        current_line++;
    }

    NovellaAppData* app_data = (NovellaAppData*)p->data;
    if (total_lines > max_visible_lines) {
        app_data->bar_x = content_width - 16;
        app_data->bar_y = 4;
        app_data->bar_width = 12;
        app_data->bar_height = content_height;

        int thumb_h = (app_data->bar_height * max_visible_lines) / total_lines;
        if (thumb_h < 20) thumb_h = 20;
        app_data->thumb_height = thumb_h;
        app_data->thumb_width = app_data->bar_width;
        app_data->thumb_x = app_data->bar_x;

        if (max_scroll_offset > 0)
            app_data->thumb_y = app_data->bar_y + (p->scroll_ofs * (app_data->bar_height - app_data->thumb_height)) / max_scroll_offset;
        else
            app_data->thumb_y = app_data->bar_y;

        rect(ax + app_data->bar_x, ay + app_data->bar_y, app_data->bar_width, app_data->bar_height, RGB(40, 40, 45));
        uint32 thumb_col = app_data->is_hovered ? RGB(140, 130, 150) : RGB(100, 90, 110);
        rect(ax + app_data->thumb_x + 2, ay + app_data->thumb_y + 2, app_data->thumb_width - 4, app_data->thumb_height - 4, thumb_col);
    }
}

static void novella_renderer(Window* win) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    NovellaAppData* app_data = (NovellaAppData*)p->data;
    if (!app_data->root) return;

    int content_x = win->x + WIN_BORDER;
    int content_y = win->y + TITLEBAR_H;
    
    PON_render(app_data->root, content_x, content_y);
}

static void on_rq_save(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    Window* win = get_active_win();
    NovellaAppData* app_data = (NovellaAppData*)get_process(win->pid)->data;
    app_data->dialog_mode = DIALOG_MODE_SAVE;
    PON_Dialog_d* dlg_data = (PON_Dialog_d*)app_data->dialog->data;
    if (dlg_data->title) kfree(dlg_data->title);
    dlg_data->title = strdup("SAVE THIS FILE");
    app_data->dialog->visible = TRUE;
    app_data->dialog_input->focused = TRUE;
    win->on_key_press = nov_handle_keydlg;
    is_dirty(TRUE);
}

static void qsave(Window* win) {
    Process* p = get_process(win->pid);
    NovellaAppData* app_data = (NovellaAppData*)p->data;
    
    if (app_data->current_filename[0] == '\0') {
        on_rq_save(NULL, 0, 0);
        return;
    }

    char path[MAX_FILENAME_LEN + 2] = "/";
    strcat(path, app_data->current_filename);
    
    fat_write_file(path, p->buf, p->curr_buf_len);
    app_data->file_modified = FALSE;
    status_update(app_data);
    is_dirty(TRUE);
}

static void on_rq_load(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    Window* win = get_active_win();
    NovellaAppData* app_data = (NovellaAppData*)get_process(win->pid)->data;
    app_data->dialog_mode = DIALOG_MODE_LOAD;
    PON_Dialog_d* dlg_data = (PON_Dialog_d*)app_data->dialog->data;
    if (dlg_data->title) kfree(dlg_data->title);
    dlg_data->title = strdup("OPEN A FILE");
    app_data->dialog->visible = TRUE;
    app_data->dialog_input->focused = TRUE;
    win->on_key_press = nov_handle_keydlg;
    is_dirty(TRUE);
}

static void nov_m_down(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    NovellaAppData* app_data = (NovellaAppData*)p->data;
    
    if (app_data->ide_panel && app_data->ide_panel->visible && app_data->ide_log) {
        int log_thumb_abs_x = app_data->ide_panel->x + app_data->ide_log->x + app_data->log_thumb_x;
        int log_thumb_abs_y = app_data->ide_panel->y + app_data->ide_log->y + app_data->log_thumb_y;
        if (x >= log_thumb_abs_x && x < log_thumb_abs_x + app_data->log_thumb_width &&
            y >= log_thumb_abs_y && y < log_thumb_abs_y + app_data->log_thumb_height) {
            app_data->is_log_dragging = TRUE;
            app_data->log_drag_start_y = y;
            app_data->log_drag_start_offset = app_data->log_scroll_offset;
            is_dirty(TRUE);
            return;
        }
    }

    int nb_abs_x = app_data->notebook ? app_data->notebook->x : 0;
    int nb_abs_y = app_data->notebook ? app_data->notebook->y : 0;
    if (x >= nb_abs_x + app_data->bar_x && x < nb_abs_x + app_data->bar_x + app_data->bar_width &&
        y >= nb_abs_y + app_data->thumb_y && y < nb_abs_y + app_data->thumb_y + app_data->thumb_height) {
        app_data->is_dragging = TRUE;
        app_data->drag_start_y = y;
        app_data->drag_start_y_offset = p->scroll_ofs;
        is_dirty(TRUE);
        return;
    }

    if (handle_mouse(app_data->root, win->x + WIN_BORDER, win->y + TITLEBAR_H, 
                         win->x + WIN_BORDER + x, win->y + TITLEBAR_H + y, PON_MOUSE_DOWN)) {
        is_dirty(TRUE);
    }
}

static void nov_m_up(Window* win, int x, int y) {
    NovellaAppData* app_data = (NovellaAppData*)get_process(win->pid)->data;
    app_data->is_dragging = FALSE;
    app_data->is_log_dragging = FALSE;

    if (handle_mouse(app_data->root, win->x + WIN_BORDER, win->y + TITLEBAR_H, 
                         win->x + WIN_BORDER + x, win->y + TITLEBAR_H + y, PON_MOUSE_UP)) {
        is_dirty(TRUE);
    }
}

static void nov_m_move(Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    NovellaAppData* app_data = (NovellaAppData*)p->data;

    BOOL was_hovered = app_data->is_hovered;
    int nb_abs_x = app_data->notebook ? app_data->notebook->x : 0;
    int nb_abs_y = app_data->notebook ? app_data->notebook->y : 0;
    app_data->is_hovered = (x >= nb_abs_x + app_data->bar_x && x < nb_abs_x + app_data->bar_x + app_data->bar_width &&
                            y >= nb_abs_y + app_data->thumb_y && y < nb_abs_y + app_data->thumb_y + app_data->thumb_height);
    if (was_hovered != app_data->is_hovered) is_dirty(TRUE);

    if (app_data->is_dragging) {
        int dy = y - app_data->drag_start_y;
        int travel = app_data->bar_height - app_data->thumb_height;

        const FontInfo* font_info = get_font_info(FONT);
        if (!font_info) return;
        int font_width = font_info->width;
        int font_height = font_info->height;
        int bold_width = font_width + 1;
        int max_line_px = app_data->notebook->width - 5 - 16;
        int max_visible_lines = app_data->notebook->height / font_height;
        int total_lines = sum_txtlns_px(p->buf, max_line_px, font_width, bold_width);

        int max_scroll = total_lines - max_visible_lines;
        if (max_scroll < 0) max_scroll = 0;

        if (travel > 0) {
            int offset_delta = (dy * max_scroll) / travel;
            p->scroll_ofs = app_data->drag_start_y_offset + offset_delta;
            if (p->scroll_ofs < 0) p->scroll_ofs = 0;
            if (p->scroll_ofs > max_scroll) p->scroll_ofs = max_scroll;
        }
        p->is_scrolled = (p->scroll_ofs < max_scroll);
        is_dirty(TRUE);
    }

    if (app_data->is_log_dragging) {
        int dy = y - app_data->log_drag_start_y;
        int travel = app_data->log_bar_height - app_data->log_thumb_height;

        const FontInfo* font_info = get_font_info(FONT);
        if (!font_info) return;
        int line_height = font_info->height;
        int max_visible_lines = (app_data->ide_log->height - 24) / line_height;
        if (max_visible_lines < 1) max_visible_lines = 1;

        int total_lines = 0;
        const char* tmp = app_data->run_output;
        if (*tmp) total_lines = 1;
        while (*tmp) {
            if (*tmp == '\n') total_lines++;
            tmp++;
        }

        int max_scroll = total_lines - max_visible_lines;
        if (max_scroll < 0) max_scroll = 0;

        if (travel > 0) {
            int offset_delta = (dy * max_scroll) / travel;
            app_data->log_scroll_offset = app_data->log_drag_start_offset + offset_delta;
            if (app_data->log_scroll_offset < 0) app_data->log_scroll_offset = 0;
            if (app_data->log_scroll_offset > max_scroll) app_data->log_scroll_offset = max_scroll;
        }
        app_data->log_user_scrolled = (app_data->log_scroll_offset < max_scroll);
        is_dirty(TRUE);
    }

    if (app_data->ide_panel && app_data->ide_panel->visible && app_data->ide_log) {
        int log_x0 = app_data->ide_panel->x + app_data->ide_log->x;
        int log_y0 = app_data->ide_panel->y + app_data->ide_log->y;
        int log_x1 = log_x0 + app_data->ide_log->width;
        int log_y1 = log_y0 + app_data->ide_log->height;
        BOOL over_log = (x >= log_x0 && x < log_x1 && y >= log_y0 && y < log_y1);
        if (over_log != app_data->is_log_hovered) {
            app_data->is_log_hovered = over_log;
            is_dirty(TRUE);
        }
    } else if (app_data->is_log_hovered) {
        app_data->is_log_hovered = FALSE;
        is_dirty(TRUE);
    }

    if (handle_mouse(app_data->root, win->x + WIN_BORDER, win->y + TITLEBAR_H, 
                         win->x + WIN_BORDER + x, win->y + TITLEBAR_H + y, PON_MOUSE_MOVE)) {
        is_dirty(TRUE);
    }
}

static void nov_key_press(Window* win, unsigned int c) {
    Process* p = get_process(win->pid);
    NovellaAppData* app_data = (NovellaAppData*)p->data;
    char* text_buf = p->buf;
    int* text_len = &p->curr_buf_len;

    if (c == 19) { qsave(win); return; } //save
    if (c == 15) { on_rq_load(NULL, 0, 0); return; } //open
    if (c == 2)  {
        app_data->ide_mode = !app_data->ide_mode;
        update_layout(app_data);
        if (win->title) kfree(win->title);
        const char* new_title = app_data->ide_mode ? "BAUDOL" : "Novella";
        win->title = (char*)kmalloc(strlen(new_title) + 1);
        if (win->title) strcpy(win->title, new_title);
        is_dirty(TRUE);
        return;
    } // ctrl+b
    if (c == 18) { execute_script(win); return; } // ctrl+r

    if (c == '`') { handle_scroll(win, -5); is_dirty(TRUE); return; }
    if (c == '|') { handle_scroll(win, 5); is_dirty(TRUE); return; }

    p->is_scrolled = FALSE;

    const FontInfo* font_info = get_font_info(FONT);
    int max_chars_per_line = app_data->notebook->width / font_info->width;

    if (c == KEY_LEFT && p->cursor_pos > 0) {
        p->cursor_pos--;
    }
    else if (c == KEY_RIGHT && p->cursor_pos < *text_len) {
        p->cursor_pos++;
    }
    else if (c == KEY_UP || c == KEY_DOWN) {
        int* line_starts = app_data->line_starts; 
        int line_count = 0;
        int current_pos = 0;
        int current_line_idx = -1;

        while (current_pos <= *text_len && line_count < MAX_VISIBLE_LINES_CACHE) {
            line_starts[line_count] = current_pos;
            if (p->cursor_pos >= current_pos) current_line_idx = line_count;

            int chars = 0;
            while (current_pos < *text_len && text_buf[current_pos] != '\n' && chars < max_chars_per_line) {
                current_pos++;
                chars++;
            }
            
            if (p->cursor_pos == current_pos && current_line_idx == -1) current_line_idx = line_count;

            line_count++;
            if (current_pos < *text_len && text_buf[current_pos] == '\n') current_pos++;
            else if (current_pos == *text_len) break;
        }

        if (current_line_idx != -1) {
            int col = p->cursor_pos - line_starts[current_line_idx];
            if (c == KEY_UP && current_line_idx > 0) {
                int prev_start = line_starts[current_line_idx - 1];
                int prev_end = line_starts[current_line_idx];
                if (text_buf[prev_end-1] == '\n') prev_end--;
                int prev_len = prev_end - prev_start;
                p->cursor_pos = prev_start + (col < prev_len ? col : prev_len);
            }
            else if (c == KEY_DOWN && current_line_idx < line_count - 1) {
                int next_start = line_starts[current_line_idx + 1];
                int next_end = (current_line_idx + 2 < line_count) ? line_starts[current_line_idx + 2] : *text_len;
                if (next_end > next_start && text_buf[next_end-1] == '\n') next_end--;
                int next_len = next_end - next_start;
                p->cursor_pos = next_start + (col < next_len ? col : next_len);
            }
        }
    }
    else if (c == '\b' && p->cursor_pos > 0) {
        for (int i = p->cursor_pos - 1; i < *text_len; i++) text_buf[i] = text_buf[i + 1];
        (*text_len)--; p->cursor_pos--; text_buf[*text_len] = '\0';
        app_data->file_modified = TRUE;
        status_update(app_data);
    }
    else if ((c == '\n' || (c >= 32 && c < 127)) && *text_len < MAX_SHBUF_SIZE - 1) {
        BOOL should_indent = FALSE;
        if (c == '\n' && p->cursor_pos > 0) {
            char prev = text_buf[p->cursor_pos - 1];
            if (prev == '{') {
                should_indent = TRUE;
            } else {
                //check PONDE or RING
                int line_start = p->cursor_pos - 1;
                while (line_start > 0 && text_buf[line_start-1] != '\n') line_start--;
                
                if (p->cursor_pos - line_start >= 5 && strncmp(&text_buf[line_start], "PONDE", 5) == 0) should_indent = TRUE;
                else if (p->cursor_pos - line_start >= 4 && strncmp(&text_buf[line_start], "RING", 4) == 0) should_indent = TRUE;
            }
        }

        for (int i = *text_len; i >= p->cursor_pos; i--) text_buf[i + 1] = text_buf[i];
        text_buf[p->cursor_pos] = c; (*text_len)++; p->cursor_pos++; text_buf[*text_len] = '\0';

        if (should_indent && *text_len + 2 < MAX_SHBUF_SIZE - 1) {
            for (int j = 0; j < 2; j++) {
                for (int i = *text_len; i >= p->cursor_pos; i--) text_buf[i + 1] = text_buf[i];
                text_buf[p->cursor_pos] = ' '; (*text_len)++; p->cursor_pos++;
            }
            text_buf[*text_len] = '\0';
        }

        app_data->file_modified = TRUE;
        status_update(app_data);
    }
    is_dirty(TRUE);
}

static void draw_ide_table(PON_Comp* comp, int ax, int ay) {
    Process* p = get_process((int)comp->appdata);
    if (!p) return;
    NovellaAppData* app_data = (NovellaAppData*)get_process(p->pid)->data;
    if (!app_data) return;

    rect(ax, ay, comp->width, comp->height, RGB(18, 12, 18));
    const FontInfo* font_info = get_font_info(FONT);
    if (!font_info) return;

    int line_height = font_info->height + 2;
    text("var explorer", ax + 6, ay + 4, COLOR_TEXT_PRIMARY, FONT, TRUE);

    int row_y = ay + 24;
    int max_rows = (comp->height - 28) / line_height;
    int shown = 0;
    Env* env = app_data->debug_env;
    while (env && shown < max_rows) {
        for (int i = 0; i < env->count && shown < max_rows; i++) {
            char value_buf[128] = {0};
            Value *val = &env->entries[i].value;
            if (val->type == VAL_NUMBER) {
                snprintf(value_buf, sizeof(value_buf), "%.3f", val->number);
            } else if (val->type == VAL_BOOL) {
                snprintf(value_buf, sizeof(value_buf), "%s", val->boolean ? "FLUFFY" : "FUZZY");
            } else if (val->type == VAL_STRING) {
                snprintf(value_buf, sizeof(value_buf), "%s", val->string ? val->string : "");
            } else if (val->type == VAL_NIL) {
                snprintf(value_buf, sizeof(value_buf), "nil");
            } else if (val->type == VAL_NATIVE_FN) {
                snprintf(value_buf, sizeof(value_buf), "<ntv_fn>");
            } else if (val->type == VAL_FUNCTION) {
                snprintf(value_buf, sizeof(value_buf), "<fn>");
            } else {
                snprintf(value_buf, sizeof(value_buf), "<???>");
            }

            text(env->entries[i].name, ax + 6, row_y, RGB(100, 192, 235), FONT, FALSE);
            text(value_buf, ax + comp->width / 2 + 6, row_y, RGB(100, 192, 235), FONT, FALSE);
            row_y += line_height;
            shown++;
        }
        env = env->enclosing;
    }

    if (shown == 0) {
        text("no variables active", ax + 6, row_y, COLOR_TEXT_SECONDARY, FONT, FALSE);
    }
}

static void draw_ide_log(PON_Comp* comp, int ax, int ay) {
    Process* p = get_process((int)comp->appdata);
    if (!p) return;
    NovellaAppData* app_data = (NovellaAppData*)get_process(p->pid)->data;
    if (!app_data) return;

    rect(ax, ay, comp->width, comp->height, RGB(16, 20, 30));
    const FontInfo* font_info = get_font_info(FONT);
    if (!font_info) return;

    text("log", ax + 6, ay + 4, COLOR_TEXT_PRIMARY, FONT, TRUE);
    int line_height = font_info->height;
    int row_y = ay + 24;
    int max_visible_lines = (comp->height - 24) / line_height;
    if (max_visible_lines < 1) max_visible_lines = 1;

    int avail_px = comp->width - 16 - 12;
    int max_chars = (font_info->width > 0) ? (avail_px / font_info->width) : 40;
    if (max_chars < 1) max_chars = 1;

    int sum_txtlns = 0;
    const char* tmp = app_data->run_output;
    while (*tmp) {
        int ln = 0;
        const char* s = tmp;
        while (*s && *s != '\n') { ln++; s++; }
        if (ln == 0) sum_txtlns += 1;
        else sum_txtlns += (ln + max_chars - 1) / max_chars;
        if (*s == '\n') tmp = s + 1; else tmp = s;
    }

    int max_scroll = sum_txtlns - max_visible_lines;
    if (max_scroll < 0) max_scroll = 0;
    if (app_data->log_scroll_offset > max_scroll) app_data->log_scroll_offset = max_scroll;
    if (app_data->log_scroll_offset < 0) app_data->log_scroll_offset = 0;

    const char* cursor = app_data->run_output;
    int vis = 0;
    while (*cursor && vis < app_data->log_scroll_offset) {
        int ln = 0;
        const char* s = cursor;
        while (*s && *s != '\n') { ln++; s++; }
        int wraps = (ln == 0) ? 1 : ((ln + max_chars - 1) / max_chars);
        vis += wraps;
        if (*s == '\n') cursor = s + 1; else cursor = s;
    }

    int drawn = 0;
    while (*cursor && drawn < max_visible_lines) {
        char buf[256];
        int i = 0;
        while (*cursor && *cursor != '\n' && i < max_chars && i < (int)sizeof(buf)-1) buf[i++] = *cursor++;
        buf[i] = '\0';
        text(buf, ax + 6, row_y, RGB(245, 120, 205), FONT, FALSE);
        row_y += line_height;
        drawn++;
        if (*cursor && *cursor != '\n') continue;
        if (*cursor == '\n') cursor++;
    }

    if (sum_txtlns > max_visible_lines) {
        app_data->log_bar_x = comp->width - 16;
        app_data->log_bar_y = 24;
        app_data->log_bar_width = 12;
        app_data->log_bar_height = comp->height - 24;

        int thumb_h = (app_data->log_bar_height * max_visible_lines) / sum_txtlns;
        if (thumb_h < 20) thumb_h = 20;
        app_data->log_thumb_height = thumb_h;
        app_data->log_thumb_width = app_data->log_bar_width;
        app_data->log_thumb_x = app_data->log_bar_x;

        if (max_scroll > 0)
            app_data->log_thumb_y = app_data->log_bar_y + (app_data->log_scroll_offset * (app_data->log_bar_height - app_data->log_thumb_height)) / max_scroll;
        else
            app_data->log_thumb_y = app_data->log_bar_y;

        rect(ax + app_data->log_bar_x, ay + app_data->log_bar_y, app_data->log_bar_width, app_data->log_bar_height, RGB(40, 40, 45));
        uint32 thumb_col = app_data->is_log_hovered ? RGB(140, 130, 150) : RGB(100, 90, 110);
        rect(ax + app_data->log_thumb_x + 2, ay + app_data->log_thumb_y + 2, app_data->log_thumb_width - 4, app_data->log_thumb_height - 4, thumb_col);
    }
}

static void handle_scroll(Window* win, int scroll_delta) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    NovellaAppData* app_data = (NovellaAppData*)p->data;

    if (app_data->ide_mode && app_data->is_log_hovered && app_data->ide_log) {
        const FontInfo* font_info = get_font_info(FONT);
        if (!font_info) return;
        int line_height = font_info->height;
        int max_visible_lines = (app_data->ide_log->height - 24) / line_height;
        if (max_visible_lines < 1) max_visible_lines = 1;

        int total_lines = 0;
        const char* tmp = app_data->run_output;
        if (*tmp) total_lines = 1;
        while (*tmp) {
            if (*tmp == '\n') total_lines++;
            tmp++;
        }

        int max_scroll = total_lines - max_visible_lines;
        if (max_scroll < 0) max_scroll = 0;

        app_data->log_scroll_offset += scroll_delta;
        if (app_data->log_scroll_offset > max_scroll) app_data->log_scroll_offset = max_scroll;
        if (app_data->log_scroll_offset < 0) app_data->log_scroll_offset = 0;
        app_data->log_user_scrolled = (app_data->log_scroll_offset < max_scroll);
        is_dirty(TRUE);
        return;
    }

    const FontInfo* font_info = get_font_info(FONT);
    if (!font_info) return;
    int font_width = font_info->width;
    int font_height = font_info->height;
    int bold_width = font_width + 1;
    int max_line_px = app_data->notebook->width - 5 - 16;
    int max_visible_lines = app_data->notebook->height / font_height;
    int total_lines = sum_txtlns_px(p->buf, max_line_px, font_width, bold_width);

    p->scroll_ofs += scroll_delta;
    int max_scroll = total_lines - max_visible_lines;
    if (max_scroll < 0) max_scroll = 0;
    if (p->scroll_ofs > max_scroll) p->scroll_ofs = max_scroll;
    if (p->scroll_ofs < 0) p->scroll_ofs = 0;
    p->is_scrolled = (p->scroll_ofs < max_scroll);
    is_dirty(TRUE);
}

static void cleanup(Window* win) {
    Process* p = get_process(win->pid);
    if (p && p->data) {
        NovellaAppData* app_data = (NovellaAppData*)p->data;
        if (app_data->debug_env) {
            free_env(app_data->debug_env);
            app_data->debug_env = NULL;
        }
        if (app_data->root) PON_free(app_data->root);
        kfree(app_data);
        p->data = NULL;
    }
}

void launch_novella(const char* filename, int flags) {
    Process* p = create_process("Novella");
    if (!p) return;
    NovellaAppData* app_data = (NovellaAppData*)kmalloc(sizeof(NovellaAppData));
    if (!app_data) {
        cleanup_process(p->pid);
        return;
    }
    memset(app_data, 0, sizeof(NovellaAppData));
    p->data = (char*)app_data;
    
    int win_w = 320, win_h = 420;
    app_data->root = PANEL(0, 0, win_w - (2*WIN_BORDER), win_h - TITLEBAR_H - 2 * WIN_BORDER, COLOR_BG_DARK);
    if (!app_data->root) {
        kfree(app_data);
        cleanup_process(p->pid);
        return;
    }
    app_data->pid = p->pid;
    app_data->base_window_width = win_w;
    app_data->log_scroll_offset = 0;
    app_data->log_user_scrolled = FALSE;
    app_data->is_log_dragging = FALSE;
    app_data->is_log_hovered = FALSE;
    app_data->log_bar_x = app_data->log_bar_y = app_data->log_bar_width = app_data->log_bar_height = 0;
    app_data->log_thumb_x = app_data->log_thumb_y = app_data->log_thumb_width = app_data->log_thumb_height = 0;

    app_data->notebook = PON_summon(COMP_GENERIC, 0, 2, app_data->root->width, app_data->root->height - 50);
    if (app_data->notebook) {
        app_data->notebook->draw = draw_notebook;
        app_data->notebook->appdata = (void*)p->pid;
        PON_child(app_data->root, app_data->notebook);
    }

    app_data->ide_panel = PANEL(app_data->root->width - IDE_PANEL_WIDTH, 2, IDE_PANEL_WIDTH, app_data->root->height - 50, COLOR_BG_LIGHT);
    if (app_data->ide_panel) {
        app_data->ide_panel->visible = FALSE;
        PON_child(app_data->root, app_data->ide_panel);

        app_data->ide_table = PON_summon(COMP_GENERIC, 4, 4, IDE_PANEL_WIDTH - 8, (app_data->root->height - 50) / 2 - 8);
        if (app_data->ide_table) {
            app_data->ide_table->draw = draw_ide_table;
            app_data->ide_table->appdata = (void*)p->pid;
            PON_child(app_data->ide_panel, app_data->ide_table);
        }

        app_data->ide_log = PON_summon(COMP_GENERIC, 4, (app_data->root->height - 50) / 2 + 4, IDE_PANEL_WIDTH - 8, (app_data->root->height - 50) / 2 - 8);
        if (app_data->ide_log) {
            app_data->ide_log->draw = draw_ide_log;
            app_data->ide_log->appdata = (void*)p->pid;
            PON_child(app_data->ide_panel, app_data->ide_log);
        }
    }
    
    app_data->status_bar = PANEL(0, app_data->root->height - 34, app_data->root->width, 36, COLOR_BG_DARK);
    if (app_data->status_bar) {
        PON_child(app_data->root, app_data->status_bar);
        
        app_data->status_text = TEXT(12, 10, "> untitled", COLOR_TEXT_SECONDARY);
        if (app_data->status_text) PON_child(app_data->status_bar, app_data->status_text);
        
        app_data->btn_save = BUTTON(app_data->root->width - 110, 6, 50, 24, "SAVE", on_rq_save);
        if (app_data->btn_save) PON_child(app_data->status_bar, app_data->btn_save);
        
        app_data->btn_load = BUTTON(app_data->root->width - 55, 6, 50, 24, "OPEN", on_rq_load); 
        if (app_data->btn_load) PON_child(app_data->status_bar, app_data->btn_load);
    }
    
    app_data->dialog = DIALOG(DIALOG_WIDTH, DIALOG_HEIGHT, "SAVE THIS FILE");
    if (app_data->dialog) {
        app_data->dialog->x = (app_data->root->width - DIALOG_WIDTH) / 2;
        app_data->dialog->y = (app_data->root->height - DIALOG_HEIGHT) / 2;
        app_data->dialog->visible = FALSE;
        app_data->dialog->draw = nov_dlg;
        PON_child(app_data->root, app_data->dialog);
        
        PON_Comp* dlg_lbl = TEXT(26, 46, "filename:", DL_TEXT);
        if (dlg_lbl) PON_child(app_data->dialog, dlg_lbl);
        
        app_data->dialog_input = TEXTFIELD(24, 68, DIALOG_WIDTH - 48, 26, MAX_FILENAME_LEN);
        if (app_data->dialog_input) {
            PON_TextField_d* input_data = (PON_TextField_d*)app_data->dialog_input->data;
            if (input_data) {
                input_data->bg_color = RGB(20, 10, 35);
                input_data->text_color = DL_TEXT;
            }
            PON_child(app_data->dialog, app_data->dialog_input);
        }
        
        PON_Comp* dlg_cancel = BUTTON(24, DIALOG_HEIGHT - 38, 80, 26, "CANCEL", on_dlg_cancel);
        if (dlg_cancel) {
            PON_Button_d* btn_c_data = (PON_Button_d*)dlg_cancel->data;
            if (btn_c_data) {
                btn_c_data->bg_color = DL_BORDER;
                btn_c_data->text_color = DL_TEXT;
            }
            PON_child(app_data->dialog, dlg_cancel);
        }
        
        PON_Comp* dlg_ok = BUTTON(DIALOG_WIDTH - 72, DIALOG_HEIGHT - 38, 48, 26, "OK", on_dlg_act);
        if (dlg_ok) {
            PON_Button_d* btn_ok_data = (PON_Button_d*)dlg_ok->data;
            if (btn_ok_data) {
                btn_ok_data->bg_color = DL_ACCENT;
                btn_ok_data->text_color = DL_TEXT;
            }
            PON_child(app_data->dialog, dlg_ok);
        }
    }

    if (filename) {
        char* content = fat_read_file((char*)filename);
        if (content) {
            strncpy(p->buf, content, MAX_SHBUF_SIZE - 1);
            p->curr_buf_len = strlen(p->buf);
            kfree(content);
            const char* last_slash = strrchr(filename, '/');
            strcpy(app_data->current_filename, last_slash ? last_slash + 1 : filename);
            status_update(app_data);
        }
    }

    update_layout(app_data);

    if (flags & NOVELLA_FLAG_BAUDOL) {
        app_data->ide_mode = TRUE;
        update_layout(app_data);
    }

    uint8 r = 58, g = 46, b = 72;
    const char* win_title = (app_data->ide_mode) ? "BAUDOL" : "Novella";
    Window* w = window_r(p->pid, win_title, -1, -1, win_w, win_h, r, g, b);
    w->content_renderer = novella_renderer;
    w->on_key_press = nov_key_press;
    w->on_mouse_down = nov_m_down;
    w->on_mouse_up = nov_m_up;
    w->on_mouse_move = nov_m_move;
    w->on_scroll = handle_scroll;
    w->on_close = cleanup;

    update_layout(app_data); //refresh so the panel thingy doesnt linger for the re-nov mode
    is_dirty(TRUE);
}
