#include "procsys.h"
#include "gui.h"
#include "pon.h"
#include "vesa.h"
#include "graphics.h"
#include "kheap.h"
#include "string.h"
#include "ac97.h"
#include "serial.h"
#include "CLStudio.h"

//this palette is largely gpt'd
#define CLP_COLOR_SHELL         RGB(214,   6,  96)
#define CLP_COLOR_SHELL_HI      RGB(255,  80, 155)
#define CLP_COLOR_SHELL_SHADOW  RGB(130,   2,  56)
#define CLP_COLOR_PANEL         RGB(248, 240, 218)
#define CLP_COLOR_PANEL_HI      RGB(255, 252, 240)
#define CLP_COLOR_PANEL_SHADOW  RGB(190, 175, 140)
#define CLP_COLOR_TAPE_BG       RGB( 18,   8,  18)
#define CLP_COLOR_TAPE_STRIPE   RGB( 32,  12,  28)
#define CLP_COLOR_REEL_HUB      RGB(214,   6,  96)
#define CLP_COLOR_REEL_RING     RGB(255,  80, 155)
#define CLP_COLOR_REEL_SHADOW   RGB(130,   2,  56)
#define CLP_COLOR_REEL_BODY     RGB( 38,  14,  32)
#define CLP_COLOR_TAPE_LABEL    RGB(255, 215, 235)
#define CLP_COLOR_TRACK_BG      RGB( 18,   8,  18)
#define CLP_COLOR_TRACK_FILL    RGB(214,   6,  96)
#define CLP_COLOR_TRACK_FILL_HI RGB(255,  80, 155)
#define CLP_COLOR_TRACK_HEAD    RGB(255, 240, 220)
#define CLP_COLOR_BTN_FACE      RGB(248, 240, 218)
#define CLP_COLOR_BTN_PRESSED   RGB(220, 208, 180)
#define CLP_COLOR_BTN_HOVER     RGB(255, 248, 228)
#define CLP_COLOR_BTN_ACTIVE    RGB(255, 210, 232)
#define CLP_COLOR_TEXT_DARK     RGB( 80,  20,  48)
#define CLP_COLOR_TEXT_LABEL    RGB(214,   6,  96)
#define CLP_COLOR_TEXT_DIM      RGB(160, 120, 140)
#define CLP_COLOR_SCREW         RGB(190, 175, 140)

#define WIN_BORDER 2
#define TITLEBAR_H 20
#define CLP_WIN_W 280
#define CLP_WIN_H 175

#define CLP_TAPE_X 8
#define CLP_TAPE_Y 6
#define CLP_TAPE_W 235
#define CLP_TAPE_H 62

#define CLP_REEL_RADIUS 18
#define CLP_REEL_LEFT_OX 36
#define CLP_REEL_RIGHT_OX 196
#define CLP_REEL_OY 32

#define CLP_TRACK_X 8
#define CLP_TRACK_Y 76
#define CLP_TRACK_W 235
#define CLP_TRACK_H 10

#define CLP_BTN_Y 102
#define CLP_BTN_H 28
#define CLP_BTN_W 70

#define CLP_FNAME_X 90
#define CLP_FNAME_Y 28

typedef enum {
    CLP_BTN_STATE_NORMAL = 0,
    CLP_BTN_STATE_HOVER = 1,
    CLP_BTN_STATE_PRESSED = 2,
    CLP_BTN_STATE_ACTIVE = 3,
} CLPBtnState;

typedef struct {
    int voice_handle;
    char* current_file;
    BOOL is_playing;
    int reel_frame;
    PON_Comp* root;
} clp_state_t;

static inline void hline(int x, int y, int w, uint32 c) { rect(x, y, w, 1, c); }
static inline void vline(int x, int y, int h, uint32 c) { rect(x, y, 1, h, c); }

