#ifndef SBG_GAME_H
#define SBG_GAME_H

#include "sbg_shared.h"

// hopefully this will be helpful when i actually do the 6-7 games

typedef struct game_contr {
    const char *id;
    const char *title;
    
    void (*init)(sbg_state_t *app, void **game_data, int seed);
    void (*tick)(sbg_state_t *app, void *game_data);
    void (*render)(Window *win, sbg_state_t *app, void *game_data, int gx, int gy);
    void (*render_sidebar)(Window *win, sbg_state_t *app, void *game_data, int spx, int spy);
    void (*handle_key)(sbg_state_t *app, void *game_data, unsigned int key);
    void (*handle_mqtt)(sbg_state_t *app, void *game_data, const char *subtopic, const uint8 *payload, uint16 len);
    void (*cleanup)(void *game_data);
} game_contr_t;

#endif
