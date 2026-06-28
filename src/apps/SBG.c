#include "apps/SBG.h"
#include "sbg_game.h"
#include "procsys.h"
#include "gui.h"
#include "string.h"
#include "graphics.h"
#include "vesa.h"
#include "keyboard.h"
#include "kheap.h"
#include "bmp.h"
#include "serial.h"
#include "texture_cache.h"
#include "net_mqtt.h"
#include "kronii.h"
#include "bae.h"
#include "pon.h"

//gpt'd because i cba to do all the accents
#define SBG_SHELL RGB(190, 88, 6)
#define SBG_SHELL_HI RGB(248, 156, 52)
#define SBG_SHELL_SHA RGB(110, 48, 2)
#define SBG_PANEL RGB(16, 14, 12)
#define SBG_PANEL_HI RGB( 28, 24, 20)
#define SBG_PANEL_SHA RGB( 6, 4, 2)
#define SBG_SCREW RGB(140, 72, 10)
#define SBG_ORANGE RGB(227, 140, 34)
#define SBG_ORANGE_HI RGB(255, 185, 80)
#define SBG_GREEN RGB(108, 217, 104)
#define SBG_GREEN_DK RGB(60, 140, 56)
#define SBG_SCORE_BG RGB(18, 12, 4)
#define SBG_SCORE_NUM RGB(255, 210, 80)
#define SBG_TXT_HI RGB(220, 220, 210)
#define SBG_TXT_MID RGB(140, 128, 108)
#define SBG_TXT_DIM RGB(80, 72, 58)
#define SBG_DANGER RGB(255, 80, 70)
#define SBG_GRID_BG RGB(4, 4, 4)
#define SBG_GRID_CELL RGB(10, 10, 10)
#define SBG_DIVIDER RGB(50, 42, 28)
#define SBG_INPUT_BG RGB(20, 18, 14)
#define SBG_INPUT_ACT RGB(28, 24, 18)
#define SBG_INPUT_BDR RGB(55, 48, 34)
#define SBG_CARD_BG RGB(16, 14, 12)
#define SBG_CARD_HOV RGB(24, 22, 18)
#define SBG_CARD_BDR RGB(44, 38, 26)
#define SBG_HOME_BG RGB(23, 7, 0)
#define SBG_LOBBY_BG RGB(8, 7, 5)
#define SBG_BTN_TXT RGB(8, 6, 2)
#define SBG_WARN RGB(200, 200, 80)
#define SBG_MENU_BTN RGB(30, 24, 14)

#define GRID_CELLS 14
#define CELL_SIZE 20
#define SIDEBAR_W 110
#define SIDEBAR_PAD 8
#define SIDEBAR_IN (SIDEBAR_W - SIDEBAR_PAD * 2)
#define WIN_BORDER 2
#define TITLE_H 20

#define GRID_PX (GRID_CELLS * CELL_SIZE)
#define WIN_W (GRID_PX + SIDEBAR_W + WIN_BORDER * 2 + 4)
#define WIN_H (GRID_PX + TITLE_H + WIN_BORDER + 4)

#define CARD_W 140
#define CARD_H 72
#define CARD_GAP 10
#define CARD_COLS 2
#define HOME_PAD 16

#define FIELD_H 36
#define FIELD_LBL 12
#define FIELD_BOX 22
#define BTN_H 26
#define BTN_GAP 8

typedef struct {
    const char *title, *subtitle, *tag;
    uint32 accent;
    const game_contr_t *interface;
} game_def_t;

extern game_contr_t g_sbg_chaser;
extern game_contr_t g_sbg_blox;

static const game_def_t GAMES[] = {
    { "CHASER", "it's snake.", "PvP", SBG_ORANGE, &g_sbg_chaser },
    { "4-BLOX", "clear lines.", "PvP", RGB(100,80,220), &g_sbg_blox }
};

#define NUM_GAMES ((int)(sizeof(GAMES)/sizeof(GAMES[0])))

static void mqtt_status_cb(mqtt_status_t status);
static void gen_room_id(char *out, int seed);

void host_pub_mods(sbg_state_t *g) { (void)g; }
void host_pub_food(sbg_state_t *g) { if (!g->is_host || g->mqtt_state != MQ_CONNECTED) return; }
void client_pub_ate(sbg_state_t *g) { if (g->is_host || g->mqtt_state != MQ_CONNECTED) return; }

void sbg_publish(sbg_state_t *g, const char *subtopic, const char *msg) {
    if (g->mqtt_state != MQ_CONNECTED) return;
    char pub[128];
    snprintf(pub, sizeof(pub), "koseki/sbg/rooms/%s/%s", g->room_id, subtopic);
    net_mqtt_publish(pub, (uint8*)msg, strlen(msg));
}