static void bevel_raised(int x, int y, int w, int h, uint32 hi, uint32 sha) {
    hline(x, y, w, hi);
    hline(x, y + 1, w - 1, hi);
    vline(x, y, h, hi);
    vline(x + 1, y + 1, h - 2, hi);
    
    hline(x + 1, y + h - 2, w - 1, sha);
    hline(x, y + h - 1, w, sha);
    vline(x + w - 2, y + 1, h - 1, sha);
    vline(x + w - 1, y, h, sha);
}
static void bevel_inset(int x, int y, int w, int h, uint32 hi, uint32 sha) {
    bevel_raised(x, y, w, h, sha, hi);
}

static void draw_shell(int cx, int cy, int cw, int ch) {
    rect(cx, cy, cw, ch, CLP_COLOR_SHELL);
    bevel_raised(cx, cy, cw, ch, CLP_COLOR_SHELL_HI, CLP_COLOR_SHELL_SHADOW);
}

static const int spoke_dirs[3][6] = {
    { 100, 0, -50, 87, -50, -87 },
    { 77, 64, -94, 34, 17, -98 },
    { 17, 98, -94, -34, 77, -64 },
};

static void draw_reel(int cx, int cy, int r, int frame) {
    int inner = r - 4;
    if (inner < 2) inner = 2;

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            int d2 = dx * dx + dy * dy;
            if (d2 > r * r) continue;
            if (d2 > inner * inner) {
                uint32 c = (dy < 0 || (dy == 0 && dx < 0))
                           ? CLP_COLOR_REEL_RING : CLP_COLOR_REEL_SHADOW;
                pixel(cx + dx, cy + dy, c);
            } else {
                pixel(cx + dx, cy + dy, CLP_COLOR_REEL_BODY);
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        int dxi = spoke_dirs[frame][i * 2];
        int dyi = spoke_dirs[frame][i * 2 + 1];
        for (int s = 2; s <= inner - 1; s++) {
            pixel(cx + (s * dxi) / 100,
                  cy + (s * dyi) / 100,
                  CLP_COLOR_REEL_HUB);
        }
    }

    rect(cx - 2, cy - 2, 5, 5, CLP_COLOR_REEL_HUB);
    pixel(cx - 2, cy - 2, CLP_COLOR_REEL_RING);
    pixel(cx + 2, cy + 2, CLP_COLOR_REEL_SHADOW);
}

static void draw_casswin(int ax, int ay,
                                  int prog_num, int prog_den, int frame) {
    rect(ax + CLP_TAPE_X, ay + CLP_TAPE_Y, CLP_TAPE_W, CLP_TAPE_H, CLP_COLOR_TAPE_BG);
    bevel_inset(ax + CLP_TAPE_X, ay + CLP_TAPE_Y, CLP_TAPE_W, CLP_TAPE_H,
                CLP_COLOR_SHELL_HI, CLP_COLOR_SHELL_SHADOW);

    for (int sy = 2; sy < CLP_TAPE_H - 2; sy += 4) {
        hline(ax + CLP_TAPE_X + 2, ay + CLP_TAPE_Y + sy, CLP_TAPE_W - 4, CLP_COLOR_TAPE_STRIPE);
    }

    int lx = ax + CLP_TAPE_X + CLP_TAPE_W / 2 - 38;
    int ly = ay + CLP_TAPE_Y + CLP_TAPE_H / 2 - 7;
    rect(lx, ly, 76, 14, CLP_COLOR_SHELL);
    bevel_raised(lx, ly, 76, 14, CLP_COLOR_SHELL_HI, CLP_COLOR_SHELL_SHADOW);

    int left_r = CLP_REEL_RADIUS;
    int right_r = CLP_REEL_RADIUS;
    if (prog_den > 0) {
        int extra = (int)(((uint64)prog_num * 8) / prog_den);
        left_r = CLP_REEL_RADIUS - 4 + extra;
        right_r = CLP_REEL_RADIUS + 4 - extra;
        if (left_r < 8) left_r = 8;
        if (right_r < 8) right_r = 8;
        if (left_r > CLP_REEL_RADIUS + 6) left_r = CLP_REEL_RADIUS + 6;
        if (right_r > CLP_REEL_RADIUS + 6) right_r = CLP_REEL_RADIUS + 6;
    }

    int lcx = ax + CLP_TAPE_X + CLP_REEL_LEFT_OX;
    int rcx = ax + CLP_TAPE_X + CLP_REEL_RIGHT_OX;
    int rcy = ay + CLP_TAPE_Y + CLP_REEL_OY;

    draw_reel(lcx, rcy, left_r, frame);
    draw_reel(rcx, rcy, right_r, (frame + 1) % 3);

    int tx0 = lcx + left_r;
    int tx1 = rcx - right_r;
    if (tx1 > tx0) {
        hline(tx0, rcy, tx1 - tx0, CLP_COLOR_SHELL_SHADOW);
        hline(tx0, rcy - 1, tx1 - tx0, CLP_COLOR_TAPE_STRIPE);
    }
}

