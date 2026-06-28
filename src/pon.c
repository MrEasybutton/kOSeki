#include "pon.h"
#include "kheap.h"
#include <stdint.h>
#include "string.h"
#include "graphics.h"
#include "vesa.h"

ThemeDf g_pon_theme = { //default stuffs
    .bg = RGB(245, 235, 250),
    .fg = RGB(208, 188, 235),
    .accent = RGB(255, 182, 210),
    .border = RGB(160, 130, 200),
    .text = RGB(68, 40, 90)
};

PON_Comp* PON_summon(ComponentType type, int x, int y, int w, int h) {
    PON_Comp* comp = (PON_Comp*)kmalloc(sizeof(PON_Comp));
    if (!comp) return NULL;
    memset(comp, 0, sizeof(PON_Comp));
    
    comp->type = type;
    comp->x = x;
    comp->y = y;
    comp->width = w;
    comp->height = h;
    comp->visible = TRUE;
    comp->enabled = TRUE;
    comp->scroll_x = 0;
    comp->scroll_y = 0;
    
    return comp;
}

void PON_child(PON_Comp* parent, PON_Comp* child) {
    if (!parent || !child) return;
    
    if (parent->child_count >= parent->child_capacity) {
        int new_cap = parent->child_capacity == 0 ? 4 : parent->child_capacity * 2;
        PON_Comp** new_children = (PON_Comp**)kmalloc(sizeof(PON_Comp*) * new_cap);
        if (!new_children) return;
        
        if (parent->children) {
            memcpy(new_children, parent->children, sizeof(PON_Comp*) * parent->child_count);
            kfree(parent->children);
        }
        parent->children = new_children;
        parent->child_capacity = new_cap;
    }
    
    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

void PON_remove_child(PON_Comp* parent, PON_Comp* child) {
    if (!parent || !child) return;
    
    for (int i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == child) {
            for (int j = i; j < parent->child_count - 1; j++) {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            child->parent = NULL;
            return;
        }
    }
}

void PON_render(PON_Comp* comp, int abs_x, int abs_y) {
    if (!comp || !comp->visible) return;
    
    int draw_x = abs_x + comp->x;
    int draw_y = abs_y + comp->y;
    
    if (comp->draw) {
        comp->draw(comp, draw_x, draw_y);
    }

    set_clip(draw_x, draw_y, comp->width, comp->height);

    for (int i = 0; i < comp->child_count; i++) {
        PON_render(comp->children[i], draw_x - comp->scroll_x, draw_y - comp->scroll_y);
    }

    reset_clip();
}

int PON_get_content_height(PON_Comp* comp) {
    if (!comp) return 0;
    int max_y = 0;
    for (int i = 0; i < comp->child_count; i++) {
        int bottom = comp->children[i]->y + comp->children[i]->height;
        if (bottom > max_y) max_y = bottom;
    }
    return max_y;
}

#define CLAMP_U8(v) ((uint8)((int)(v) > 255 ? 255 : ((int)(v) < 0 ? 0 : (int)(v))))

static void _draw_panel(PON_Comp* comp, int ax, int ay) {
    PON_Panel_d* data = (PON_Panel_d*)comp->data;

    uint32 face = data->color;
    uint8 r = (face >> 16) & 0xFF;
    uint8 g = (face >> 8) & 0xFF;
    uint8 b = face & 0xFF;

    uint32 outline = g_pon_theme.border;
    uint32 bevel_hi = RGB(CLAMP_U8(r + 55), CLAMP_U8(g + 55), CLAMP_U8(b + 55));
    uint32 bevel_lo = RGB(CLAMP_U8(r - 45), CLAMP_U8(g - 45), CLAMP_U8(b - 45));

    int W = comp->width, H = comp->height;

    rect(ax, ay, W, 1, outline);
    rect(ax, ay + H - 1, W, 1, outline);
    rect(ax, ay, 1, H, outline);
    rect(ax + W - 1, ay, 1, H, outline);

    rect(ax + 1, ay + 1, W - 2, H - 2, face);

    for (int t = 1; t <= 3; t++) {
        rect(ax + t, ay + t, W - 2*t, 1, bevel_hi);
        rect(ax + t, ay + t, 1, H - 2*t, bevel_hi);
        rect(ax + t, ay + H - t - 1, W - 2*t, 1, bevel_lo);
        rect(ax + W - t - 1, ay + t, 1, H - 2*t, bevel_lo);
    }
}

static void _destroy_panel(PON_Comp* comp) {
    if (comp->data && (uintptr_t)comp->data > 0x1000) kfree(comp->data);
}

PON_Comp* PANEL(int x, int y, int w, int h, uint32 color) {
    PON_Comp* comp = PON_summon(COMP_PANEL, x, y, w, h);
    if (!comp) return NULL;
    
    PON_Panel_d* data = (PON_Panel_d*)kmalloc(sizeof(PON_Panel_d));
    if (!data) {
        kfree(comp);
        return NULL;
    }
    data->color = color;
    data->rounded = FALSE;
    
    comp->data = data;
    comp->draw = _draw_panel;
    comp->destroy = _destroy_panel;
    
    return comp;
}

static void _draw_button(PON_Comp* comp, int ax, int ay) {
    PON_Button_d* data = (PON_Button_d*)comp->data;
    if (!data) return;

    uint32 base = data->bg_color;
    uint8 r = (base >> 16) & 0xFF;
    uint8 g = (base >> 8) & 0xFF;
    uint8 b = base & 0xFF;

    if (comp->hovered && !comp->pressed && !comp->selected) {
        r = CLAMP_U8(r + 28);
        g = CLAMP_U8(g + 8);
        b = CLAMP_U8(b + 18);
    } else if (comp->pressed || comp->selected) {
        r = CLAMP_U8(r - 22);
        g = CLAMP_U8(g - 22);
        b = CLAMP_U8(b - 12);
    }

    uint32 face = RGB(r, g, b);
    uint32 bevel_hi = RGB(CLAMP_U8(r + 55), CLAMP_U8(g + 55), CLAMP_U8(b + 55));
    uint32 bevel_lo = RGB(CLAMP_U8(r - 50), CLAMP_U8(g - 50), CLAMP_U8(b - 50));
    uint32 outline = g_pon_theme.border;

    int W = comp->width, H = comp->height;

    rect(ax, ay, W, 1, outline);
    rect(ax, ay + H - 1, W, 1, outline);
    rect(ax, ay, 1, H, outline);
    rect(ax + W - 1, ay, 1, H, outline);

    rect(ax + 1, ay + 1, W - 2, H - 2, face);

    for (int t = 1; t <= 2; t++) {
        uint32 top_left = (comp->pressed || comp->selected) ? bevel_lo : bevel_hi;
        uint32 bot_right = (comp->pressed || comp->selected) ? bevel_hi : bevel_lo;

        rect(ax + t, ay + t, W - 2*t, 1, top_left);
        rect(ax + t, ay + t, 1, H - 2*t, top_left);
        rect(ax + t, ay + H - t - 1, W - 2*t, 1, bot_right);
        rect(ax + W - t - 1, ay + t, 1, H - 2*t, bot_right);
    }

    if (data->label) {
        const FontInfo* font = get_font_info(FONT_KALNIA);
        int char_w = (font ? font->width + 1 : 8);
        int text_w = strlen(data->label) * char_w;
        int tx = ax + (comp->width - text_w) / 2;
        int ty = ay + (comp->height - (font ? font->height : 12)) / 2;
        
        if (comp->pressed || comp->selected) { tx += 1; ty += 1; }
        
        text(data->label, tx, ty, data->text_color, FONT_KALNIA, data->text_bold);
    }
}

static void _destroy_button(PON_Comp* comp) {
    if (comp->data && (uintptr_t)comp->data > 0x1000) {
        PON_Button_d* data = (PON_Button_d*)comp->data;
        if (data->label) kfree(data->label);
        kfree(data);
    }
}

PON_Comp* BUTTON(int x, int y, int w, int h, const char* label, event_cb_t action) {
    PON_Comp* comp = PON_summon(COMP_BUTTON, x, y, w, h);
    if (!comp) return NULL;
    
    PON_Button_d* data = (PON_Button_d*)kmalloc(sizeof(PON_Button_d));
    if (!data) {
        kfree(comp);
        return NULL;
    }
    data->label = label ? strdup(label) : NULL;
    data->bg_color = g_pon_theme.fg;
    data->hover_color = g_pon_theme.accent;
    data->text_color = g_pon_theme.text;
    data->text_bold = FALSE;
    
    comp->data = data;
    comp->draw = _draw_button;
    comp->destroy = _destroy_button;
    comp->on_click = action;
    
    return comp;
}

static void _draw_text(PON_Comp* comp, int ax, int ay) {
    PON_Text_d* data = (PON_Text_d*)comp->data;
    if (!data || !data->str) {
        return;
    }

    const char* str = data->str;
    size_t len = strlen(str);

    if (data->has_bg) {
        const FontInfo* font = get_font_info(FONT_KALNIA);
        int char_w = (font ? font->width + 1 : 8);
        int char_h = (font ? font->height : 12);
        rect(ax, ay, len * char_w, char_h, data->bg_color);
    }

    bool bold = FALSE;
    if (len >= 2 && str[0] == '*' && str[len - 1] == '*') {
        bold = TRUE;
        char* tmp = kmalloc(len - 1);
        if (tmp) {
            memcpy(tmp, str + 1, len - 2);
            tmp[len - 2] = '\0';
            text(tmp, ax, ay, data->color, FONT_KALNIA, bold);
            kfree(tmp);
            return;
        }
    }

    text(str, ax, ay, data->color, FONT_KALNIA, bold);
}

static void _destroy_text(PON_Comp* comp) {
    if (comp->data && (uintptr_t)comp->data > 0x1000) {
        PON_Text_d* data = (PON_Text_d*)comp->data;
        if (data->str) kfree(data->str);
        kfree(data);
    }
}

PON_Comp* TEXT(int x, int y, const char* str, uint32 color) {
    PON_Comp* comp = PON_summon(COMP_TEXT, x, y, 0, 0);
    if (!comp) return NULL;
    
    PON_Text_d* data = (PON_Text_d*)kmalloc(sizeof(PON_Text_d));
    if (!data) {
        kfree(comp);
        return NULL;
    }
    data->str = str ? strdup(str) : NULL;
    data->color = color;
    data->bg_color = 0;
    data->has_bg = FALSE;
    
    comp->data = data;
    comp->draw = _draw_text;
    comp->destroy = _destroy_text;
    
    return comp;
}

void update_str(PON_Comp* comp, const char* str) {
    if (!comp || comp->type != COMP_TEXT || !comp->data) return;
    PON_Text_d* data = (PON_Text_d*)comp->data;
    if (data->str) kfree(data->str);
    data->str = str ? strdup(str) : NULL;
}


static void _clear_focus(PON_Comp* comp) {
    if (!comp) return;
    comp->focused = FALSE;
    for (int i = 0; i < comp->child_count; i++) {
        _clear_focus(comp->children[i]);
    }
}

BOOL handle_mouse(PON_Comp* comp, int abs_x, int abs_y, int mouse_x, int mouse_y, PON_MouseEvent event) {
    if (!comp || !comp->visible || !comp->enabled) return FALSE;
    
    BOOL consumed = FALSE;
    int cx = abs_x + comp->x;
    int cy = abs_y + comp->y;
    
    // process the children top-frist
    for (int i = comp->child_count - 1; i >= 0; i--) {
        if (handle_mouse(comp->children[i], cx - comp->scroll_x, cy - comp->scroll_y, mouse_x, mouse_y, event)) {
            consumed = TRUE;
            break;
        }
    }

    BOOL is_inside = (mouse_x >= cx && mouse_x < cx + comp->width &&
                      mouse_y >= cy && mouse_y < cy + comp->height);
    
    BOOL changed = FALSE;
    if (is_inside) {
        if (!comp->hovered) {
            if (comp->on_mouse_enter) comp->on_mouse_enter(comp, mouse_x - cx, mouse_y - cy);
            changed = TRUE;
        }
        comp->hovered = TRUE;
        
        if (event == PON_MOUSE_DOWN && !consumed) {
            if (!comp->pressed) {
                if (comp->on_mouse_down) comp->on_mouse_down(comp, mouse_x - cx, mouse_y - cy);
                
                PON_Comp* root = comp;
                while (root->parent) root = root->parent;
                _clear_focus(root);
                comp->focused = TRUE;
                
                changed = TRUE;
            }
            comp->pressed = TRUE;
            consumed = TRUE;
        } else if (event == PON_MOUSE_UP) {
            if (comp->pressed) {
                if (comp->on_mouse_up) comp->on_mouse_up(comp, mouse_x - cx, mouse_y - cy);
                if (comp->on_click && !consumed) comp->on_click(comp, mouse_x - cx, mouse_y - cy);
                changed = TRUE;
                comp->pressed = FALSE;
            }
        }
    } else {
        if (comp->hovered) {
            if (comp->on_mouse_leave) comp->on_mouse_leave(comp, mouse_x - cx, mouse_y - cy);
            changed = TRUE;
        }
        comp->hovered = FALSE;
        
        if (event == PON_MOUSE_UP && comp->pressed) {
            if (comp->on_mouse_up) comp->on_mouse_up(comp, mouse_x - cx, mouse_y - cy);
            comp->pressed = FALSE;
            changed = TRUE;
        }
    }
    
    return consumed || changed;
}

BOOL handle_key(PON_Comp* comp, unsigned int key) {
    if (!comp || !comp->visible || !comp->enabled) return FALSE;

    BOOL consumed = FALSE;
    if (comp->focused && comp->on_key_press) {
        comp->on_key_press(comp, key);
        consumed = TRUE;
    }

    for (int i = 0; i < comp->child_count; i++) {
        if (handle_key(comp->children[i], key)) {
            consumed = TRUE;
        }
    }
    
    return consumed;
}

static void _draw_dialog(PON_Comp* comp, int ax, int ay) {
    PON_Dialog_d* data = (PON_Dialog_d*)comp->data;
    if (!data) return;
    
    rect(ax + 3, ay + 3, comp->width, comp->height, RGB(8, 8, 10));

    rect(ax, ay, comp->width, comp->height, g_pon_theme.bg);
    rect(ax, ay, comp->width, 1, g_pon_theme.border);
    rect(ax, ay + comp->height - 1, comp->width, 1, g_pon_theme.border);
    rect(ax, ay, 1, comp->height, g_pon_theme.border);
    rect(ax + comp->width - 1, ay, 1, comp->height, g_pon_theme.border);

    rect(ax + 1, ay + 1, comp->width - 2, 35, g_pon_theme.fg);
    rect(ax + 1, ay + 1, 4, 35, g_pon_theme.accent);

    if (data->title) {
        text(data->title, ax + 15, ay + 12, g_pon_theme.text, FONT_KALNIA, FALSE);
    }
}

static void _destroy_dialog(PON_Comp* comp) {
    if (comp->data && (uintptr_t)comp->data > 0x1000) {
        PON_Dialog_d* data = (PON_Dialog_d*)comp->data;
        if (data->title) kfree(data->title);
        kfree(data);
    }
}

PON_Comp* DIALOG(int w, int h, const char* title) {
    PON_Comp* comp = PON_summon(COMP_DIALOG, 0, 0, w, h);
    if (!comp) return NULL;

    PON_Dialog_d* data = (PON_Dialog_d*)kmalloc(sizeof(PON_Dialog_d));
    if (!data) {
        kfree(comp);
        return NULL;
    }
    data->title = title ? strdup(title) : NULL;

    comp->data = data;
    comp->draw = _draw_dialog;
    comp->destroy = _destroy_dialog;

    return comp;
}

static void _draw_text_input(PON_Comp* comp, int ax, int ay) {
    PON_TextField_d* data = (PON_TextField_d*)comp->data;
    if (!data) return;

    rect(ax, ay, comp->width, comp->height, data->bg_color);
    rect(ax, ay, comp->width, 1, g_pon_theme.border);
    rect(ax, ay + comp->height - 1, comp->width, 1, g_pon_theme.border);
    rect(ax, ay, 1, comp->height, g_pon_theme.border);
    rect(ax + comp->width - 1, ay, 1, comp->height, g_pon_theme.border);

    if (data->buffer) {
        text(data->buffer, ax + 6, ay + (comp->height - 20) / 2, data->text_color, FONT_KALNIA, FALSE);

        if (comp->focused) {
            int cursor_x = ax + 4 + (strlen(data->buffer) * 11);
            rect(cursor_x, ay + (comp->height - 15) / 2, 1, 15, g_pon_theme.accent);
        }
    }
}

static void _text_input_key(PON_Comp* comp, unsigned int key) {
    PON_TextField_d* data = (PON_TextField_d*)comp->data;
    if (!data || !data->buffer) return;

    BOOL modified = FALSE;
    int len = strlen(data->buffer);
    if (key == '\b') {
        if (len > 0) {
            data->buffer[len - 1] = '\0';
            modified = TRUE;
        }
    } else if (len < data->max_len - 1 && key >= 32 && key <= 126) {
        data->buffer[len] = (char)key;
        data->buffer[len + 1] = '\0';
        modified = TRUE;
    }

    if (modified && comp->on_change) {
        comp->on_change(comp, 0, 0);
    }
}

static void _text_input_click(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    (void)comp;
}

static void _destroy_textfield(PON_Comp* comp) {
    if (comp->data && (uintptr_t)comp->data > 0x1000) {
        PON_TextField_d* data = (PON_TextField_d*)comp->data;
        if (data->buffer) kfree(data->buffer);
        kfree(data);
    }
}

PON_Comp* TEXTFIELD(int x, int y, int w, int h, int max_len) {
    PON_Comp* comp = PON_summon(COMP_TEXTFIELD, x, y, w, h);
    if (!comp) return NULL;

    PON_TextField_d* data = (PON_TextField_d*)kmalloc(sizeof(PON_TextField_d));
    if (!data) {
        kfree(comp);
        return NULL;
    }
    data->buffer = (char*)kmalloc(max_len);
    if (!data->buffer) {
        kfree(data);
        kfree(comp);
        return NULL;
    }
    memset(data->buffer, 0, max_len);
    data->max_len = max_len;
    data->bg_color = RGB(200, 177, 204);
    data->text_color = g_pon_theme.text;

    comp->data = data;
    comp->draw = _draw_text_input;
    comp->destroy = _destroy_textfield;
    comp->on_key_press = _text_input_key;
    comp->on_click = _text_input_click;

    return comp;
}

void PON_free(PON_Comp* comp) {
    if (!comp) return;

    for (int i = 0; i < comp->child_count; i++) {
        PON_free(comp->children[i]);
    }

    if (comp->children) kfree(comp->children);
    if (comp->destroy) comp->destroy(comp);

    kfree(comp);
}