static void on_game_click(PON_Comp* comp, int x, int y) {
    (void)x; (void)y;
    sbg_state_t *g = (sbg_state_t*)comp->appdata;
    PON_Button_d *b_data = (PON_Button_d*)comp->data;
    for (int i = 0; i < NUM_GAMES; i++) {
        if (strcmp(b_data->label, GAMES[i].title) == 0) {
            g->selected_game = i; 
            g->game_interface = GAMES[i].interface;
            break;
        }
    }
    g->screen = SCREEN_LOBBY;
    is_dirty(TRUE);
}

static void on_back_click(PON_Comp* comp, int x, int y) {
    (void)x; (void)y;
    sbg_state_t *g = (sbg_state_t*)comp->appdata;
    if (g->mqtt_state != MQ_OFF) { net_mqtt_disconnect(); g->mqtt_state = MQ_OFF; }

    // reinint game state for next
    if (g->game_interface && g->game_interface->cleanup && g->game_data)
        g->game_interface->cleanup(g->game_data);
    g->game_data = NULL;
    g->is_host = FALSE; g->mqtt_state = MQ_OFF; g->join_checking = 0;
    g->screen = SCREEN_HOME;
    is_dirty(TRUE);
}

static void on_join_click(PON_Comp* comp, int x, int y) {
    (void)x; (void)y;
    sbg_state_t *g = (sbg_state_t*)comp->appdata;
    if (g->username[0] && g->room_id[0] && !g->join_checking) {
        if (g->mqtt_state != MQ_OFF) { net_mqtt_disconnect(); g->mqtt_state = MQ_OFF; }
        
        // reinit
        if (g->game_interface && g->game_interface->cleanup && g->game_data)
            g->game_interface->cleanup(g->game_data);
        g->game_data = NULL;
        if (g->game_interface && g->game_interface->init)
            g->game_interface->init(g, &g->game_data, (int)timer_ticks);
        g->join_checking = 1;
        g->join_check_timer = timer_ticks;
        g->is_host = FALSE; g->mqtt_state = MQ_CONNECTING;
        net_mqtt_set_handlestat(mqtt_status_cb); net_mqtt_init("broker.emqx.io", 1883, NULL);
    }
    is_dirty(TRUE);
}

static void on_create_click(PON_Comp* comp, int x, int y) {
    (void)x; (void)y;
    sbg_state_t *g = (sbg_state_t*)comp->appdata;
    gen_room_id(g->room_id, g->pid);
    if (g->username[0]) {
        if (g->mqtt_state != MQ_OFF) { net_mqtt_disconnect(); g->mqtt_state = MQ_OFF; }
        g->join_checking = 0;
        
        // reinit
        if (g->game_interface && g->game_interface->cleanup && g->game_data)
            g->game_interface->cleanup(g->game_data);
        g->game_data = NULL;
        if (g->game_interface && g->game_interface->init)
            g->game_interface->init(g, &g->game_data, (int)timer_ticks);
        g->is_host = TRUE; g->mqtt_state = MQ_CONNECTING;
        net_mqtt_set_handlestat(mqtt_status_cb); net_mqtt_init("broker.emqx.io", 1883, NULL);
    }
    is_dirty(TRUE);
}

