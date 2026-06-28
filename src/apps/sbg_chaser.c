#include "sbg_game.h"
#include "isr.h"
#include "graphics.h"
#include "string.h"
#include "kheap.h"
#include "timer.h"
#include "utils.h"
#include "texture_cache.h"
#include "bae.h"
#include "serial.h"
#include "keyboard.h"
#include "vesa.h"

extern uint32 timer_ticks;

#define MAX_LEN 100
#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3
#define DEATH_WALL 1
#define DEATH_SELF 2
#define INIT_RATE 8

#define GRID_CELLS 14
#define CELL_SIZE 20
#define GRID_PX (GRID_CELLS * CELL_SIZE)

#define SBG_ORANGE RGB(227, 140, 34)
#define SBG_ORANGE_HI RGB(255, 185, 80)
#define SBG_GREEN RGB(108, 217, 104)
#define SBG_GREEN_DK RGB(60, 140, 56)
#define SBG_GRID_CELL RGB(10, 10, 10)
#define SBG_SNAKE_A RGB(227, 140, 34)
#define SBG_SNAKE_B RGB(168, 104, 24)
#define SBG_SNAKE_C RGB(190, 88, 14)
#define SBG_DANGER RGB(255, 80, 70)
#define SBG_TXT_MID RGB(140, 128, 108)
#define SBG_TXT_DIM RGB(80, 72, 58)
#define SBG_PANEL_SHA RGB(6, 4, 2)
#define SBG_PANEL_HI RGB(28, 24, 20)

typedef struct {
    int x[MAX_LEN], y[MAX_LEN], px[MAX_LEN], py[MAX_LEN];
    int len, dir, ndir, dir_queue[2], dq_len;
    int food_x, food_y;
    int uctr, urat, atick;
    int food_dx, food_dy, fmctr;
    int food_pending;
    Bitmap *head_bmp, *food_bmp;
} snake_state_t;

extern void sbg_publish(sbg_state_t *g, const char *subtopic, const char *msg);

static int rng(int s, int a, int b) {
    return ((s * 1103515245 + 12345) ^ (a * 6364136223846793005LL + b)) & 0x7FFFFFFF;
}

static void food_spawn(snake_state_t *s, int seed) {
    for (int n = 0, ok = 0; !ok && n < 200; n++) {
        int rx = rng(seed+n,13,17) % (GRID_CELLS-2) + 1;
        int ry = rng(seed+n, 7,31) % (GRID_CELLS-2) + 1;
        ok = 1; for (int i = 0; i < s->len; i++) if (s->x[i]==rx && s->y[i]==ry) { ok=0; break; }
        if (ok) { s->food_x = rx; s->food_y = ry; }
    }
    int d = rng(seed,99,3)%4;
    s->food_dx = (d==1)?1:(d==3)?-1:0; s->food_dy = (d==0)?-1:(d==2)?1:0; s->fmctr = 0;
}

static void host_pub_mods(sbg_state_t *app, snake_state_t *s) {
    if (!app->is_host) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "P:%d,M:%d,S:%d", app->portals, app->moving_food, app->game_started);
    sbg_publish(app, "mods", buf);
}
static void host_pub_food(sbg_state_t *app, snake_state_t *s) {
    if (!app->is_host) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "X:%d,Y:%d,S:%d", s->food_x, s->food_y, app->game_started);
    sbg_publish(app, "food", buf);
}
static void client_pub_ate(sbg_state_t *app) {
    if (app->is_host) return;
    char sub[64];
    snprintf(sub, sizeof(sub), "ate/%s", app->username);
    sbg_publish(app, sub, "1");
}

static void snake_respawn(sbg_state_t *app, snake_state_t *s) {
    int m = GRID_CELLS / 2;
    s->x[0]=m; s->y[0]=m; s->x[1]=m-1; s->y[1]=m; s->x[2]=m-2; s->y[2]=m;
    s->px[0]=m-1; s->py[0]=m; s->px[1]=m-2; s->py[1]=m; s->px[2]=m-3; s->py[2]=m;
    s->len=3; s->dir=s->ndir=DIR_RIGHT; s->dq_len=0;
    app->game_over=0; app->death_cause=0; s->uctr=s->atick=0; s->urat=INIT_RATE;
    app->countdown=3; app->countdown_timer=timer_ticks;
}

