#include "sbg_game.h"
#include "vesa.h"
#include "graphics.h"
#include "string.h"
#include "kheap.h"
#include "keyboard.h"

extern uint32 timer_ticks;
extern void sbg_publish(sbg_state_t *g, const char *subtopic, const char *msg);

#define COLS 10
#define ROWS 20
#define HIDDEN 2
#define TOTLINES (ROWS + HIDDEN)
#define CELL 12
#define BOARD_W (COLS * CELL)
#define BOARD_H (ROWS * CELL)

static const uint32 PIECE_COLOR[8] = {
    0, 
    RGB(0, 220, 220),
    RGB(220, 160, 0),
    RGB(160, 0, 220),
    RGB(0, 220, 0),
    RGB(220, 0, 0),
    RGB(0, 0, 220),
    RGB(220, 100, 0),
};

#define C_GHOST RGB(40,40,40)
#define C_BG RGB( 6, 6, 6)
#define C_GRID RGB(16,16,16)
#define C_BORDER RGB(60,54,38)
#define C_TXT_HI RGB(220,220,210)
#define C_TXT_MID RGB(140,128,108)
#define C_DANGER RGB(255, 80, 70)
#define C_WARN RGB(200,200, 80)
#define C_SCORE RGB(255,210, 80)

#define SBG_ORANGE RGB(227, 140, 34)
#define SBG_ORANGE_HI RGB(255, 185, 80)
#define SBG_TXT_MID RGB(140, 128, 108)
#define SBG_TXT_DIM RGB( 80, 72, 58)

typedef struct { sint8 c, r; } cell_t;

static const cell_t PIECES[7][4][4] = {
    {{{-1,0},{0,0},{1,0},{2,0}}, {{1,-1},{1,0},{1,1},{1,2}}, {{-1,1},{0,1},{1,1},{2,1}}, {{0,-1},{0,0},{0,1},{0,2}}}, // I
    {{{ 0,0},{1,0},{0,1},{1,1}}, {{ 0,0},{1,0},{0,1},{1,1}}, {{ 0,0},{1,0},{0,1},{1,1}}, {{ 0,0},{1,0},{0,1},{1,1}}}, // O
    {{{ 0,0},{-1,0},{1,0},{0,-1}}, {{ 0,0},{0,-1},{0,1},{1,0}}, {{ 0,0},{-1,0},{1,0},{0,1}}, {{ 0,0},{0,-1},{0,1},{-1,0}}}, // T
    {{{-1,0},{0,0},{0,-1},{1,-1}}, {{0,-1},{0,0},{1,0},{1,1}}, {{-1,1},{0,1},{0,0},{1,0}}, {{-1,-1},{-1,0},{0,0},{0,1}}}, // S
    {{{-1,-1},{0,-1},{0,0},{1,0}}, {{1,-1},{1,0},{0,0},{0,1}}, {{-1,0},{0,0},{0,1},{1,1}}, {{0,-1},{0,0},{-1,0},{-1,1}}}, // Z
    {{{ 0,0},{-1,0},{1,0},{-1,-1}}, {{ 0,0},{0,-1},{0,1},{1,-1}}, {{ 0,0},{-1,0},{1,0},{1,1}}, {{ 0,0},{0,-1},{0,1},{-1,1}}}, // J
    {{{ 0,0},{-1,0},{1,0},{1,-1}}, {{ 0,0},{0,-1},{0,1},{1,1}}, {{ 0,0},{-1,0},{1,0},{-1,1}}, {{ 0,0},{0,-1},{0,1},{-1,-1}}}, // L
};

typedef struct { sint8 dc, dr; } kick_t;