static void sbg_init_ui(sbg_state_t *g) {
    int cw = WIN_W - 2*WIN_BORDER, ch = WIN_H - TITLE_H - WIN_BORDER;

    g->home_root = PANEL(0, 0, cw, ch, SBG_HOME_BG);
    PON_Comp *home_title = TEXT(HOME_PAD, 12, "GAMES", SBG_TXT_HI);
    PON_child(g->home_root, home_title);
    
    for (int i = 0; i < NUM_GAMES; i++) {
        int col = i % CARD_COLS, row = i / CARD_COLS;
        int bx = HOME_PAD + col * (CARD_W + CARD_GAP);
        int by = 60 + row * (CARD_H + CARD_GAP);
        
        PON_Comp *btn = BUTTON(bx, by, CARD_W, CARD_H, GAMES[i].title, on_game_click);
        btn->appdata = g;
        
        PON_Button_d *b_data = (PON_Button_d*)btn->data;
        b_data->bg_color = SBG_CARD_BG;
        b_data->hover_color = SBG_CARD_HOV;
        b_data->text_color = SBG_CARD_BG; // scuffed ass workaround but oh well

        PON_Comp *tt = TEXT(8, 4, GAMES[i].title, SBG_TXT_HI);
        PON_child(btn, tt);
        PON_Comp *sub = TEXT(8, 24, GAMES[i].subtitle, SBG_TXT_MID);
        PON_child(btn, sub);
        PON_Comp *tag = TEXT(8, 52, GAMES[i].tag, GAMES[i].accent);
        PON_child(btn, tag);

        PON_Comp *acc = PANEL(0, 0, 4, CARD_H, GAMES[i].accent);
        PON_child(btn, acc);

        PON_child(g->home_root, btn);
    }

    g->lobby_root = PANEL(0, 0, cw, ch, SBG_LOBBY_BG);
    int fw = (cw < 300) ? cw - 32 : 280;
    int lx = (cw - fw) / 2;
    
    PON_Comp *back = BUTTON(lx, 22, 84, 26, "< BACK", on_back_click);
    back->appdata = g;
    PON_child(g->lobby_root, back);
    
    PON_Comp *u_lbl = TEXT(lx, 50, "USERNAME", SBG_TXT_DIM);
    PON_child(g->lobby_root, u_lbl);
    g->u_in = TEXTFIELD(lx, 74, fw, 24, 16);
    PON_TextField_d *u_in_data = (PON_TextField_d*)g->u_in->data;
    kfree(u_in_data->buffer);
    u_in_data->buffer = g->username; 
    u_in_data->text_color = SBG_TXT_HI;
    u_in_data->bg_color = SBG_INPUT_BG;
    PON_child(g->lobby_root, g->u_in);
    
    PON_Comp *r_lbl = TEXT(lx, 106, "ROOM ID", SBG_TXT_DIM);
    PON_child(g->lobby_root, r_lbl);
    g->r_in = TEXTFIELD(lx, 130, fw, 24, 16);
    PON_TextField_d *r_in_data = (PON_TextField_d*)g->r_in->data;
    kfree(r_in_data->buffer);
    r_in_data->buffer = g->room_id;
    r_in_data->text_color = SBG_TXT_HI;
    r_in_data->bg_color = SBG_INPUT_BG;
    PON_child(g->lobby_root, g->r_in);
    
    PON_Comp *join_btn = BUTTON(lx, 182, fw, 32, "JOIN ROOM", on_join_click);
    join_btn->appdata = g;
    PON_Button_d *j_data = (PON_Button_d*)join_btn->data;
    j_data->bg_color = SBG_ORANGE;
    j_data->hover_color = SBG_ORANGE_HI;
    j_data->text_color = SBG_BTN_TXT;
    PON_child(g->lobby_root, join_btn);
    
    PON_Comp *create_btn = BUTTON(lx, 222, fw, 32, "CREATE ROOM", on_create_click);
    create_btn->appdata = g;
    PON_Button_d *c_data = (PON_Button_d*)create_btn->data;
    c_data->bg_color = SBG_GREEN;
    c_data->hover_color = SBG_GREEN_DK;
    c_data->text_color = SBG_BTN_TXT;
    PON_child(g->lobby_root, create_btn);
}

static inline void hline(int x, int y, int w, uint32 c) { rect(x, y, w, 1, c); }
static inline void vline(int x, int y, int h, uint32 c) { rect(x, y, 1, h, c); }

static void bevel_raised(int x, int y, int w, int h, uint32 hi, uint32 sha) {
    hline(x, y, w, hi);
    hline(x, y + 1, w - 1, hi);
    vline(x, y, h, hi);
    vline(x + 1, y + 1, h - 2, hi);
    hline(x + 1, y + h - 1, w - 1, sha);
    hline(x, y + h, w, sha);
    vline(x + w - 1, y + 1, h - 1, sha);
    vline(x + w, y, h, sha);
}
static void bevel_inset(int x, int y, int w, int h, uint32 hi, uint32 sha) {
    bevel_raised(x, y, w, h, sha, hi);
}

static void rrect_border(int x, int y, int w, int h, uint32 c) {
    hline(x + 2, y, w - 4, c); hline(x + 2, y + h - 1, w - 4, c);
    vline(x, y + 2, h - 4, c); vline(x + w - 1, y + 2, h - 4, c);
    pixel(x + 1, y + 1, c); pixel(x + w - 2, y + 1, c);
    pixel(x + 1, y + h - 2, c); pixel(x + w - 2, y + h - 2, c);
}

static void draw_shell(int x, int y, int w, int h) {
    rect(x, y, w, h, SBG_SHELL);
    bevel_raised(x, y, w, h, SBG_SHELL_HI, SBG_SHELL_SHA);
    int px = x + 5, py = y + 5, pw = w - 10, ph = h - 10;
    rect(px, py, pw, ph, SBG_PANEL);
    bevel_raised(px, py, pw, ph, SBG_PANEL_HI, SBG_PANEL_SHA);
    uint32 sc = SBG_SCREW;
    rect(x + 3, y + 3, 4, 4, sc); pixel(x + 3, y + 3, SBG_PANEL_HI);
    rect(x + w - 7, y + 3, 4, 4, sc); pixel(x + w - 7, y + 3, SBG_PANEL_HI);
    rect(x + 3, y + h - 7, 4, 4, sc); pixel(x + 3, y + h - 7, SBG_PANEL_HI);
    rect(x + w - 7, y + h - 7, 4, 4, sc); pixel(x + w - 7, y + h - 7, SBG_PANEL_HI);
}

static void prule(int x, int y, int w, uint32 c) { hline(x, y, w, c); }