// i apologise for this mess its my fault for asking claude to compact it
static void snake_tick(sbg_state_t *app, snake_state_t *s, int seed) {
    if (app->game_over) return;
    s->dir = s->ndir;
    if (s->dq_len > 0) { s->ndir = s->dir_queue[0]; if (s->dq_len > 1) s->dir_queue[0] = s->dir_queue[1]; s->dq_len--; }
    int nx = s->x[0] + (s->dir==DIR_RIGHT) - (s->dir==DIR_LEFT);
    int ny = s->y[0] + (s->dir==DIR_DOWN) - (s->dir==DIR_UP);
    if (app->portals) {
        if (nx < 0) nx = GRID_CELLS-1; else if (nx >= GRID_CELLS) nx = 0;
        if (ny < 0) ny = GRID_CELLS-1; else if (ny >= GRID_CELLS) ny = 0;
    } else if (nx < 0 || nx >= GRID_CELLS || ny < 0 || ny >= GRID_CELLS) {
        if (app->lives > 1) { app->lives--; snake_respawn(app, s); return; }
        app->game_over=1; app->death_cause=DEATH_WALL; return;
    }
    for (int i = 1; i < s->len; i++) {
        if (nx == s->x[i] && ny == s->y[i]) {
            if (app->lives > 1) { app->lives--; snake_respawn(app, s); return; }
            app->game_over=1; app->death_cause=DEATH_SELF; return;
        }
    }
    int ate = (nx == s->food_x && ny == s->food_y);
    for (int i = 0; i < s->len; i++) { s->px[i]=s->x[i]; s->py[i]=s->y[i]; }
    for (int i = s->len-1; i > 0; i--) { s->x[i]=s->x[i-1]; s->y[i]=s->y[i-1]; }
    s->x[0]=nx; s->y[0]=ny;
    if (ate) {
        if (s->len < MAX_LEN) { s->x[s->len]=s->x[s->len-1]; s->y[s->len]=s->y[s->len-1]; s->px[s->len]=s->x[s->len-1]; s->py[s->len]=s->y[s->len-1]; s->len++; }
        app->score++; if (s->urat > 6) s->urat--;
        if (app->is_host) { food_spawn(s, seed + app->score*17); host_pub_food(app, s); }
        else { client_pub_ate(app); s->food_pending = 1; s->food_x = -1; s->food_y = -1; }
    }
    if (app->moving_food && !ate && app->is_host && ++s->fmctr >= 3) {
        s->fmctr = 0;
        int fx = s->food_x + s->food_dx, fy = s->food_y + s->food_dy;
        if (fx <= 0 || fx >= GRID_CELLS-1) { s->food_dx = -s->food_dx; fx = s->food_x + s->food_dx; }
        if (fy <= 0 || fy >= GRID_CELLS-1) { s->food_dy = -s->food_dy; fy = s->food_y + s->food_dy; }
        int ok = 1; for (int i = 0; i < s->len; i++) if (s->x[i]==fx && s->y[i]==fy) { ok=0; break; }
        if (ok) { s->food_x = fx; s->food_y = fy; }
        host_pub_food(app, s);
    }
}

static void chaser_init(sbg_state_t *app, void **game_data, int seed) {
    snake_state_t *s = (snake_state_t*)kmalloc(sizeof(snake_state_t));
    memset(s, 0, sizeof(snake_state_t));
    *game_data = s;

    int m = GRID_CELLS / 2;
    s->x[0]=m; s->y[0]=m; s->x[1]=m-1; s->y[1]=m; s->x[2]=m-2; s->y[2]=m;
    s->px[0]=m-1; s->py[0]=m; s->px[1]=m-2; s->py[1]=m; s->px[2]=m-3; s->py[2]=m;
    s->len=3; s->dir=s->ndir=DIR_RIGHT; s->dq_len=0;
    app->game_over=app->death_cause=s->uctr=s->atick=0;
    app->last_pub_score=-1; s->urat=INIT_RATE;
    s->head_bmp = texcache_get("/SYSTEM/ggq-gg.bmp");
    s->food_bmp = texcache_get("/SYSTEM/ggq-cc.bmp");
    
    app->score = 0;
    app->lives = 3;
    
    food_spawn(s, seed);
}

static void chaser_tick(sbg_state_t *app, void *game_data) {
    snake_state_t *s = (snake_state_t*)game_data;
    if (app->countdown > 0) return;
    int can_play = app->game_started && !app->game_over;
    if (can_play) {
        if (++s->uctr >= s->urat) {
            snake_tick(app, s, app->pid + app->score);
            s->uctr = s->atick = 0;
        }
        s->atick++;
    }
    
    if (app->mqtt_state == MQ_CONNECTED && app->is_host && app->game_started) {
        if (timer_ticks % 120 == 0) host_pub_food(app, s);
    }
}

