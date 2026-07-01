#ifndef PON_H
#define PON_H

#include "types.h"
#include "graphics.h"
#include "gui.h"

struct PON_Comp;

typedef void (*draw_cb_t)(struct PON_Comp* comp, int abs_x, int abs_y);
typedef void (*event_cb_t)(struct PON_Comp* comp, int rel_x, int rel_y);

typedef enum {
    COMP_GENERIC,
    COMP_BUTTON,
    COMP_PANEL,
    COMP_TEXT,
    COMP_LISTENTRY,
    COMP_DIALOG,
    COMP_TEXTFIELD
} ComponentType;

typedef struct PON_Comp {
    ComponentType type;
    int x, y;
    int width, height;
    BOOL visible;
    BOOL enabled;
    BOOL hovered;
    BOOL pressed;
    BOOL selected;
    BOOL focused;
    
    void* data; // internals
    void* appdata;
    
    int scroll_x, scroll_y;

    struct PON_Comp* parent;
    struct PON_Comp** children;
    int child_count;
    int child_capacity;

    draw_cb_t draw;
    void (*destroy)(struct PON_Comp* comp);
    event_cb_t on_click;
    event_cb_t on_change;
    event_cb_t on_mouse_down;
    event_cb_t on_mouse_up;
    event_cb_t on_mouse_enter;
    event_cb_t on_mouse_leave;
    void (*on_key_press)(struct PON_Comp* comp, unsigned int key);
} PON_Comp;

typedef struct {
    uint32 color;
    BOOL rounded;
} PON_Panel_d;

typedef struct {
    char* label;
    uint32 bg_color;
    uint32 hover_color;
    BOOL text_bold;
    uint32 text_color;
} PON_Button_d;

typedef struct {
    char* str;
    uint32 color;
    uint32 bg_color;
    BOOL has_bg;
} PON_Text_d;

typedef struct {
    char* title;
} PON_Dialog_d;

typedef struct {
    char* buffer;
    int max_len;
    uint32 bg_color;
    uint32 text_color;
} PON_TextField_d;

typedef struct {
    int* value;
    int min_v, max_v, step;
    PON_Comp* field; //refresh on inc/dec
} Stepper;

typedef enum {
    PON_MOUSE_MOVE,
    PON_MOUSE_DOWN,
    PON_MOUSE_UP
} PON_MouseEvent;

PON_Comp* PON_summon(ComponentType type, int x, int y, int w, int h);
void PON_child(PON_Comp* parent, PON_Comp* child);
void PON_remove_child(PON_Comp* parent, PON_Comp* child);
void PON_render(PON_Comp* comp, int abs_x, int abs_y);
int PON_get_content_height(PON_Comp* comp);
BOOL handle_mouse(PON_Comp* comp, int abs_x, int abs_y, int mouse_x, int mouse_y, PON_MouseEvent event);
BOOL handle_key(PON_Comp* comp, unsigned int key);
void PON_free(PON_Comp* comp);

PON_Comp* BUTTON(int x, int y, int w, int h, const char* label, event_cb_t action);
PON_Comp* PANEL(int x, int y, int w, int h, uint32 color);
PON_Comp* TEXT(int x, int y, const char* str, uint32 color);

void update_str(PON_Comp* comp, const char* str);
PON_Comp* TEXTFIELD(int x, int y, int w, int h, int max_len);

//compounds
PON_Comp* DIALOG(int w, int h, const char* title);
PON_Comp* STEPPER(PON_Comp* parent, int x, int y, int h, int field_w, int* value, int min_v, int max_v, int step, Stepper* binding);

typedef struct {
    uint32 bg;
    uint32 fg;
    uint32 accent;
    uint32 border;
    uint32 text;
} ThemeDf;

extern ThemeDf g_pon_theme;

#endif