static void draw_btn(int x, int y, int w, int h, const char *label,
                     uint32 face, uint32 hi, uint32 sha, uint32 tc, bool pressed) {
    rect(x, y, w, h, face);
    if (pressed) bevel_inset (x, y, w, h, hi, sha);
    else bevel_raised(x, y, w, h, hi, sha);
    int ox = pressed ? 1 : 0;
    hline(x + 2, y + 2 + ox, w - 4, hi);
    int lw = strlen(label) * 8;
    text(label, x + (w - lw) / 2 + ox - 6, y + (h - 12) / 2 + ox, tc, FONT_DEFAULT, 1);
}

static void draw_field(int x, int y, int w,
                        const char *label, const char *val, bool active) {
    text(label, x, y - 4, active ? SBG_ORANGE : SBG_TXT_DIM, FONT_DEFAULT, 1);
    int by = y + FIELD_LBL;
    rect(x, by, w, FIELD_BOX, active ? SBG_INPUT_ACT : SBG_INPUT_BG);
    bevel_inset(x, by, w, FIELD_BOX, SBG_PANEL_HI, SBG_PANEL_SHA);
    hline(x, by + FIELD_BOX, w, active ? SBG_ORANGE : SBG_INPUT_BDR);
    text(val, x + 5, by + 5, active ? SBG_SCORE_NUM : SBG_TXT_MID, FONT_DEFAULT, 1);
    if (active && (timer_ticks % 40 < 20)) {
        int cx2 = x + 4 + (int)strlen(val) * 12;
        rect(cx2, by + 4, 1, 16, SBG_SCORE_NUM);
    }
}

static bool field_hit(int mx, int my, int fx, int fy, int fw) {
    int by = fy + FIELD_LBL;
    return mx >= fx && mx < fx + fw && my >= by && my < by + FIELD_BOX;
}

static void gen_room_id(char *out, int seed) {
    const char *ch = "abcdefghijklmnopqrstuvwxyz0123456789";
    int s = seed ^ (timer_ticks * 1103515245);
    for (int i = 0; i < 6; i++) { s = s * 1103515245 + 12345; out[i] = ch[((unsigned)s >> 4) % 36]; }
    out[6] = '\0';
}

static void sanitise_name(char *dst, const char *src, int max) {
    int i = 0;
    for (; i < max - 1 && src[i]; i++) dst[i] = (src[i] >= 32 && src[i] <= 126) ? src[i] : '?';
    dst[i] = '\0';
}

static void host_pub_start(sbg_state_t *g) {
    if (!g->is_host || g->mqtt_state != MQ_CONNECTED) return;
    sbg_publish(g, "start", "1");
}
static void host_pub_restart(sbg_state_t *g) {
    if (!g->is_host || g->mqtt_state != MQ_CONNECTED) return;
    sbg_publish(g, "restart", "1");
}
static void host_pub_alive(sbg_state_t *g) {
    if (!g->is_host || g->mqtt_state != MQ_CONNECTED) return;
    sbg_publish(g, "alive", "1");
}

static void mqtt_msg_handler(const char *topic, const uint8 *payload, uint16 len) {
    Process *p = get_process_by_name("ShoeboxGames"); if (!p || !p->data) return;
    sbg_state_t *g = (sbg_state_t*)p->data;
    const char *pfx = "koseki/sbg/rooms/";
    int pfxlen = strlen(pfx);
    if (strncmp(topic, pfx, pfxlen) != 0) return;
    char room[16];
    const char *rs = topic + pfxlen, *re = strchr(rs, '/'); if (!re) return;
    int rl = re - rs; if (rl >= 16) rl = 15;
    strncpy(room, rs, rl); room[rl] = '\0';
    if (strcmp(room, g->room_id) != 0) return;

    if (strncmp(re, "/alive", 6) == 0) {
        if (g->join_checking) { g->join_checking = 0; g->screen = SCREEN_GAME; }
    } else if (strncmp(re, "/probe", 6) == 0) {
        if (g->is_host) host_pub_alive(g);
    } else if (strncmp(re, "/players/", 9) == 0) {
        char user[16]; sanitise_name(user, re + 9, 16);
        if (strcmp(user, g->username) == 0) return;
        if (g->join_checking) { g->join_checking = 0; g->screen = SCREEN_GAME; }
        int found = -1;
        for (int i = 0; i < g->num_players; i++) if (strcmp(g->players[i].name, user) == 0) { found = i; break; }
        if (len == 3 && memcmp(payload, "BYE", 3) == 0) {
            if (found != -1) { for (int j = found; j < g->num_players - 1; j++) g->players[j] = g->players[j+1]; g->num_players--; }
        } else {
            char buf[16]; int cl = len < 15 ? len : 15; memcpy(buf, payload, cl); buf[cl] = '\0';
            int sc = strtoi(buf); if (sc < 0) sc = 0; if (sc > 9999) sc = 9999;
            if (found != -1) { g->players[found].score = sc; g->players[found].last_tick = timer_ticks; }
            else if (g->num_players < MAX_PLAYERS) {
                strncpy(g->players[g->num_players].name, user, 15);
                g->players[g->num_players].name[15] = '\0';
                g->players[g->num_players].score = sc;
                g->players[g->num_players].last_tick = timer_ticks;
                g->num_players++;
            }
        }
        if (g->is_host && g->game_started) {
            int dead_count = 0;
            for (int i = 0; i < g->num_players; i++) {
                if (strcmp(g->players[i].name, g->username) != 0 && g->players[i].score == 0) dead_count++;
            }
            if (g->num_players > 1 && dead_count == g->num_players - 1 && g->all_falls_down) g->all_falls_down = TRUE;
        }
    } else {
        if (g->game_interface && g->game_interface->handle_mqtt) {
            g->game_interface->handle_mqtt(g, g->game_data, re, payload, len);
        }
    }
    is_dirty(TRUE);
}