static void bmp_cell(Bitmap *b, int dx, int dy, int sz) {
    if (!b) return;
    for (int py = 0; py < sz; py++) for (int px = 0; px < sz; px++) {
        int sx = (px * b->width) / sz; if (sx >= b->width) sx = b->width - 1;
        int sy = (py * b->height) / sz; if (sy >= b->height) sy = b->height - 1;
        uint32 c = b->data[sy * b->width + sx];
        if ((c & 0xFFFFFF) != 0xFFFFFF) pixel(dx + px, dy + py, c);
    }
}

static inline void hline(int x, int y, int w, uint32 c) { rect(x, y, w, 1, c); }
static inline void vline(int x, int y, int h, uint32 c) { rect(x, y, 1, h, c); }

static void bevel_inset(int x, int y, int w, int h, uint32 hi, uint32 sha) {
    hline(x, y, w, sha);
    hline(x, y + 1, w - 1, sha);
    vline(x, y, h, sha);
    vline(x + 1, y + 1, h - 2, sha);
    hline(x + 1, y + h - 1, w - 1, hi);
    hline(x, y + h, w, hi);
    vline(x + w - 1, y + 1, h - 1, hi);
    vline(x + w, y, h, hi);
}

static void rect_clip(int x, int y, int w, int h, uint32 c,
                      int cx, int cy, int cw, int ch) {
    int x2 = x + w, y2 = y + h;
    if (x < cx) x = cx; 
    if (y < cy) y = cy;
    if (x2 > cx + cw) x2 = cx + cw; 
    if (y2 > cy + ch) y2 = cy + ch;
    if (x2 > x && y2 > y) rect(x, y, x2 - x, y2 - y, c);
}

static void rrect_border(int x, int y, int w, int h, uint32 c) {
    hline(x + 2, y, w - 4, c); hline(x + 2, y + h - 1, w - 4, c);
    vline(x, y + 2, h - 4, c); vline(x + w - 1, y + 2, h - 4, c);
    pixel(x + 1, y + 1, c); pixel(x + w - 2, y + 1, c);
    pixel(x + 1, y + h - 2, c); pixel(x + w - 2, y + h - 2, c);
}

