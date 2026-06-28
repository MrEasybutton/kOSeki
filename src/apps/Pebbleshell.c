#include "apps/Pebbleshell.h"
#include "procsys.h"
#include "gui.h"
#include "string.h"
#include "graphics.h"
#include "kheap.h"
#include "vesa.h"
#include "fonts.h"
#include "serial.h"

#define WIN_BORDER 2
#define TITLEBAR_H 20
#define PROMPT_TEXT "$BIBOO > "
#define FONT FONT_KALNIA

typedef struct {
    int y_ofs;
    int max_y_ofs;
    int content_height;
    int view_height;
    
    BOOL is_dragging;
    int drag_start_y;
    int drag_start_y_ofs;
    BOOL is_hovered;

    int bar_x, bar_y, bar_width, bar_height;
    int thumb_x, thumb_y, thumb_width, thumb_height;
} ScrollView;

int count_terminal_lines(const char* buffer, int max_chars_per_line) {
    if (!buffer || max_chars_per_line <= 0) return 0;
    
    int line_count = 0;
    const char* ptr = buffer;
    
    while (*ptr != '\0') {
        int chars_in_line = 0;
        
        while (*ptr != '\0' && *ptr != '\n' && chars_in_line < max_chars_per_line) {
            ptr++;
            chars_in_line++;
        }
        
        line_count++;
        
        if (*ptr == '\n') {
            ptr++;
        }
        
        if (chars_in_line >= max_chars_per_line && *ptr != '\0' && *ptr != '\n') {
            
        }
    }
    
    return line_count;
}