static void mqtt_status_cb(mqtt_status_t status) {
    Process *p = get_process_by_name("ShoeboxGames"); if (!p || !p->data) return;
    sbg_state_t *g = (sbg_state_t*)p->data;
    if (status == MQTT_CONNECTED) {
        g->mqtt_state = MQ_CONNECTED;
        char sub[64];
        snprintf(sub,sizeof(sub),"koseki/sbg/rooms/%s/#",g->room_id);
        net_mqtt_subscribe(sub, mqtt_msg_handler);
        char buf[16]; itoa(buf,'d',g->score);
        char pub[64]; snprintf(pub,sizeof(pub),"koseki/sbg/rooms/%s/players/%s",g->room_id,g->username);
        net_mqtt_publish(pub,(uint8*)buf,strlen(buf));
        if (g->is_host) { host_pub_alive(g); }
        g->active_field = 0;
        if (!g->game_started) g->countdown = -1;
        if (!g->join_checking) g->screen = SCREEN_GAME;
    } else {
        g->mqtt_state = (status == MQTT_ERROR) ? MQ_ERROR : MQ_OFF;
    }
    is_dirty(TRUE);
}

static void draw_intro(Window *win, sbg_state_t *g) {
    int cx = win->x + WIN_BORDER, cy = win->y + TITLE_H;
    int cw = win->width - 2*WIN_BORDER;
    int ch = win->height - TITLE_H - WIN_BORDER;

    rect(cx, cy, cw, ch, RGB(8, 4, 2));

    Bitmap *b = g->logo_bmp;
    if (b) {
        float scx = (float)(cw * 7 / 10) / b->width;
        float scy = (float)(ch * 7 / 10) / b->height;
        float sc = scx < scy ? scx : scy;
        int sw = (int)(b->width * sc);
        int sh = (int)(b->height * sc);
        int sx = cx + (cw - sw) / 2;
        int sy = cy + (ch - sh) / 2 - 18;
        for (int y = 0; y < sh; y++) {
            for (int x = 0; x < sw; x++) {
                int bx = (x * b->width) / sw; if (bx >= b->width) bx = b->width - 1;
                int by = (y * b->height) / sh; if (by >= b->height) by = b->height - 1;
                uint32 c = b->data[by * b->width + bx];
                if ((c & 0xFFFFFF) != 0xFFFFFF) pixel(sx + x, sy + y, c);
            }
        }
    }

    const char *prompt = "<PRESS [E] TO CONTINUE>";
    int pw = (int)strlen(prompt) * 11;
    text(prompt, cx + (cw - pw) / 2, cy + ch - 42, SBG_ORANGE_HI, FONT_DEFAULT, 1);
}

static void draw_home(Window *win, sbg_state_t *g) {
    if (!g->home_root) return;
    int cx = win->x + WIN_BORDER, cy = win->y + TITLE_H;
    PON_render(g->home_root, cx, cy);
}

static void handle_home_click(Window *win, sbg_state_t *g) {
    if (!g->home_root) return;
    extern MOUSE_STATUS g_status;
    int mx = mouse_getx(), my = mouse_gety();
    int cx = win->x + WIN_BORDER, cy = win->y + TITLE_H;
    
    PON_MouseEvent event = PON_MOUSE_MOVE;
    if (g_status.left_button && !g->last_lb) event = PON_MOUSE_DOWN;
    else if (!g_status.left_button && g->last_lb) event = PON_MOUSE_UP;
    g->last_lb = (BOOL)g_status.left_button;

    if (handle_mouse(g->home_root, cx, cy, mx, my, event)) is_dirty(TRUE);
}

static void draw_lobby(Window *win, sbg_state_t *g) {
    if (!g->lobby_root) return;
    int cx = win->x + WIN_BORDER, cy = win->y + TITLE_H;
    PON_render(g->lobby_root, cx, cy);
    if (g->join_checking && (timer_ticks - g->join_check_timer) > 240) {
        g->join_checking = 0; net_mqtt_disconnect(); g->mqtt_state = MQ_OFF;
        text("ROOM NOT FOUND", cx + 20, cy + 300, SBG_DANGER, FONT_DEFAULT, 1);
    }
}