static const kick_t KICKS_JLSTZ[4][2][5] = {
    {{{0,0}, {-1,0}, {-1, 1}, {0,-2}, {-1,-2}}, {{0,0}, { 1,0}, { 1, 1}, {0,-2}, { 1,-2}}},
    {{{0,0}, { 1,0}, { 1,-1}, {0, 2}, { 1, 2}}, {{0,0}, { 1,0}, { 1, 1}, {0,-2}, { 1,-2}}},
    {{{0,0}, {-1,0}, {-1, 1}, {0,-2}, {-1,-2}}, {{0,0}, { 1,0}, { 1, 1}, {0,-2}, { 1,-2}}},
    {{{0,0}, {-1,0}, {-1,-1}, {0, 2}, {-1, 2}}, {{0,0}, {-1,0}, {-1, 1}, {0,-2}, {-1,-2}}}
};

static const kick_t KICKS_I[4][2][5] = {
    {{{0,-1}, {-1,-1}, {2,-1}, {2,-2}, {-1,1}}, {{1,0}, {2,0}, {-1,0}, {-1,-1}, {2,2}}},
    {{{-1,0}, {-2,0}, {1,0}, {-2,-2}, {1,1}}, {{0,-1}, {-1,-1}, {2,-1}, {-1,1}, {2,-2}}},
    {{{0,1}, {-2,1}, {1,1}, {-2,2}, {1,-1}}, {{-1,0}, {1,0}, {-2,0}, {1,1}, {-2,-2}}},
    {{{1,0}, {2,0}, {-1,0}, {2,2}, {-1,-1}}, {{0,1}, {1,1}, {-2,1}, {1,-1}, {-2,2}}}
};

static const kick_t KICKS180_JLSTZ[4][6] = {
    {{0,0}, {0,1}, {1,1}, {-1,1}, {1,0}, {-1,0}},
    {{0,0}, {1,0}, {1,2}, {1,1}, {0,2}, {0,1}},
    {{0,0}, {0,-1}, {-1,-1}, {1,-1}, {-1,0}, {1,0}},
    {{0,0}, {-1,0}, {-1,2}, {-1,1}, {0,2}, {0,1}}
};

static const kick_t KICKS180_I[4][6] = {
    {{1,-1}, {1,0}, {2,0}, {0,0}, {2,-1}, {0,-1}},
    {{-1,-1}, {0,-1}, {0,1}, {0,0}, {-1,1}, {-1,0}},
    {{-1,1}, {-1,0}, {-2,0}, {0,0}, {-2,1}, {0,1}},
    {{1,1}, {0,1}, {0,3}, {0,2}, {1,3}, {1,2}}
};

typedef struct {
    uint8 board[TOTLINES][COLS];
    uint32 magic;
    int piece, rot, pc, pr;
    int next[3];
    int hold, hold_used;
    int bag[7], bag_idx;
    int lines_cleared;
    int lines_this_drop;
    int combo;
    int garbage_pending;
    int garbage_outgoing;
    uint32 gravity_timer;
    uint32 lock_timer;
    int lock_active;
    int soft_drop;
    int level;
    uint32 rng_state;
    int das_dir;
    uint32 das_timer;
    uint32 das_last_event;
    int das_charged;
    int last_was_tspin;
} state_t;

static uint32 rng(uint32 *s) {
    *s ^= *s << 13; *s ^= *s >> 17; *s ^= *s << 5;
    return *s;
}

static void fill_bag(state_t *t) {
    for (int i = 0; i < 7; i++) t->bag[i] = i;
    for (int i = 6; i > 0; i--) {
        int j = (int)(rng(&t->rng_state) % (uint32)(i + 1));
        int tmp = t->bag[i]; t->bag[i] = t->bag[j]; t->bag[j] = tmp;
    }
    t->bag_idx = 0;
}

static int next_piece(state_t *t) {
    if (t->bag_idx >= 7) fill_bag(t);
    return t->bag[t->bag_idx++];
}

static int collides(state_t *t, int piece, int rot, int pc, int pr) {
    for (int i = 0; i < 4; i++) {
        int c = pc + PIECES[piece][rot][i].c;
        int r = pr + PIECES[piece][rot][i].r;
        if (c < 0 || c >= COLS || r >= TOTLINES) return 1;
        if (r >= 0 && t->board[r][c]) return 1;
    }
    return 0;
}