static void chaser_render(Window *win, sbg_state_t *app, void *game_data, int gx, int gy) {
    (void)win;

    snake_state_t *s = (snake_state_t*)game_data;

    int can_play =
        app->game_started &&
        app->countdown <= 0 &&
        !app->game_over;

    int interp = (!can_play)
        ? 0
        : (s->atick * CELL_SIZE) / s->urat;

    rect(gx - 2, gy - 2, GRID_PX + 4, GRID_PX + 4, SBG_PANEL_SHA);
    bevel_inset(gx - 2, gy - 2, GRID_PX + 4, GRID_PX + 4, SBG_PANEL_HI, SBG_PANEL_SHA);
    rect(gx, gy, GRID_PX, GRID_PX, SBG_GRID_CELL);

    if (app->portals) {
        for (int i = 0; i < GRID_CELLS; i++) {
            hline(gx + i * CELL_SIZE, gy - 2, CELL_SIZE, SBG_GREEN);
            hline(gx + i * CELL_SIZE, gy + GRID_PX, CELL_SIZE, SBG_GREEN);
            vline(gx - 2, gy + i * CELL_SIZE, CELL_SIZE, SBG_GREEN);
            vline(gx + GRID_PX, gy + i * CELL_SIZE, CELL_SIZE, SBG_GREEN);
        }
    }

    #define BODY (CELL_SIZE * 6 / 10)
    #define PAD ((CELL_SIZE - BODY) / 2)
    #define CRN 3
    #define BRIDGE (CELL_SIZE + CELL_SIZE / 2)

    int srx[MAX_LEN];
    int sry[MAX_LEN];

    for (int i = 0; i < s->len; i++) {
        int dx = s->x[i] - s->px[i];
        int dy = s->y[i] - s->py[i];

        int wrap =
            (dx > 1 || dx < -1 ||
             dy > 1 || dy < -1);

        if (dx > 1) {
            dx = -1;
        }

        if (dx < -1) {
            dx = 1;
        }

        if (dy > 1) {
            dy = -1;
        }

        if (dy < -1) {
            dy = 1;
        }

        int bx2 = gx + s->x[i] * CELL_SIZE;
        int by2 = gy + s->y[i] * CELL_SIZE;

        srx[i] = wrap ? bx2 : (bx2 - dx * CELL_SIZE) + dx * interp;
        sry[i] = wrap ? by2 : (by2 - dy * CELL_SIZE) + dy * interp;
    }

    for (int i = s->len - 1; i >= 0; i--) {
        int rx = srx[i] + PAD;
        int ry = sry[i] + PAD;

        uint32 col =
            (i == 0 || i % 3 == 0)
                ? SBG_SNAKE_A
                : (i % 3 == 1)
                    ? SBG_SNAKE_B
                    : SBG_SNAKE_C;

        rect_clip(rx, ry, BODY, BODY, col, gx, gy, GRID_PX, GRID_PX);

        if (i + 1 < s->len) {
            int nx2 = srx[i + 1] + PAD;
            int ny2 = sry[i + 1] + PAD;

            int adx = nx2 - rx;
            int ady = ny2 - ry;

            if (adx < 0) {
                adx = -adx;
            }

            if (ady < 0) {
                ady = -ady;
            }

            if (adx <= BRIDGE && ady <= BRIDGE) {
                int bx3 = rx;
                int by3 = ry;
                int bw = BODY;
                int bh = BODY;

                if (nx2 > rx) {
                    bx3 = rx + BODY;
                    bw = nx2 - bx3;
                } else if (nx2 < rx) {
                    bx3 = nx2 + BODY;
                    bw = rx - bx3;
                } else if (ny2 > ry) {
                    by3 = ry + BODY;
                    bh = ny2 - by3;
                } else if (ny2 < ry) {
                    by3 = ny2 + BODY;
                    bh = ry - by3;
                }

                if (bw > 0 && bh > 0) {
                    rect_clip(bx3, by3, bw, bh, col, gx, gy, GRID_PX, GRID_PX);
                }
            }
        }

        if (i == s->len - 1 && s->len > 1) {
            int p2x = srx[i - 1] + PAD;
            int p2y = sry[i - 1] + PAD;

            if (p2x > rx) {
                rect(rx, ry, CRN, CRN, SBG_GRID_CELL);
                rect(rx, ry + BODY - CRN, CRN, CRN, SBG_GRID_CELL);
            } else if (p2x < rx) {
                rect(rx + BODY - CRN, ry, CRN, CRN, SBG_GRID_CELL);
                rect(rx + BODY - CRN, ry + BODY - CRN, CRN, CRN, SBG_GRID_CELL);
            } else if (p2y > ry) {
                rect(rx, ry, CRN, CRN, SBG_GRID_CELL);
                rect(rx + BODY - CRN, ry, CRN, CRN, SBG_GRID_CELL);
            } else {
                rect(rx, ry + BODY - CRN, CRN, CRN, SBG_GRID_CELL);
                rect(rx + BODY - CRN, ry + BODY - CRN, CRN, CRN, SBG_GRID_CELL);
            }
        }

        if (i == 0) {
            if (s->head_bmp) {
                bmp_cell(s->head_bmp, srx[i], sry[i], CELL_SIZE);
            } else {
                if (s->dir == DIR_RIGHT) {
                    rect(rx + BODY - CRN, ry, CRN, CRN, SBG_GRID_CELL);
                    rect(rx + BODY - CRN, ry + BODY - CRN, CRN, CRN, SBG_GRID_CELL);
                } else if (s->dir == DIR_LEFT) {
                    rect(rx, ry, CRN, CRN, SBG_GRID_CELL);
                    rect(rx, ry + BODY - CRN, CRN, CRN, SBG_GRID_CELL);
                } else if (s->dir == DIR_DOWN) {
                    rect(rx + BODY - CRN, ry, CRN, CRN, SBG_GRID_CELL);
                    rect(rx + BODY - CRN, ry + BODY - CRN, CRN, CRN, SBG_GRID_CELL);
                } else {
                    rect(rx, ry, CRN, CRN, SBG_GRID_CELL);
                    rect(rx + BODY - CRN, ry, CRN, CRN, SBG_GRID_CELL);
                }
            }
        }
    }

    if (s->food_x >= 0 && s->food_y >= 0) {
        int fx = gx + s->food_x * CELL_SIZE;
        int fy = gy + s->food_y * CELL_SIZE;

        if (s->food_bmp) {
            bmp_cell(s->food_bmp, fx, fy, CELL_SIZE);
        } else {
            rect(fx + 3, fy + 3, CELL_SIZE - 6, CELL_SIZE - 6, SBG_GREEN);
        }
    }

    if (!app->game_started ||
        app->countdown > 0 ||
        app->game_over) {

        for (int oy = gy; oy < gy + GRID_PX; oy += 2) {
            rect(gx, oy, GRID_PX, 1, 0x040404u | (0xFFu << 24));
        }

        int bw = GRID_PX - 16;
        int bh = 56;
        int bx2 = gx + 8;
        int by2 = gy + GRID_PX / 2 - 28;

        rect(bx2, by2, bw, bh, RGB(10, 8, 6));
        rrect_border(bx2, by2, bw, bh, SBG_ORANGE);
        hline(bx2 + 2, by2 + 1, bw - 4, SBG_ORANGE_HI);

        if (!app->game_started) {
            text("WAITING", bx2 + 8, by2 + 10, SBG_TXT_MID, FONT_DEFAULT, 1);

            text(app->is_host ? "press START to begin." : "Please wait warmly...", bx2 + 8, by2 + 28, SBG_TXT_DIM, FONT_DEFAULT, 1);
        } else if (app->countdown > 0) {
            text("READY", bx2 + 8, by2 + 8, SBG_TXT_MID, FONT_DEFAULT, 1);

            char cd[2] = {
                '0' + app->countdown,
                '\0'
            };

            text(cd, bx2 + bw / 2 - 4, by2 + 24, SBG_ORANGE, FONT_DEFAULT, 0);
        } else {
            text("GAME OVER", bx2 + 8, by2 + 8, SBG_DANGER, FONT_DEFAULT, 0);

            text(
                app->death_cause == DEATH_SELF
                    ? "Gigi ate herself."
                    : "Gigi ran into a wall.",
                bx2 + 8,
                by2 + 26,
                SBG_TXT_MID,
                FONT_DEFAULT,
                1
            );
        }
    }
}