static void handle_lobby_click(Window *win, sbg_state_t *g) {
    if (!g->lobby_root) return;
    extern MOUSE_STATUS g_status;
    int mx = mouse_getx(), my = mouse_gety();
    int cx = win->x + WIN_BORDER, cy = win->y + TITLE_H;

    PON_MouseEvent event = PON_MOUSE_MOVE;
    if (g_status.left_button && !g->last_lb) event = PON_MOUSE_DOWN;
    else if (!g_status.left_button && g->last_lb) event = PON_MOUSE_UP;
    g->last_lb = (BOOL)g_status.left_button;

    if (handle_mouse(g->lobby_root, cx, cy, mx, my, event)) is_dirty(TRUE);
}

static void draw_game(Window *win, sbg_state_t *g) {
    int cx = win->x + WIN_BORDER, cy = win->y + TITLE_H;
    int cw = win->width - 2*WIN_BORDER;
    int ch = win->height - TITLE_H - WIN_BORDER;
    draw_shell(cx, cy, cw, ch);
    int ix = cx + 7, iy = cy + 7;
    int iw = cw - 14, ih = ch - 14;
    int sx = ix + iw - SIDEBAR_W;
    rect(sx, iy, SIDEBAR_W, ih, SBG_PANEL);
    bevel_inset(sx, iy, SIDEBAR_W, ih, SBG_PANEL_HI, SBG_PANEL_SHA);
    vline(sx, iy, ih, SBG_DIVIDER);
    int spx = sx + SIDEBAR_PAD, spy = iy + 6;
    text(g->game_interface ? g->game_interface->title : "SBG", spx, spy, SBG_ORANGE, FONT_DEFAULT, 0); spy += 16;
    prule(spx, spy, SIDEBAR_IN, SBG_ORANGE); spy += 6;
    rect(spx, spy, SIDEBAR_IN, 20, SBG_SCORE_BG);
    bevel_inset(spx, spy, SIDEBAR_IN, 20, SBG_PANEL_HI, SBG_PANEL_SHA);
    text("SCORE", spx+3, spy+2, SBG_TXT_DIM, FONT_DEFAULT, 1);
    {
        char b[8]; int s = g->score, si = 0;
        if (!s) { b[si++]='0'; }
        else { int t=s,d=1; while(t){t/=10;d*=10;} d/=10; while(d){b[si++]='0'+(s/d)%10;d/=10;} }
        b[si]='\0';
        text(b, spx + SIDEBAR_IN - 4 - si*8, spy+2, SBG_SCORE_NUM, FONT_DEFAULT, 0);
    }
    spy += 26;
    text("LIVES", spx, spy, SBG_TXT_DIM, FONT_DEFAULT, 1); spy += 18;
    for (int i = 0; i < 3; i++) {
        uint32 lc = (i < g->lives) ? SBG_ORANGE : RGB(36,30,18);
        rect(spx + i*14, spy, 10, 10, lc);
        bevel_raised(spx + i*14, spy, 10, 10, (i < g->lives) ? SBG_ORANGE_HI : RGB(44,38,24), (i < g->lives) ? RGB(140,70,10) : SBG_PANEL_SHA);
    }
    spy += 18;
    prule(spx, spy, SIDEBAR_IN, SBG_DIVIDER); spy += 6;
    int y_user = spy;
    draw_field(spx, spy + 4, SIDEBAR_IN, "USER", g->username, g->active_field == 1);
    spy += FIELD_H + 4;
    prule(spx, spy, SIDEBAR_IN, SBG_DIVIDER); spy += 6;
    
    if (g->game_interface && g->game_interface->render_sidebar) {
        g->game_interface->render_sidebar(win, g, g->game_data, spx, spy);
        spy += 48;
    }

    int y_start = -1;
    if (g->is_host) {
        if (!g->game_started) {
            spy += 12;
            y_start = spy; draw_btn(spx, spy, SIDEBAR_IN, 18, "START", RGB(16,44,14), SBG_GREEN, SBG_GREEN_DK, SBG_GREEN, false);
            extern MOUSE_STATUS g_status; if (g_status.left_button && mouse_getx() >= spx && mouse_getx() < spx+SIDEBAR_IN && mouse_gety() >= y_start && mouse_gety() < y_start+18) {
                host_pub_start(g); g->game_started=1; g->countdown=3; g->countdown_timer=timer_ticks;
            }
            spy += 24;
        } else if (g->all_falls_down || (g->game_over && g->num_players <= 1)) {
            spy += 12;
            y_start = spy; draw_btn(spx, spy, SIDEBAR_IN, 18, "RESTART", RGB(44,10,10), SBG_DANGER, RGB(110,20,10), SBG_DANGER, false);
            extern MOUSE_STATUS g_status; if (g_status.left_button && mouse_getx() >= spx && mouse_getx() < spx+SIDEBAR_IN && mouse_gety() >= y_start && mouse_gety() < y_start+18) {
                if (g->game_interface && g->game_interface->init) {
                    g->game_interface->cleanup(g->game_data);
                    g->game_interface->init(g, &g->game_data, (int)timer_ticks);
                }
                g->game_started=1; g->countdown=3; g->countdown_timer=timer_ticks; g->all_falls_down=FALSE;
                host_pub_restart(g);
            }
            spy += 24;
        }
        prule(spx, spy, SIDEBAR_IN, SBG_DIVIDER); spy += 6;
    }
    if (g->mqtt_state == MQ_CONNECTING) text("...", spx, spy, SBG_WARN, FONT_DEFAULT, 1);
    else if (g->mqtt_state == MQ_ERROR) text("ERR", spx, spy, SBG_DANGER, FONT_DEFAULT, 1);
    int y_back = iy + ih - 22;
    if (g->mqtt_state == MQ_CONNECTED && g->room_id[0]) {
        char rbuf[20]; snprintf(rbuf,sizeof(rbuf),"ID: %s",g->room_id);
        text(rbuf, spx, y_back - 20, SBG_TXT_HI, FONT_KALNIA, 0);
    }
    draw_btn(spx, y_back, SIDEBAR_IN, 16, "< MENU", SBG_MENU_BTN, RGB(58,46,24), SBG_PANEL_SHA, SBG_TXT_MID, false);
    extern MOUSE_STATUS g_status;
    if (g_status.left_button && mouse_getx() >= sx && mouse_getx() < sx + SIDEBAR_W) {
        if (field_hit(mouse_getx(), mouse_gety(), spx, y_user, SIDEBAR_IN)) g->active_field = 1;
        else if (mouse_gety() >= y_back && mouse_gety() < y_back+16) {
            if (g->mqtt_state == MQ_CONNECTED) { 
                char pub[64]; snprintf(pub,sizeof(pub),"koseki/sbg/rooms/%s/players/%s",g->room_id,g->username); 
                net_mqtt_publish(pub,(uint8*)"BYE",3); net_mqtt_disconnect(); 
            }
            if (g->game_interface && g->game_interface->cleanup && g->game_data) { 
                g->game_interface->cleanup(g->game_data); g->game_data = NULL;
            }
            g->mqtt_state = MQ_OFF; g->num_players = 0; g->game_started = 0; g->game_over = 0; g->death_cause = 0; g->countdown = 0; g->all_falls_down = FALSE; g->is_host = FALSE;
            g->screen = SCREEN_HOME;
        } else g->active_field = 0;
    }
    if (g->countdown > 0 && (timer_ticks - g->countdown_timer) >= 80) { g->countdown--; g->countdown_timer = timer_ticks; }
    
    if (g->game_interface && g->game_interface->tick) g->game_interface->tick(g, g->game_data);

    if (g->mqtt_state == MQ_CONNECTED) {
        int cur = g->game_over ? 0 : g->score;
        if (cur != g->last_pub_score || (timer_ticks - g->hb_timer) > 134) {
            char buf[16]; itoa(buf,'d',cur); char pub[64]; snprintf(pub,sizeof(pub),"koseki/sbg/rooms/%s/players/%s",g->room_id,g->username); net_mqtt_publish(pub,(uint8*)buf,strlen(buf));
            g->hb_timer=timer_ticks; g->last_pub_score=cur;
        }
        int found = -1;
        for (int i = 0; i < g->num_players; i++) if (strcmp(g->players[i].name,g->username)==0){found=i;break;}
        if (found != -1) { g->players[found].score=cur; g->players[found].last_tick=timer_ticks; }
        else if (g->num_players < MAX_PLAYERS) { strncpy(g->players[g->num_players].name,g->username,15); g->players[g->num_players].score=cur; g->players[g->num_players].last_tick=timer_ticks; g->num_players++; }
    }
    is_dirty(TRUE);
    int garea_w = iw - SIDEBAR_W - 1, gx = ix + (garea_w - GRID_PX) / 2, gy = iy + (ih - GRID_PX) / 2;
    if (g->game_interface && g->game_interface->render) g->game_interface->render(win, g, g->game_data, gx, gy);

    if (g->show_players && g->mqtt_state == MQ_CONNECTED) {
        int np = g->num_players, row = 14, ph = 16 + (np > 0 ? np * row : row) + 4, pw = 130, px2 = gx + GRID_PX - pw - 6, py2 = gy + 6;
        rect(px2, py2, pw, ph, RGB(8,6,4)); 

        rrect_border(px2, py2, pw, ph, SBG_ORANGE); 
        prule(px2+2, py2+1, pw-4, SBG_ORANGE_HI); 
        text("PLAYERS [L]", px2+5, py2+2, SBG_TXT_DIM, FONT_DEFAULT, 1);
        int ry = py2 + 16; if (np == 0) text("no players", px2+5, ry, SBG_TXT_DIM, FONT_DEFAULT, 1);
        else for (int i = 0; i < np; i++) { bool me = strcmp(g->players[i].name, g->username) == 0; uint32 nc = me ? SBG_ORANGE : SBG_TXT_HI; text(g->players[i].name, px2+5, ry, nc, FONT_DEFAULT, 1); char sb[8]; int sv = g->players[i].score, si = 0; if (!sv) sb[si++]='0'; else { int t=sv,d=1; while(t){t/=10;d*=10;} d/=10; while(d){sb[si++]='0'+(sv/d)%10;d/=10;} } sb[si]='\0'; text(sb, px2+pw-5-si*8, ry, me ? SBG_SCORE_NUM : SBG_TXT_MID, FONT_DEFAULT, 1); ry += row; }
    }
}