static int lock_piece(state_t *t) {
    for (int i = 0; i < 4; i++) {
        int c = t->pc + PIECES[t->piece][t->rot][i].c;
        int r = t->pr + PIECES[t->piece][t->rot][i].r;
        if (r >= 0 && r < TOTLINES && c >= 0 && c < COLS)
            t->board[r][c] = (uint8)(t->piece + 1);
    }
    int cleared = 0;
    for (int r = TOTLINES - 1; r >= 0; r--) {
        int full = 1;
        for (int c = 0; c < COLS; c++) if (!t->board[r][c]) { full = 0; break; }
        if (full) {
            for (int rr = r; rr > 0; rr--)
                for (int c = 0; c < COLS; c++) t->board[rr][c] = t->board[rr-1][c];
            for (int c = 0; c < COLS; c++) t->board[0][c] = 0;
            cleared++; r++;
        }
    }
    t->lines_this_drop = cleared;
    t->lines_cleared += cleared;
    if (cleared) { t->combo++; } else { t->combo = 0; }
    return cleared;
}

static int spawn_piece(state_t *t, int piece) {
    t->piece = piece;
    t->rot = 0;
    t->pc = COLS / 2 - 1;
    t->pr = HIDDEN - 1;
    t->lock_active = 0;
    t->lock_timer = 0;
    t->soft_drop = 0;
    t->das_dir = 0; t->das_charged = 0;
    if (collides(t, t->piece, t->rot, t->pc, t->pr)) return 0;
    return 1;
}

static uint32 gravity_delay(int level) {
    uint32 d = 40;
    for (int i = 1; i < level && d > 2; i++) d = d * 9 / 10;
    return d;
}
#define DAS_DELAY 4
#define DAS_REPEAT 8
#define LOCK_DELAY 42
#define MAGIC 0x81800555u

static void add_garbage(state_t *t, int lines) {
    for (int r = 0; r < TOTLINES - lines; r++)
        for (int c = 0; c < COLS; c++) t->board[r][c] = t->board[r + lines][c];
    int gap = (int)(rng(&t->rng_state) % (uint32)COLS);
    for (int r = TOTLINES - lines; r < TOTLINES; r++)
        for (int c = 0; c < COLS; c++) t->board[r][c] = (c == gap) ? 0 : 8;
    while (collides(t, t->piece, t->rot, t->pc, t->pr)) t->pr--;
}

static int ghost_row(state_t *t) {
    int gr = t->pr;
    while (!collides(t, t->piece, t->rot, t->pc, gr + 1)) gr++;
    return gr;
}

static int check_tspin(state_t *t, int kick_index) {
    if (t->piece != 2) return 0;

    static const sint8 corners[4][2] = {{-1,-1},{1,-1},{1,1},{-1,1}};
    static const int front[4][2] = {{0,1},{1,2},{2,3},{3,0}};

    int filled = 0;
    for (int i = 0; i < 4; i++) {
        int cc = t->pc + corners[i][0];
        int cr = t->pr + corners[i][1];
        int blocked = (cc < 0 || cc >= COLS || cr >= TOTLINES ||
                       (cr >= 0 && t->board[cr][cc]));
        if (blocked) filled++;
    }
    if (filled < 3) return 0;

    int f_filled = 0;
    for (int i = 0; i < 2; i++) {
        int idx = front[t->rot][i];
        int cc = t->pc + corners[idx][0];
        int cr = t->pr + corners[idx][1];
        int blocked = (cc < 0 || cc >= COLS || cr >= TOTLINES ||
                       (cr >= 0 && t->board[cr][cc]));
        if (blocked) f_filled++;
    }

    if (f_filled == 2 || kick_index >= 4) return 2;
    return 1;
}