static void draw_progress_track(int ax, int ay, int prog_num, int prog_den) {
    rect(ax + CLP_TRACK_X, ay + CLP_TRACK_Y, CLP_TRACK_W, CLP_TRACK_H, CLP_COLOR_TRACK_BG);
    bevel_inset(ax + CLP_TRACK_X, ay + CLP_TRACK_Y, CLP_TRACK_W, CLP_TRACK_H,
                CLP_COLOR_PANEL_HI, CLP_COLOR_PANEL_SHADOW);

    if (prog_den > 0 && prog_num > 0) {
        int fill_w = (int)(((uint64)prog_num * (uint64)(CLP_TRACK_W - 4)) / prog_den);
        if (fill_w > CLP_TRACK_W - 4) fill_w = CLP_TRACK_W - 4;
        if (fill_w > 0) {
            int iy = ay + CLP_TRACK_Y + 2, ih = CLP_TRACK_H - 4, half = ih / 2;
            rect(ax + CLP_TRACK_X + 2, iy, fill_w, half, CLP_COLOR_TRACK_FILL_HI);
            rect(ax + CLP_TRACK_X + 2, iy + half, fill_w, ih - half, CLP_COLOR_TRACK_FILL);
            vline(ax + CLP_TRACK_X + 2 + fill_w, iy, ih, CLP_COLOR_TRACK_HEAD);
        }
    }
}

static void draw_clp_shell(PON_Comp* comp, int ax, int ay) {
    draw_shell(ax, ay, comp->width, comp->height);
}

static void draw_clp_panel(PON_Comp* comp, int ax, int ay) {
    rect(ax, ay, comp->width, comp->height, CLP_COLOR_PANEL);
    bevel_raised(ax, ay, comp->width, comp->height, CLP_COLOR_PANEL_HI, CLP_COLOR_PANEL_SHADOW);
}

static void draw_cass(PON_Comp* comp, int ax, int ay) {
    clp_state_t* state = (clp_state_t*)comp->appdata;
    if (!state) return;

    uint32 prog_cur = 0, prog_tot = 0;
    if (state->is_playing) {
        if (!ac97_get_voice_progress(state->voice_handle, &prog_cur, &prog_tot)) {
            state->is_playing = FALSE;
            state->voice_handle = -1;
        }
    }

    draw_casswin(ax, ay, (int)prog_cur, (int)prog_tot, state->reel_frame);
    draw_progress_track(ax, ay, (int)prog_cur, (int)prog_tot);
    
    const char* fname = state->current_file ? state->current_file : "...";
    const char* slash = strrchr(fname, '/');
    if (slash) fname = slash + 1;
    char display_name[32];
    bool truncated = strlen(fname) > 6;

    if (truncated) {
        memcpy(display_name, fname, 4);
        memcpy(display_name + 4, "..", 3);
        fname = display_name;
    }

    text(fname,
        ax + CLP_FNAME_X,
        ay + CLP_FNAME_Y,
        RGBA(245, 225, 240, 255),
        FONT_KALNIA,
        TRUE);
}

