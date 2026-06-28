#ifndef GUI_H
#define GUI_H

#include "types.h"

#define MAX_WINDOWS 16
#define TITLEBAR_H 20
#define WIN_BORDER 2

struct Window; 

typedef void (*window_content_renderer_t)(struct Window* window);

typedef struct Window {
    int id;
    int pid;
    int x, y;
    int width, height;
    char* title;
    BOOL active;

    void (*content_renderer)(struct Window* win);
    void (*on_close)(struct Window* win);
    void (*on_click)(struct Window* win, int x, int y);
    void (*on_mouse_down)(struct Window* win, int x, int y);
    void (*on_mouse_up)(struct Window* win, int x, int y);
    void (*on_mouse_move)(struct Window* win, int x, int y);
    void (*on_key_press)(struct Window* win, unsigned int key);
    void (*on_scroll)(struct Window* win, int scroll_delta);
    
    uint32* frame_cache;
    BOOL frame_cache_dirty; //need rebuild?
    int last_x, last_y;
    int last_w, last_h;

    uint8 avg_r;
    uint8 avg_g;
    uint8 avg_b;
} Window;

typedef enum {
    BUTTON_STATE_NORMAL,
    BUTTON_STATE_HOVER,
    BUTTON_STATE_CLICKED
} BUTTON_STATE;

typedef void (*button_action_t)();

typedef struct {
    int x;
    int y;
    int width;
    int height;
    button_action_t action;
    BUTTON_STATE state;
    char* icon_pixel_data;
    int icon_width;
    int icon_height;
    int icon_row_padded;
    char* icon_path;

    uint8 avg_r;
    uint8 avg_g;
    uint8 avg_b;
} icon_button_t;

void icon(int x, int y, int width, int height, const char* icon_path, button_action_t action);
void switchstate(int mouse_x, int mouse_y, BOOL mouse_down);

void g_init(void);
void redraw_all(void);
void close_win(int window_id);
void focus_win(Window* win);
void desktop();
Window* window(int pid, const char* title, int x, int y, int width, int height); // -1 for autocasc
Window* window_r(int pid, const char* title, int x, int y, int width, int height, uint8 r, uint8 g, uint8 b);
void draw_all_win();
void m_win_event_handler(int mouse_x, int mouse_y, BOOL mouse_down);
void m_update(void);
void cl_draw_static_text();
void notif_handler(const char* title, const char* message);
void is_dirty(BOOL dirty);
void is_bg_dirty(BOOL dirty);
void free_icn(icon_button_t* btn);
void handle_scroll_event(int mouse_x, int mouse_y, int scroll_delta);
void gui_cleanup();

BOOL sample_icn(const char* icon_path, uint8* r, uint8* g, uint8* b);

extern BOOL g_screen_dirty;

BOOL is_drag_win(void);
Window* get_active_win();
Window* get_win_by_pid(int pid);

#endif