static int try_rotate(state_t *t, int dir) {
    int new_rot = (t->rot + dir + 4) % 4;
    int is_180 = (dir == -2 || dir == 2);
    int kick_type = (dir == 1) ? 0 : 1;
    int num_kicks = is_180 ? 6 : 5;

    for (int k = 0; k < num_kicks; k++) {
        int dc, dr;
        if (is_180) {
            if (t->piece == 0) {
                dc = KICKS180_I[t->rot][k].dc;
                dr = KICKS180_I[t->rot][k].dr;
            } else {
                dc = KICKS180_JLSTZ[t->rot][k].dc;
                dr = KICKS180_JLSTZ[t->rot][k].dr;
            }
        } else {
            if (t->piece == 0) {
                dc = KICKS_I[t->rot][kick_type][k].dc;
                dr = KICKS_I[t->rot][kick_type][k].dr;
            } else {
                dc = KICKS_JLSTZ[t->rot][kick_type][k].dc;
                dr = KICKS_JLSTZ[t->rot][kick_type][k].dr;
            }
        }

        int nc = t->pc + dc;
        int nr = t->pr - dr;
        if (!collides(t, t->piece, new_rot, nc, nr)) {
            t->rot = new_rot; t->pc = nc; t->pr = nr;
            t->lock_active = 0;
            t->last_was_tspin = check_tspin(t, k);
            return 1;
        }
    }
    return 0;
}

static void hard_drop(sbg_state_t *app, state_t *t) {
    while (!collides(t, t->piece, t->rot, t->pc, t->pr + 1)) t->pr++;
    int cleared = lock_piece(t);

    int tspin = t->last_was_tspin;
    t->last_was_tspin = 0;

    static const int LINE_SCORE[5] = {0, 100, 300, 500, 800};
    int garbage_sent = 0;
    if (tspin == 2) {
        static const int TS_SCORE[4] = {0, 200, 800, 1400};
        static const int TS_GARB[4] = {0, 2, 4, 6};
        int idx = (cleared <= 3) ? cleared : 3;
        app->score += TS_SCORE[idx] * t->level + t->combo * 50;
        if (cleared > 0) garbage_sent = TS_GARB[idx];
    } else if (tspin == 1 && cleared >= 1) {
        app->score += 100 * t->level + t->combo * 50;
        garbage_sent = 1;
    } else {
        int ls = (cleared <= 4) ? LINE_SCORE[cleared] : 800;
        app->score += ls * t->level + t->combo * 50;
        if (cleared >= 2) garbage_sent = cleared - 1;
    }

    if (garbage_sent > 0) {
        if (t->garbage_pending >= garbage_sent) {
            t->garbage_pending -= garbage_sent;
            garbage_sent = 0;
        } else {
            garbage_sent -= t->garbage_pending;
            t->garbage_pending = 0;
        }
    }
    t->garbage_outgoing += garbage_sent;


    if (!spawn_piece(t, t->next[0])) { app->game_over = 1; return; }
    t->next[0] = t->next[1]; t->next[1] = t->next[2]; t->next[2] = next_piece(t);
    t->hold_used = 0;
    t->level = (t->lines_cleared / 10) + 1;
    t->gravity_timer = timer_ticks;
}

static void init(sbg_state_t *app, void **game_data, int seed) {
    state_t *t = (state_t*)kmalloc(sizeof(state_t));
    memset(t, 0, sizeof(state_t));
    *game_data = t;

    t->rng_state = ((uint32)seed * 2654435761u) ^ 0xDEADBEEFu ^ (uint32)timer_ticks;
    if (!t->rng_state) t->rng_state = 1;
    t->magic = MAGIC;
    rng(&t->rng_state); rng(&t->rng_state); rng(&t->rng_state);
    t->hold = -1; t->level = 1;
    fill_bag(t);
    int first = next_piece(t);
    t->next[0] = next_piece(t);
    t->next[1] = next_piece(t);
    t->next[2] = next_piece(t);
    spawn_piece(t, first);
    t->gravity_timer = timer_ticks;

    app->score = 0; app->game_over = 0; app->lives = 1;
    app->last_pub_score = -1;
}