static void draw_toggle(int x, int y, int on) {
    uint32 bg = on ? SBG_GREEN : RGB(40,36,26);
    uint32 nub = on ? SBG_GREEN_DK : RGB(26,22,14);
    rect(x, y, 34, 14, bg);
    rect(on ? x + 20 : x + 2, y + 2, 12, 10, nub);
}

static void chaser_render_sidebar(Window *win, sbg_state_t *app, void *game_data, int spx, int spy) {
    (void)win;
    snake_state_t *s = (snake_state_t*)game_data;
    #define SIDEBAR_IN 94

    if (app->is_host) {
        text("MODS", spx, spy, SBG_GREEN, FONT_DEFAULT, 1); spy += 12;
        
        int y_portals = spy; 
        draw_toggle(spx, spy + 6, app->portals); 
        text("PRTL", spx+40, spy+2, SBG_TXT_MID, FONT_DEFAULT, 1); spy += 18;
        
        int y_prey = spy; 
        draw_toggle(spx, spy + 6, app->moving_food); 
        text("PREY", spx+40, spy+2, SBG_TXT_MID, FONT_DEFAULT, 1); spy += 18;

        extern MOUSE_STATUS g_status;
        if (g_status.left_button) {
            int mx = mouse_getx(), my = mouse_gety();
            if (mx >= spx && mx < spx+34 && my >= y_portals && my < y_portals+20) {
                if (timer_ticks - app->last_click_tick > 20) {
                    app->portals ^= 1; host_pub_mods(app, s);
                    app->last_click_tick = timer_ticks;
                }
            }
            if (mx >= spx && mx < spx+34 && my >= y_prey && my < y_prey+20) {
                if (timer_ticks - app->last_click_tick > 20) {
                    app->moving_food ^= 1; host_pub_mods(app, s);
                    app->last_click_tick = timer_ticks;
                }
            }
        }
    }
}

static void chaser_handle_key(sbg_state_t *app, void *game_data, unsigned int key) {
    snake_state_t *s = (snake_state_t*)game_data;
    int can_play = app->game_started && app->countdown<=0 && !app->game_over;
    if (can_play) {
        int last=s->dq_len?s->dir_queue[s->dq_len-1]:s->dir, nd=-1;
        if (key==KEY_UP &&last!=DIR_DOWN ) nd=DIR_UP;
        else if (key==KEY_RIGHT &&last!=DIR_LEFT ) nd=DIR_RIGHT;
        else if (key==KEY_DOWN &&last!=DIR_UP ) nd=DIR_DOWN;
        else if (key==KEY_LEFT &&last!=DIR_RIGHT) nd=DIR_LEFT;
        
        if (nd>=0) {
            if (nd!=s->ndir&&s->dq_len==0) s->ndir=nd;
            else if (nd!=last&&s->dq_len<2) s->dir_queue[s->dq_len++]=nd;
        }
        if (app->is_host) {
            if (key=='1'){app->portals ^=1;host_pub_mods(app, s);}
            if (key=='2'){app->moving_food^=1;host_pub_mods(app, s);}
        }
    }
}