static void chaser_on_key_press(Window *win, unsigned int key) {
    if (!win) return;
    Process *p = get_process(win->pid); if (!p || !p->data) return;
    sbg_state_t *g = (sbg_state_t*)p->data;
    char c = (char)key;
    if (g->screen == SCREEN_INTRO) {
        if (c == 'e' || c == 'E') { g->screen = SCREEN_HOME; is_dirty(TRUE); }
        return;
    }
    if (g->screen == SCREEN_HOME) {
        if (handle_key(g->home_root, key)) { is_dirty(TRUE); return; }
        if (c >= '1' && c < '1'+NUM_GAMES) { 
            g->selected_game=c-'1'; 
            g->game_interface = GAMES[g->selected_game].interface;
            g->screen=SCREEN_LOBBY; 
        } 
        is_dirty(TRUE); return; 
    }
    if (g->screen == SCREEN_LOBBY) { 
        if (handle_key(g->lobby_root, key)) { is_dirty(TRUE); return; }
        if (c=='\b'||c==27) { g->screen=SCREEN_HOME; is_dirty(TRUE); } 
        return; 
    }
    if (g->screen == SCREEN_GAME) {
        if (g->active_field > 0) { 
            char *t = (g->active_field==1) ? g->username : g->room_id; 
            int l = strlen(t); 
            if (c=='\b'&&l>0) t[l-1]='\0'; 
            else if (c>=32&&c<=126&&l<15) { t[l]=c;t[l+1]='\0'; } 
            else if (c=='\n') g->active_field=0; 
            is_dirty(TRUE); return; 
        }
        if (c == 'l' || c == 'L') { g->show_players ^= 1; is_dirty(TRUE); return; }
        if (g->game_interface && g->game_interface->handle_key) {
            g->game_interface->handle_key(g, g->game_data, key);
        }
        is_dirty(TRUE);
    }
}