static void tick(sbg_state_t *app, void *game_data) {
    state_t *t = (state_t*)game_data;
    if (t->magic != MAGIC) return;
    if (!app->game_started || app->countdown > 0 || app->game_over) return;

    #define DAS_EXPIRE 8
    t->soft_drop = kb_is_key_pressed(SCAN_CODE_KEY_DOWN);
    if (t->das_dir && (timer_ticks - t->das_last_event) >= DAS_EXPIRE) {
        t->das_dir = 0; t->das_charged = 0;
    }
    if (t->das_dir) {
        uint32 elapsed = timer_ticks - t->das_timer;
        if (!t->das_charged && elapsed >= DAS_DELAY) { t->das_charged = 1; t->das_timer = timer_ticks; }
        if (t->das_charged && (timer_ticks - t->das_timer) >= DAS_REPEAT) {
            int nc = t->pc + t->das_dir;
            if (!collides(t, t->piece, t->rot, nc, t->pr)) {
                t->pc = nc; t->lock_active = 0; t->last_was_tspin = 0;
            }
            t->das_timer = timer_ticks;
        }
    }

    uint32 gdelay = t->soft_drop ? 2 : gravity_delay(t->level);
    if (!t->lock_active && (timer_ticks - t->gravity_timer) >= gdelay) {
        if (!collides(t, t->piece, t->rot, t->pc, t->pr + 1)) {
            t->pr++; t->last_was_tspin = 0;
        } else {
            t->lock_active = 1; t->lock_timer = timer_ticks;
        }
        t->gravity_timer = timer_ticks;
    }

    if (t->lock_active && (timer_ticks - t->lock_timer) >= LOCK_DELAY) {
        if (collides(t, t->piece, t->rot, t->pc, t->pr + 1)) {
            if (t->garbage_pending > 0) { add_garbage(t, t->garbage_pending); t->garbage_pending = 0; }
            hard_drop(app, t);
        } else {
            t->lock_active = 0;
        }
    }

    if (t->garbage_outgoing > 0 && app->mqtt_state == MQ_CONNECTED) {
        char val[8];
        int n = t->garbage_outgoing, vi = 0;
        if (n >= 10) { val[vi++] = '0' + n / 10; }
        val[vi++] = '0' + n % 10; val[vi] = '\0';
        for (int p = 0; p < app->num_players; p++) {
            if (strncmp(app->players[p].name, app->username, 15) == 0)
                continue;
            char sub[48];
            snprintf(sub, sizeof(sub), "garbage/%s", app->players[p].name);
            sbg_publish(app, sub, val);
        }
        t->garbage_outgoing = 0;
    }
}

static void draw_cell(int x, int y, uint32 col) {
    rect(x + 1, y + 1, CELL - 2, CELL - 2, col);

    uint32 hi = 0xFF000000u | (((col & 0xFF0000u) >> 16) + 60 > 255 ? col | 0xFF0000u : col + 0x3C0000u);
    rect(x + 1, y + 1, CELL - 2, 1, hi);
    rect(x + 1, y + 1, 1, CELL - 2, hi);
}

static inline void hline(int x, int y, int w, uint32 c) { rect(x, y, w, 1, c); }
static inline void vline(int x, int y, int h, uint32 c) { rect(x, y, 1, h, c); }

static void rrect_border(int x, int y, int w, int h, uint32 c) {
    hline(x + 2, y, w - 4, c); hline(x + 2, y + h - 1, w - 4, c);
    vline(x, y + 2, h - 4, c); vline(x + w - 1, y + 2, h - 4, c);
    pixel(x + 1, y + 1, c); pixel(x + w - 2, y + 1, c);
    pixel(x + 1, y + h - 2, c); pixel(x + w - 2, y + h - 2, c);
}


static void minimino(int px, int py, int piece) {
    #define MC 8
    for (int i = 0; i < 4; i++) {
        int c = PIECES[piece][0][i].c + 1;
        int r = PIECES[piece][0][i].r + 1;
        rect(px + c*MC, py + r*MC, MC-1, MC-1, PIECE_COLOR[piece + 1]);
    }
}