static void chaser_handle_mqtt(sbg_state_t *app, void *game_data, const char *re, const uint8 *payload, uint16 len) {
    snake_state_t *s = (snake_state_t*)game_data;
    
    if (strncmp(re, "/start", 6) == 0) {
        if (!app->game_started) { app->game_started = 1; app->countdown = 3; app->countdown_timer = timer_ticks; }
    } else if (strncmp(re, "/restart", 8) == 0) {
        int m = GRID_CELLS / 2;
        s->x[0]=m; s->y[0]=m; s->x[1]=m-1; s->y[1]=m; s->x[2]=m-2; s->y[2]=m;
        s->px[0]=m-1; s->py[0]=m; s->px[1]=m-2; s->py[1]=m; s->px[2]=m-3; s->py[2]=m;
        s->len=3; s->dir=s->ndir=DIR_RIGHT; s->dq_len=0;
        app->game_over=app->death_cause=s->uctr=s->atick=0;
        app->last_pub_score=-1; s->urat=INIT_RATE; app->lives=3;
        food_spawn(s, (int)timer_ticks);
        app->game_started = 1; app->countdown = 3; app->countdown_timer = timer_ticks;
        app->all_falls_down = FALSE;
    } else if (strncmp(re, "/mods", 5) == 0) {
        if (!app->is_host && len > 0 && len < 32) {
            char buf[32]; memcpy(buf, payload, len); buf[len] = '\0';
            char *pp = strstr(buf,"P:"), *mp = strstr(buf,"M:"), *sp = strstr(buf,"S:");
            if (pp) app->portals = (pp[2] == '1');
            if (mp) app->moving_food = (mp[2] == '1');
            if (sp && sp[2] == '1' && !app->game_started) { app->game_started = 1; app->countdown = 3; app->countdown_timer = timer_ticks; }
        }
    } else if (strncmp(re, "/food", 5) == 0) {
        if (!app->is_host && len > 0 && len < 32) {
            char buf[32]; memcpy(buf, payload, len); buf[len] = '\0';
            char *xp = strstr(buf,"X:"), *yp = strstr(buf,"Y:"), *sp2 = strstr(buf,"S:");
            if (xp && yp) {
                char xb[8], yb[8];
                int xl = (int)((yp - 1) - (xp + 2)); if (xl < 0) xl = 0; if (xl > 7) xl = 7;
                int yl = sp2 ? (int)((sp2 - 1) - (yp + 2)) : (int)strlen(yp+2);
                if (yl < 0) yl = 0; 
                if (yl > 7) yl = 7;
                memcpy(xb, xp+2, xl); xb[xl] = '\0';
                memcpy(yb, yp+2, yl); yb[yl] = '\0';
                s->food_x = strtoi(xb); s->food_y = strtoi(yb);
                s->food_pending = 0;
            }
            if (sp2 && sp2[2] == '1' && !app->game_started) { app->game_started = 1; app->countdown = 3; app->countdown_timer = timer_ticks; }
        }
    } else if (strncmp(re, "/ate/", 5) == 0) {
        if (app->is_host) { food_spawn(s, (int)timer_ticks + app->score * 17); host_pub_food(app, s); }
    }
}

static void chaser_cleanup(void *game_data) {
    snake_state_t *s = (snake_state_t*)game_data;
    if (s->head_bmp) texcache_rel("/SYSTEM/ggq-gg.bmp");
    if (s->food_bmp) texcache_rel("/SYSTEM/ggq-cc.bmp");
    kfree(s);
}

game_contr_t g_sbg_chaser = {
    .id = "chaser",
    .title = "CHASER",
    .init = chaser_init,
    .tick = chaser_tick,
    .render = chaser_render,
    .render_sidebar = chaser_render_sidebar,
    .handle_key = chaser_handle_key,
    .handle_mqtt = chaser_handle_mqtt,
    .cleanup = chaser_cleanup
};