void pebbleshell(Window* win) {
    if (!win) return;

    Process* p = get_process(win->pid);
    if (!p) return;

    ScrollView* sv = (ScrollView*)p->data;
    if (!sv) return;

    int content_x = win->x + WIN_BORDER;
    int content_y = win->y + TITLEBAR_H + WIN_BORDER;
    int content_width = win->width - (2 * WIN_BORDER);
    int content_height = win->height - TITLEBAR_H - (2 * WIN_BORDER);

    rect_grad(content_x, content_y, content_width, content_height, RGB(0, 0, 0), RGB(0, 0, 0));

    const FontInfo* font_info = get_font_info(FONT);
    if (!font_info) return;

    int font_width = font_info->width;
    int font_height = font_info->height;

    if (font_width <= 0 || font_height <= 0) return;

    int max_chars_per_line = content_width / font_width;
    if (max_chars_per_line <= 0) max_chars_per_line = 1;
    int max_visible_lines = (content_height - 10) / font_height;
    if (max_visible_lines <= 0) max_visible_lines = 1;

    const char* TERM_PROMPT = "$BIBOO > ";
    int prompt_len = 9; //len of above
    
    int total_lines = count_terminal_lines(p->buf, max_chars_per_line);
    
    sv->view_height = max_visible_lines * font_height;
    sv->content_height = total_lines * font_height;
    sv->max_y_ofs = (sv->content_height > sv->view_height) ? (sv->content_height - sv->view_height) : 0;
    
    int max_scroll_ofs = total_lines - max_visible_lines;
    if (max_scroll_ofs < 0) max_scroll_ofs = 0;
    
    sv->y_ofs = p->scroll_ofs * font_height;
    if (sv->y_ofs > sv->max_y_ofs) {
        sv->y_ofs = sv->max_y_ofs;
        p->scroll_ofs = sv->y_ofs / font_height;
    }
    
    if (p->scroll_ofs > max_scroll_ofs) {
        p->scroll_ofs = max_scroll_ofs;
    }
    if (p->scroll_ofs < 0) {
        p->scroll_ofs = 0;
    }

    if (!p->is_scrolled || p->scroll_ofs >= max_scroll_ofs) {
        p->scroll_ofs = max_scroll_ofs;
        sv->y_ofs = p->scroll_ofs * font_height;
    }
    
    char* render_ptr = p->buf;
    int current_line = 0;
    int draw_y = content_y + 5;

    while (*render_ptr != '\0' && current_line < p->scroll_ofs) {
        int chars_in_line = 0;
        
        while (*render_ptr != '\0' && *render_ptr != '\n' && chars_in_line < max_chars_per_line) {
            render_ptr++;
            chars_in_line++;
        }
        
        if (*render_ptr == '\n') {
            render_ptr++;
        }
        
        current_line++;
    }

    int lines_rendered = 0;
    while (*render_ptr != '\0' && 
           lines_rendered < max_visible_lines && 
           (draw_y + font_height) <= (content_y + content_height - 5)) {
        
        int draw_x = content_x + 5;
        
        BOOL at_prompt = TRUE;
        for (int i = 0; i < prompt_len; i++) {
            if (render_ptr[i] != TERM_PROMPT[i]) {
                at_prompt = FALSE;
                break;
            }
        }
        
        if (at_prompt) {
            for (int i = 0; i < prompt_len; i++) {
                char ch[2] = {TERM_PROMPT[i], '\0'};
                text(ch, draw_x, draw_y, RGB(235, 175, 255), FONT, TRUE);
                draw_x += font_width;
            }
            render_ptr += prompt_len;
        }

        int chars_drawn = at_prompt ? prompt_len : 0;
        char line_buf[256];
        int lbuf_len = 0;
        while (*render_ptr != '\0' && *render_ptr != '\n' && chars_drawn < max_chars_per_line && lbuf_len < 255) {
            line_buf[lbuf_len++] = *render_ptr++;
            chars_drawn++;
        }
        line_buf[lbuf_len] = '\0';
        if (lbuf_len > 0) {
            text_clip(line_buf, draw_x, draw_y, RGB(255, 255, 255), FONT);
        }
        
        draw_y += font_height;
        lines_rendered++;
        
        if (*render_ptr == '\n') {
            render_ptr++;
        }
    }
    
    if (total_lines > max_visible_lines) {
        sv->bar_x = content_width - 14;
        sv->bar_y = 5;
        sv->bar_width = 12;
        sv->bar_height = sv->view_height;

        if (sv->content_height > 0) {
            sv->thumb_height = (sv->bar_height * sv->view_height) / sv->content_height;
            if (sv->thumb_height < 20) sv->thumb_height = 20;
            if (sv->thumb_height > sv->bar_height) sv->thumb_height = sv->bar_height;
        } else {
            sv->thumb_height = sv->bar_height;
        }
        
        if (sv->max_y_ofs > 0) {
            sv->thumb_y = sv->bar_y + (sv->y_ofs * (sv->bar_height - sv->thumb_height)) / sv->max_y_ofs;
        } else {
            sv->thumb_y = sv->bar_y;
        }
        sv->thumb_x = sv->bar_x + 2;
        sv->thumb_width = sv->bar_width + 4;

        rect_grad(content_x + sv->bar_x, content_y + sv->bar_y, sv->bar_width, sv->bar_height, 
                  RGB(40, 40, 40), RGB(40, 40, 40));
        
        uint32 thumb_color = sv->is_hovered ? RGB(200, 200, 200) : RGB(180, 180, 180);
        rect_grad(content_x + sv->bar_x + 2, content_y + sv->thumb_y + 2, sv->bar_width - 4, sv->thumb_height - 4,
                  thumb_color, RGB(120, 120, 120));
    }
}

void pbsh_handle_scroll(Window* win, int scroll_delta) {
    kprint("pbsh_handle_scroll: Called with scroll_delta = %d\n", scroll_delta);
    if (!win) return;
    
    Process* p = get_process(win->pid);
    if (!p) return;
    
    const FontInfo* font_info = get_font_info(FONT);
    if (!font_info || font_info->width <= 0 || font_info->height <= 0) return;

    int content_width = win->width - (2 * WIN_BORDER);
    int content_height = win->height - TITLEBAR_H - (2 * WIN_BORDER);
    int max_chars_per_line = content_width / font_info->width;
    if (max_chars_per_line <= 0) max_chars_per_line = 1;
    int max_visible_lines = (content_height - 10) / font_info->height;
    if (max_visible_lines <= 0) max_visible_lines = 1;

    int total_lines = count_terminal_lines(p->buf, max_chars_per_line);
    
    p->scroll_ofs += scroll_delta;

    int max_scroll_ofs = total_lines - max_visible_lines;
    if (max_scroll_ofs < 0) max_scroll_ofs = 0;

    if (p->scroll_ofs > max_scroll_ofs) {
        p->scroll_ofs = max_scroll_ofs;
    }
    if (p->scroll_ofs < 0) {
        p->scroll_ofs = 0;
    }

    if (p->scroll_ofs < max_scroll_ofs) {
        p->is_scrolled = TRUE;
    } else {
        //clear at bottom
        p->is_scrolled = FALSE;
    }
}