static void blox_render(Window *win, sbg_state_t *app, void *game_data, int gx, int gy) {
    (void)win;
    state_t *t = (state_t*)game_data;
    if (t->magic != MAGIC) return;
    int ox = gx + (280 - BOARD_W) / 2;
    int oy = gy + (280 - BOARD_H) / 2;

    int hx = ox - 58;
    int hy = oy + 30;
    rect(hx - 1, hy - 1, 52, 44, C_BORDER);
    rect(hx, hy, 50, 42, C_BG);
    text("HOLD", hx + 2, hy - 20, C_TXT_MID, FONT_DEFAULT, 1);
    if (t->hold >= 0) minimino(hx + 5, hy + 6, t->hold);

    int mx = ox - 6;
    int my = oy;
    int mw = 4;
    int mh = BOARD_H;
    rect(mx - 1, my - 1, mw + 2, mh + 2, C_BORDER);
    rect(mx, my, mw, mh, C_BG);
    if (t->garbage_pending > 0) {
        int gh = (t->garbage_pending * mh) / ROWS;
        if (gh > mh) gh = mh;
        rect(mx, my + mh - gh, mw, gh, C_DANGER);
    }

    int nx = ox + BOARD_W + 6;
    int ny = oy + 30;
    rect(nx - 1, ny - 1, 52, 112, C_BORDER);
    rect(nx, ny, 50, 110, C_BG);
    text("NEXT", nx + 2, ny - 20, C_TXT_MID, FONT_DEFAULT, 1);
    for (int i = 0; i < 3; i++) minimino(nx + 5, ny + 5 + i * 35, t->next[i]);

    rect(ox, oy, BOARD_W, BOARD_H, C_BG);
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            rect(ox + c*CELL, oy + r*CELL, CELL, 1, C_GRID);

    for (int r = HIDDEN; r < TOTLINES; r++) {
        for (int c = 0; c < COLS; c++) {
            uint8 ci = t->board[r][c];
            if (ci) {
                uint32 col = (ci <= 7) ? PIECE_COLOR[ci] : RGB(80,80,80);
                draw_cell(ox + c*CELL, oy + (r - HIDDEN)*CELL, col);
            }
        }
    }

    int gr = ghost_row(t);
    if (gr != t->pr) {
        for (int i = 0; i < 4; i++) {
            int c = t->pc + PIECES[t->piece][t->rot][i].c;
            int r = gr + PIECES[t->piece][t->rot][i].r - HIDDEN;
            if (r >= 0) rect(ox + c*CELL + 1, oy + r*CELL + 1, CELL-2, CELL-2, C_GHOST);
        }
    }

    for (int i = 0; i < 4; i++) {
        int c = t->pc + PIECES[t->piece][t->rot][i].c;
        int r = t->pr + PIECES[t->piece][t->rot][i].r - HIDDEN;
        if (r >= 0) draw_cell(ox + c*CELL, oy + r*CELL, PIECE_COLOR[t->piece + 1]);
    }

    
    if (!app->game_started || app->countdown > 0 || app->game_over) {
        for (int oy2 = oy; oy2 < oy + BOARD_H; oy2 += 2)
            rect(ox, oy2, BOARD_W, 1, 0x040404u | (0xFFu<<24));
        int bw = 256, bh = 52, bx2 = ox - 8 - BOARD_W / 2, by2 = oy + BOARD_H/2 - 26;
        rect(bx2, by2, bw, bh, RGB(10,8,6)); 
        rrect_border(bx2, by2, bw, bh, SBG_ORANGE); 
        hline(bx2+2, by2+1, bw-4, SBG_ORANGE_HI); 
        
        if (!app->game_started) { 
            text("WAITING", bx2+8, by2+10, SBG_TXT_MID, FONT_DEFAULT, 1); 
            text(app->is_host ? "press START to begin." : "Please wait warmly...", bx2+8, by2+28, SBG_TXT_DIM, FONT_DEFAULT, 1); 
        }
        else if (app->countdown > 0) { 
            text("READY", bx2+8, by2+8, SBG_TXT_MID, FONT_DEFAULT, 1); 
            char cd[2]={'0'+app->countdown,'\0'}; 
            text(cd, bx2+bw/2-4, by2+24, SBG_ORANGE, FONT_DEFAULT, 0); 
        } else {
            text("GAME OVER", bx2+6, by2+8, C_DANGER, FONT_DEFAULT, 0);
            char lb[20]; snprintf(lb, sizeof(lb), "Lines: %d", t->lines_cleared);
            text(lb, bx2+6, by2+28, C_TXT_MID, FONT_DEFAULT, 1);
        }
    }
}