void chaser(Window *win) {
    if (!win) return;
    Process *p = get_process(win->pid); if (!p || !p->data) return;
    sbg_state_t *g = (sbg_state_t*)p->data;
    switch (g->screen) {
        case SCREEN_INTRO: draw_intro(win,g); break;
        case SCREEN_HOME: handle_home_click(win,g); draw_home(win,g); break;
        case SCREEN_LOBBY: handle_lobby_click(win,g);draw_lobby(win,g); break;
        case SCREEN_GAME: draw_game(win,g); break;
    }
}

static void chaser_cleanup_state(sbg_state_t *g) {
    if (!g) return;
    if (g->logo_bmp) { texcache_rel("/SYSTEM/sbg_logo.bmp"); g->logo_bmp = NULL; }
    if (g->home_root) PON_free(g->home_root);
    if (g->lobby_root) {
        if (g->u_in) ((PON_TextField_d*)g->u_in->data)->buffer = NULL;
        if (g->r_in) ((PON_TextField_d*)g->r_in->data)->buffer = NULL;
        PON_free(g->lobby_root);
    }
    if (g->game_interface && g->game_interface->cleanup) {
        g->game_interface->cleanup(g->game_data);
    }
    kfree(g);
}

void chaser_cleanup(Window *win) {
    if (!win) return;
    Process *p = get_process(win->pid); if (!p) return;
    if (p->data) {
        chaser_cleanup_state((sbg_state_t*)p->data);
        p->data = NULL;
    }
}

void launch_SBG() {
    Process *p = create_process("ShoeboxGames"); if (!p) return;
    sbg_state_t *g = (sbg_state_t*)kmalloc(sizeof(sbg_state_t));
    if (!g) { cleanup_process(p->pid); return; }
    memset(g, 0, sizeof(sbg_state_t));
    g->pid = p->pid;
    strncpy(g->username, "GIGI", 15);
    g->screen = SCREEN_INTRO;
    g->logo_bmp = texcache_get("/SYSTEM/sbg_logo.bmp");
    
    g->selected_game = 0;
    g->game_interface = GAMES[0].interface;
    if (g->game_interface && g->game_interface->init) {
        g->game_interface->init(g, &g->game_data, p->pid);
    }

    sbg_init_ui(g);
    p->data = g;
    Window *w = window(p->pid, "Shoebox Games", -1, -1, WIN_W, WIN_H);
    if (!w) { chaser_cleanup_state(g); p->data=NULL; cleanup_process(p->pid); return; }
    w->content_renderer = chaser;
    w->on_close = chaser_cleanup;
    w->on_key_press = chaser_on_key_press;
    register_for_process(p->pid, w);
}