static void draw_clp_button(PON_Comp* comp, int ax, int ay) {
    PON_Button_d* data = (PON_Button_d*)comp->data;
    clp_state_t* state = (clp_state_t*)comp->appdata;
    
    CLPBtnState btn_state = CLP_BTN_STATE_NORMAL;
    if (comp->pressed) btn_state = CLP_BTN_STATE_PRESSED;
    else if (comp->hovered) btn_state = CLP_BTN_STATE_HOVER;
    else if (comp->selected) btn_state = CLP_BTN_STATE_ACTIVE;

    uint32 face, hi, sha;
    switch (btn_state) {
        case CLP_BTN_STATE_HOVER:
            face = CLP_COLOR_BTN_HOVER;
            hi = CLP_COLOR_PANEL_HI; sha = CLP_COLOR_PANEL_SHADOW; break;
        case CLP_BTN_STATE_PRESSED:
            face = CLP_COLOR_BTN_PRESSED;
            hi = CLP_COLOR_PANEL_SHADOW; sha = CLP_COLOR_PANEL_HI; break;
        case CLP_BTN_STATE_ACTIVE:
            face = CLP_COLOR_BTN_ACTIVE;
            hi = CLP_COLOR_SHELL_HI; sha = CLP_COLOR_SHELL_SHADOW; break;
        default:
            face = CLP_COLOR_BTN_FACE;
            hi = CLP_COLOR_PANEL_HI; sha = CLP_COLOR_PANEL_SHADOW; break;
    }

    rect(ax, ay, comp->width, comp->height, face);

    if (btn_state == CLP_BTN_STATE_PRESSED)
        bevel_inset(ax, ay, comp->width, comp->height, sha, hi);
    else
        bevel_raised(ax, ay, comp->width, comp->height, hi, sha);

    int accent_y = (btn_state == CLP_BTN_STATE_PRESSED) ? ay + 3 : ay + 2;
    hline(ax + 3, accent_y, comp->width - 6, CLP_COLOR_SHELL);

    if (data->label) {
        int off = (btn_state == CLP_BTN_STATE_PRESSED) ? 1 : 0;
        uint32 tc = (btn_state == CLP_BTN_STATE_ACTIVE) ? CLP_COLOR_TEXT_LABEL : CLP_COLOR_TEXT_DARK;
        int llen = strlen(data->label);
        text(data->label, ax + (comp->width - llen * 7) / 2 + off, ay + comp->height / 2 - 5 + off, tc, FONT_KALNIA, TRUE);
    }
}

static void clp_renderer(struct Window* win) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    clp_state_t* state = (clp_state_t*)p->data;
    if (!state->root) return;

    if (state->is_playing)
        state->reel_frame = (state->reel_frame + 1) % 3;

    int content_x = win->x + WIN_BORDER;
    int content_y = win->y + TITLEBAR_H;
    
    PON_render(state->root, content_x, content_y);
}

static void on_play_click(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    clp_state_t* state = (clp_state_t*)comp->appdata;
    if (!state) return;
    if (!state->is_playing) {
        state->voice_handle = ac97_play(state->current_file);
        if (state->voice_handle != -1) state->is_playing = TRUE;
    }
    is_dirty(TRUE);
}

static void on_stop_click(PON_Comp* comp, int rx, int ry) {
    (void)rx; (void)ry;
    clp_state_t* state = (clp_state_t*)comp->appdata;
    if (!state) return;
    if (state->is_playing) {
        ac97_stop_voice(state->voice_handle);
        state->is_playing = FALSE;
        state->voice_handle = -1;
        state->reel_frame = 0;
    }
    is_dirty(TRUE);
}