static void blox_render_sidebar(Window *win, sbg_state_t *app, void *game_data, int spx, int spy) {
    (void)win; (void)app; (void)game_data; (void)spx; (void)spy;
}

static void blox_handle_key(sbg_state_t *app, void *game_data, unsigned int key) {
    state_t *t = (state_t*)game_data;
    if (!app->game_started || app->countdown > 0 || app->game_over) return;

    if (key == KEY_LEFT || key == KEY_RIGHT) {
        int dir = (key == KEY_RIGHT) ? 1 : -1;
        t->das_last_event = timer_ticks;
        if (t->das_dir != dir) {
            int nc = t->pc + dir;
            if (!collides(t, t->piece, t->rot, nc, t->pr)) { t->pc = nc; t->lock_active = 0; t->last_was_tspin = 0; }
            t->das_dir = dir; t->das_timer = timer_ticks; t->das_charged = 0;
        }
        return;
    }

    if (key == 'z' || key == 'Z') { try_rotate(t, -1); return; }
    if (key == 'x' || key == 'X') { try_rotate(t, 1); return; }
    if (key == 'a' || key == 'A') { try_rotate(t, -2); return; }

    if (key == ' ') { hard_drop(app, t); return; }

    if (key == KEY_LSHIFT) {
        if (t->hold_used) return;
        int prev_hold = t->hold;
        t->hold = t->piece; t->hold_used = 1;
        int spawn = (prev_hold < 0) ? t->next[0] : prev_hold;
        if (prev_hold < 0) { t->next[0] = t->next[1]; t->next[1] = t->next[2]; t->next[2] = next_piece(t); }
        if (!spawn_piece(t, spawn)) { app->game_over = 1; }
        t->gravity_timer = timer_ticks;
        return;
    }
}

static void blox_handle_mqtt(sbg_state_t *app, void *game_data,
                               const char *subtopic, const uint8 *payload, uint16 len) {
    state_t *t = (state_t*)game_data;

    if (strncmp(subtopic, "/start", 6) == 0) {
        if (t->magic != MAGIC) {
            init(app, &app->game_data, (int)timer_ticks);
            t = (state_t*)app->game_data;
        }
        if (!app->game_started) { app->game_started = 1; app->countdown = 3; app->countdown_timer = timer_ticks; }
    } else if (strncmp(subtopic, "/restart", 8) == 0) {
        kfree(app->game_data);
        init(app, &app->game_data, (int)timer_ticks);
        app->game_started = 1; app->countdown = 3; app->countdown_timer = timer_ticks;
    } else if (strncmp(subtopic, "/garbage/", 9) == 0) {
        const char *target = subtopic + 9;
        if (strncmp(target, app->username, 15) == 0 && len > 0 && len < 4) {
            char buf[4]; memcpy(buf, payload, len); buf[len] = '\0';
            int lines = 0;
            for (int i = 0; buf[i] >= '0' && buf[i] <= '9'; i++) lines = lines*10 + buf[i]-'0';
            if (lines > 0 && lines <= 20) t->garbage_pending += lines;
        }
    }
}

static void blox_cleanup(void *game_data) { if (game_data) kfree(game_data); }

game_contr_t g_sbg_blox = {
    .id = "4-blox",
    .title = "4-BLOX",
    .init = init,
    .tick = tick,
    .render = blox_render,
    .render_sidebar = blox_render_sidebar,
    .handle_key = blox_handle_key,
    .handle_mqtt = blox_handle_mqtt,
    .cleanup = blox_cleanup,
};