void pbsh_m_down(Window* win, int x, int y) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    ScrollView* sv = (ScrollView*)p->data;

    if (x >= sv->thumb_x && x < sv->thumb_x + sv->thumb_width &&
        y >= sv->thumb_y && y < sv->thumb_y + sv->thumb_height) {
        
        sv->is_dragging = TRUE;
        sv->drag_start_y = y;
        sv->drag_start_y_ofs = p->scroll_ofs;
    }
}

void pbsh_m_up(Window* win, int x __attribute__((unused)), int y __attribute__((unused))) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    ScrollView* sv = (ScrollView*)p->data;
    sv->is_dragging = FALSE;
}

void pbsh_m_move(Window* win, int x, int y) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    ScrollView* sv = (ScrollView*)p->data;

    BOOL was_hovered = sv->is_hovered;
    sv->is_hovered = (x >= sv->thumb_x && x < sv->thumb_x + sv->thumb_width &&
                      y >= sv->thumb_y && y < sv->thumb_y + sv->thumb_height);
    
    if (was_hovered != sv->is_hovered) {
        is_dirty(TRUE);
    }

    if (sv->is_dragging) {
        int content_width = win->width - (2 * WIN_BORDER);
        int content_height = win->height - TITLEBAR_H - (2 * WIN_BORDER);
        const FontInfo* font_info = get_font_info(FONT);
        if (!font_info || font_info->width <= 0 || font_info->height <= 0) return;

        int max_chars_per_line = content_width / font_info->width;
        if (max_chars_per_line <= 0) max_chars_per_line = 1;
        int max_visible_lines = (content_height - 10) / font_info->height;
        if (max_visible_lines <= 0) max_visible_lines = 1;
        int total_lines = count_terminal_lines(p->buf, max_chars_per_line);
        
        int dy = y - sv->drag_start_y;
        int thumb_travel = sv->bar_height - sv->thumb_height;
        int max_scroll_ofs = total_lines - max_visible_lines;
        if (max_scroll_ofs < 0) max_scroll_ofs = 0;
        
        int ofs_delta = (thumb_travel > 0) ? (dy * max_scroll_ofs) / thumb_travel : 0;
        int new_scroll_ofs = sv->drag_start_y_ofs + ofs_delta;

        if (new_scroll_ofs < 0) new_scroll_ofs = 0;
        if (new_scroll_ofs > max_scroll_ofs) new_scroll_ofs = max_scroll_ofs;
        
        p->scroll_ofs = new_scroll_ofs;
        sv->y_ofs = p->scroll_ofs * font_info->height;
        
        p->is_scrolled = TRUE;
        
        is_dirty(TRUE);
    }
}

void pbsh_cleanup(Window* win) {
    if (!win) return;
    Process* p = get_process(win->pid);
    if (p && p->data) {
        kfree(p->data);
        p->data = NULL;
    }
}

void launch_pbsh() {
    Process* p = create_process("pbsh");
    if (!p) return;

    ScrollView* sv = (ScrollView*)kmalloc(sizeof(ScrollView));
    if (!sv) {
        cleanup_process(p->pid);
        return;
    }
    memset(sv, 0, sizeof(ScrollView));
    p->data = sv;

    strcpy(p->buf, "PebbleShell v1.0\n"
                    "Use with caution! :D\n"
                    "$BIBOO > ");
    p->curr_buf_len = strlen(p->buf);
    p->scroll_ofs = 0;
    p->is_scrolled = FALSE;

    Window* w = window(p->pid, "kOSeki Pebbleshell", -1, -1, 330, 240);
    if (!w) {
        kfree(sv);
        cleanup_process(p->pid);
        return;
    }

    w->content_renderer = pebbleshell;
    w->on_close = pbsh_cleanup;
    w->on_mouse_down = pbsh_m_down;
    w->on_mouse_up = pbsh_m_up;
    w->on_mouse_move = pbsh_m_move;
}