static void clp_m_down(struct Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    clp_state_t* state = (clp_state_t*)p->data;
    if (!state->root) return;

    if (handle_mouse(state->root, win->x + WIN_BORDER, win->y + TITLEBAR_H, 
                         win->x + WIN_BORDER + x, win->y + TITLEBAR_H + y, PON_MOUSE_DOWN)) {
        is_dirty(TRUE);
    }
}

static void clp_m_up(struct Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    clp_state_t* state = (clp_state_t*)p->data;
    if (!state->root) return;

    if (handle_mouse(state->root, win->x + WIN_BORDER, win->y + TITLEBAR_H, 
                         win->x + WIN_BORDER + x, win->y + TITLEBAR_H + y, PON_MOUSE_UP)) {
        is_dirty(TRUE);
    }
}

static void clp_m_move(struct Window* win, int x, int y) {
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    clp_state_t* state = (clp_state_t*)p->data;
    if (!state->root) return;

    if (handle_mouse(state->root, win->x + WIN_BORDER, win->y + TITLEBAR_H, 
                         win->x + WIN_BORDER + x, win->y + TITLEBAR_H + y, PON_MOUSE_MOVE)) {
        is_dirty(TRUE);
    }
}

static void clp_on_close(struct Window* win) {
    Process* p = get_process(win->pid);
    if (p && p->data) {
        clp_state_t* state = (clp_state_t*)p->data;
        if (state->is_playing) ac97_stop_voice(state->voice_handle);
        if (state->current_file) kfree(state->current_file);
        if (state->root) PON_free(state->root);
        kfree(state);
        p->data = NULL;
    }
}

void launch_clp(const char* filename) {
    Process* p = create_process("CLPlayer");
    if (!p) return;

    clp_state_t* state =
        (clp_state_t*)kmalloc(sizeof(clp_state_t));
    memset(state, 0, sizeof(clp_state_t));
    state->voice_handle = -1;

    const char* src = filename ? filename : "/SYSTEM/startup.wav";
    state->current_file = (char*)kmalloc(strlen(src) + 1);
    strcpy(state->current_file, src);
    p->data = (char*)state;

    int win_w = CLP_WIN_W;
    int win_h = CLP_WIN_H;
    
    state->root = PANEL(0, 2, win_w - 4, win_h - TITLEBAR_H - 4, CLP_COLOR_SHELL);
    state->root->draw = draw_clp_shell;
    state->root->appdata = state;

    PON_Comp* panel = PANEL(6, 6, win_w - 16, win_h - TITLEBAR_H - 16, CLP_COLOR_PANEL);
    panel->draw = draw_clp_panel;
    PON_child(state->root, panel);

    PON_Comp* cassette = PON_summon(COMP_GENERIC, 2, 2, panel->width - 4, 90);
    cassette->draw = draw_cass;
    cassette->appdata = state;
    PON_child(panel, cassette);

    PON_Comp* btn_play = BUTTON(10, 94, CLP_BTN_W, CLP_BTN_H, "PLAY", on_play_click);
    btn_play->draw = draw_clp_button;
    btn_play->appdata = state;
    PON_child(panel, btn_play);

    PON_Comp* btn_stop = BUTTON(174, 94, CLP_BTN_W, CLP_BTN_H, "STOP", on_stop_click);
    btn_stop->draw = draw_clp_button;
    btn_stop->appdata = state;
    PON_child(panel, btn_stop);

    uint8 r = 214, g = 6, b = 96;
    Window* win = window_r(p->pid, "CL Player", -1, -1, win_w, win_h, r, g, b);
    if (!win) { 
        clp_on_close(NULL);
        cleanup_process(p->pid); 
        return; 
    }

    win->content_renderer = clp_renderer;
    win->on_mouse_down = clp_m_down;
    win->on_mouse_move = clp_m_move;
    win->on_mouse_up = clp_m_up;
    win->on_close = clp_on_close;
}