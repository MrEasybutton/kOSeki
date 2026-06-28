#ifndef SBG_SHARED_H
#define SBG_SHARED_H

#include "types.h"
#include "gui.h"
#include "pon.h"
#include "bmp.h"

#define MAX_PLAYERS 8
typedef struct { char name[16]; int score; uint32 last_tick; } participant_t;

typedef enum { SCREEN_INTRO, SCREEN_HOME, SCREEN_LOBBY, SCREEN_GAME } screen_t;
enum { MQ_OFF, MQ_CONNECTING, MQ_CONNECTED, MQ_ERROR };

struct game_contr;

typedef struct sbg_state {
    screen_t screen;
    int  selected_game, active_field, mqtt_state;
    char username[16], room_id[16];
    participant_t players[MAX_PLAYERS];
    int  num_players;
    uint32 hb_timer;
    BOOL is_host;

    int score, lives, countdown;
    int last_pub_score;
    BOOL game_started, game_over;
    int death_cause;
    BOOL portals, moving_food;

    uint32 countdown_timer;
    BOOL all_falls_down;

    int join_checking;
    uint32 join_check_timer;
    int show_players;

    BOOL last_lb;
    uint32 last_click_tick;
    int pid;

    Bitmap *logo_bmp;

    PON_Comp *home_root, *lobby_root;
    PON_Comp *u_in, *r_in;

    void *game_data;
    const struct game_contr *game_interface;
} sbg_state_t;

#endif
