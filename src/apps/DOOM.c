#include "apps/DOOM.h"
#include "procsys.h"
#include "gui.h"
#include "string.h"
#include "graphics.h"
#include "vesa.h"
#include "console.h"
#include "kmath.h"
#include "keyboard.h"
#include "serial.h"
#include "bmp.h"
#include "kheap.h"
#include "texture_cache.h"
#include "ac97.h"

#define COLOR_PRIMARY RGB(32, 18, 42)
#define COLOR_SECONDARY RGB(16, 6, 36)
#define COLOR_ACCENT RGB(217, 193, 150)

#define ENEMY_COLOR_PRIMARY RGB(250, 248, 252)
#define ENEMY_COLOR_SECONDARY RGB(20, 14, 22)
#define ENEMY_COLOR_ACCENT RGB(20, 20, 20)

#define PLAYER_PROJ_COLOR_PRIMARY RGB(230, 122, 181)
#define PLAYER_PROJ_COLOR_SECONDARY RGB(214, 4, 165)
#define PLAYER_PROJ_COLOR_ACCENT RGB(220, 24, 24)

#define ENEMY_PROJ_COLOR_PRIMARY RGB(12, 4, 20)
#define ENEMY_PROJ_COLOR_SECONDARY RGB(40, 20, 60)
#define ENEMY_PROJ_COLOR_ACCENT RGB(120, 80, 160)

#define MAX_ENEMIES 24
#define MAX_PROJECTILES 48
#define THISTHATUNLOCK 16
#define MELT_SLICE_WIDTH 4

extern uint32 timer_ticks;

typedef enum {
    TEX_TYPE_BMP,
    TEX_TYPE_PROCEDURAL
} DoomTextureType;

typedef struct {
    float x, y;
    float vx, vy;
    int active;
    float lifetime;
    int is_player_bullet;
} Projectile;

typedef struct {
    float x, y;
    int active;
    float shoot_timer;
    float move_timer;
    int is_dying;
    float die_timer;
} Enemy;

typedef struct {
    const char* identifier;
    DoomTextureType type;
    union {
        struct {} bmp;
        struct {
            const uint8 (*pattern)[PROC_TEX_WIDTH];
            uint8 transparent_idx;
        } procedural;
    } data;
} DoomTextureDef;

static const DoomTextureDef DOOM_TEXTURES_DEFS[] = {
    { .identifier = "/SYSTEM/doom_splash.bmp", .type = TEX_TYPE_BMP },
    { .identifier = "wall", .type = TEX_TYPE_PROCEDURAL, .data.procedural = { .pattern = PROC_TEX_WALL_PATTERN, .transparent_idx = 0 } },
    { .identifier = "window", .type = TEX_TYPE_PROCEDURAL, .data.procedural = { .pattern = PROC_TEX_WINDOW_PATTERN, .transparent_idx = 0 } },
    { .identifier = "table", .type = TEX_TYPE_PROCEDURAL, .data.procedural = { .pattern = PROC_TEX_TABLE_PATTERN, .transparent_idx = 0 } },
    { .identifier = "enemy", .type = TEX_TYPE_PROCEDURAL, .data.procedural = { .pattern = PROC_TEX_NOVELITE, .transparent_idx = 0 } },
    { .identifier = "inkblot", .type = TEX_TYPE_PROCEDURAL, .data.procedural = { .pattern = PROC_TEX_NOVELITE_INKBLOT, .transparent_idx = 0 } },
    { .identifier = "player_bullet",.type = TEX_TYPE_PROCEDURAL, .data.procedural = { .pattern = PROC_TEX_PLAYER_BULLET,.transparent_idx = 0 } }
};

#define NUM_DOOM_TEXTURES (sizeof(DOOM_TEXTURES_DEFS) / sizeof(DoomTextureDef))

void draw_splash_screen(int cx, int cy, int cw, int ch, Bitmap* splash_texture);

typedef enum {
    TEX_SPLASH = 0,
    TEX_WALL = 1,
    TEX_WINDOW = 2,
    TEX_TABLE = 3,
    TEX_ENEMY = 4,
    TEX_ENEMY_PROJECTILE = 5,
    TEX_PLAYER_PROJECTILE = 6,
} DoomTextureID;

typedef struct {
    const char* speaker_name;
    const char* icon_path;
    const char* text;
} DialogEntry;

typedef struct {
    float player_x;
    float player_y;
    float player_a;
    int game_started;
    
    Enemy enemies[MAX_ENEMIES];
    Projectile projectiles[MAX_PROJECTILES];
    
    Bitmap* textures[NUM_DOOM_TEXTURES];
    
    int health;
    float hurt_timer;
    int enemies_killed;
    
    int black_hole_unlocked;
    int black_hole_active;
    float black_hole_x;
    float black_hole_y;
    float black_hole_timer;
    float black_hole_radius;
    int music_handle;

    int melt_active;
    int* melt_offsets;
    uint32* melt_buffer;
    int melt_cw, melt_ch;

    int dialog_active;
    int current_dialog_idx;
    Bitmap* current_dialog_icon;

    int current_level;
    const DialogEntry* current_dialog_ptr;
    int current_dialog_count;

    int flash_active;
    float flash_timer;
    int flash_stage;
} DoomGameState;

static const DialogEntry PRE_GAME_DIALOGS[] = {
    { "RAORA", "SYSTEM/rao-ic.bmp", "Stop right there, Archiver!" },
    { "SHIORI", "SYSTEM/nov-ic.bmp", "Oh? You're approaching me?" },
    { "RAORA", "SYSTEM/rao-ic.bmp", "I can't beat the pasta \nout of you without getting \ncloser." },
    { "SHIORI", "SYSTEM/nov-ic.bmp", "Oh ho!" },
    { "SHIORI", "SYSTEM/nov-ic.bmp", "Then come as close as \nyou'd like." }
};

#define NUM_PRE_GAME_DIALOGS (sizeof(PRE_GAME_DIALOGS) / sizeof(DialogEntry))

static const DialogEntry STAGE2_DIALOGS[] = {
    { "SHIORI", "SYSTEM/nov-ic.bmp", "Impressive..." },
    { "RAORA", "SYSTEM/rao-ic.bmp", "Don't put your monocled \ngoons..." },
    { "RAORA", "SYSTEM/rao-ic.bmp", "Against my GOD EYES!" },
    { "SHIORI", "SYSTEM/nov-ic.bmp", "Confident, are we?" },
    { "SHIORI", "SYSTEM/nov-ic.bmp", "Ha! The others will be here \nsoon, anyway..." }
};

#define NUM_STAGE2_DIALOGS (sizeof(STAGE2_DIALOGS) / sizeof(DialogEntry))

#define WIN_BORDER 2
#define TITLEBAR_H 20
#define START_X 2.0f
#define START_Y 3.0f

#define MAP_WIDTH 24
#define MAP_HEIGHT 24

int stage1[MAP_WIDTH][MAP_HEIGHT] = {
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},
    {2,0,0,0,0,2,2,2,2,2,2,2,2,2,2,2,2,2,2,0,0,0,0,2},
    {2,0,0,0,0,2,2,0,0,0,0,0,0,0,0,0,0,2,2,0,0,0,0,2},
    {2,0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,2,2,0,0,2,2,0,0,0,0,2,2,0,0,2,2,2,2,2,2},
    {2,0,0,0,0,0,0,0,2,0,0,0,0,0,0,2,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,2,0,0,0,0,0,0,2,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,2,2,2,0,0,2,2,2,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,2,2,0,0,2,2,2,0,0,2,2,2,0,0,2,2,2,2,2,2},
    {2,0,0,0,0,0,0,0,0,2,0,0,0,0,2,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,2,0,0,0,0,2,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,2,0,0,0,0,2,0,0,0,0,2,0,0,0,0,2,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,2,0,0,0,0,2,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,2,0,0,0,0,2,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,0,0,2,2,2,2,2,0,0,2,2,2,2,2,0,0,2,2,2,2},
    {2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2},
    {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,2,2,2,2,2,0,0,2,2,2,2,2,0,0,0,0,0,2},
    {2,0,0,0,0,0,2,2,2,2,2,0,0,2,2,2,2,2,0,0,0,0,0,2},
    {2,2,2,2,2,2,2,2,2,2,2,4,4,2,2,2,2,2,2,2,2,2,2,2},
};

int stage2[MAP_WIDTH][MAP_HEIGHT] = {
    {2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2},
    {2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,2},
    {2,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,0,4,4,4,4,0,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,2,2,2,2},
    {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,0,4,0,0,4,0,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,4,4,0,0,4,4,0,0,0,0,0,0,0,0,2},
    {2,0,0,0,0,0,0,0,0,4,4,0,0,4,4,0,0,0,0,0,0,0,0,2},
    {2,0,4,4,4,4,4,4,4,4,4,0,0,4,4,4,4,4,4,4,4,4,0,2},
    {2,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,2},
    {2,0,4,0,2,2,2,2,2,2,2,0,0,2,2,2,2,2,2,2,0,4,0,2},
    {2,0,4,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,4,0,2},
    {2,0,4,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,4,0,2},
    {2,0,4,0,0,0,0,0,0,0,2,0,0,2,0,0,0,0,0,0,0,4,0,2},
    {2,0,4,0,2,0,0,0,2,2,2,0,0,2,2,2,0,0,0,2,0,4,0,2},
    {2,0,4,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,4,0,2},
    {2,0,4,0,2,2,2,2,2,2,2,0,0,2,2,2,2,2,2,2,0,4,0,2},
    {2,0,4,0,0,0,0,0,0,0,2,0,0,2,0,0,0,0,0,0,0,4,0,2},
    {2,0,4,4,4,4,4,4,4,4,2,0,0,2,4,4,4,4,4,4,4,4,0,2},
    {2,2,2,2,2,2,2,2,2,2,2,4,4,2,2,2,2,2,2,2,2,2,2,2},
};

static uint32 rng = 8180014555u;

static float randf()
{
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;

    return (rng >> 8) * (1.0f / 16777216.0f);
}

static inline float clamp(float x, float a, float b) {
    return (x < a) ? a : (x > b) ? b : x;
}

// ts only exists because the actual bitmap texturing crashed the OS early-on and i cba to fix it
Bitmap* draw_p_bmp(const uint8 pattern[PROC_TEX_HEIGHT][PROC_TEX_WIDTH], 
                                 uint32 color1, 
                                 uint32 color2, 
                                 uint32 color3,
                                 uint8 tp_idx) {
    Bitmap* bmp = (Bitmap*)kmalloc(sizeof(Bitmap));
    if (!bmp) {
        kprint("PROC_TEX: Failed to allocate Bitmap struct\n");
        return NULL;
    }

    bmp->width = PROC_TEX_WIDTH;
    bmp->height = PROC_TEX_HEIGHT;
    bmp->data = (uint32*)kmalloc(bmp->width * bmp->height * sizeof(uint32));
    if (!bmp->data) {
        kprint("PROC_TEX: Failed to allocate pixel data\n");
        kfree(bmp);
        return NULL;
    }

    for (int y = 0; y < PROC_TEX_HEIGHT; y++) {
        for (int x = 0; x < PROC_TEX_WIDTH; x++) {
            uint32 color;
            uint8 pattern_val = pattern[y][x];

            if (pattern_val == tp_idx) {
                color = RGB(255, 255, 255);
            } else if (pattern_val == 1) {
                color = color1;
            } else if (pattern_val == 2) {
                color = color2;
            } else if (pattern_val == 3) {
                color = color3;
            } else {
                color = RGB(0, 0, 0);
            }
            bmp->data[y * bmp->width + x] = color;
        }
    }
    kprint("PROC_TEX: procBMP done\n");
    return bmp;
}

static inline int (*get_map(DoomGameState* game))[MAP_HEIGHT] {
    return (game->current_level == 1) ? stage2 : stage1;
}

void init_enemies(DoomGameState* game) {
    int (*map)[MAP_HEIGHT] = get_map(game);
    int spawn_cells[MAP_WIDTH * MAP_HEIGHT][2];
    int spawn_count = 0;
    const float min_distance_sq = 6.0f * 6.0f;

    for (int x = 0; x < MAP_WIDTH; x++) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            if (map[x][y] != 0) continue;

            float spawn_x = (float)x + 0.5f;
            float spawn_y = (float)y + 0.5f;
            float dx = spawn_x - START_X;
            float dy = spawn_y - START_Y;

            if (dx * dx + dy * dy < min_distance_sq) continue;

            spawn_cells[spawn_count][0] = x;
            spawn_cells[spawn_count][1] = y;
            spawn_count++;
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (spawn_count == 0) {
            game->enemies[i].active = 0;
            continue;
        }

        int index = (int)(randf() * spawn_count);
        if (index >= spawn_count) index = spawn_count - 1;

        int cell_x = spawn_cells[index][0];
        int cell_y = spawn_cells[index][1];
        spawn_cells[index][0] = spawn_cells[spawn_count - 1][0];
        spawn_cells[index][1] = spawn_cells[spawn_count - 1][1];
        spawn_count--;

        game->enemies[i].x = (float)cell_x + 0.5f;
        game->enemies[i].y = (float)cell_y + 0.5f;
        game->enemies[i].active = 1;
        game->enemies[i].is_dying = 0;
        game->enemies[i].die_timer = 0;
        game->enemies[i].shoot_timer = randf() * 3.0f;
        game->enemies[i].move_timer = randf() * 0.5f;
    }
}

void update_enemies(DoomGameState* game, float dt) {
    int (*map)[MAP_HEIGHT] = get_map(game);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game->enemies[i].active) continue;
        
        Enemy* e = &game->enemies[i];
        if (e->is_dying) {
            e->die_timer -= dt;
            if (e->die_timer <= 0) {
                e->active = 0;
            }
            continue;
        }
        
        e->shoot_timer -= dt;
        e->move_timer -= dt;
        
        if (e->shoot_timer <= 0) {
            float dx = game->player_x - e->x;
            float dy = game->player_y - e->y;
            float dist = sqrt(dx * dx + dy * dy);
            
            if (dist > 0.1f && dist < 15.0f) {
                for (int j = 0; j < MAX_PROJECTILES; j++) {
                    if (!game->projectiles[j].active) {
                        game->projectiles[j].x = e->x;
                        game->projectiles[j].y = e->y;
                        game->projectiles[j].vx = (dx / dist) * 0.05f;
                        game->projectiles[j].vy = (dy / dist) * 0.05f;
                        game->projectiles[j].active = 1;
                        game->projectiles[j].lifetime = 7.5f;
                        game->projectiles[j].is_player_bullet = 0;
                        break;
                    }
                }
            }
            
            e->shoot_timer = 3.0f + randf() * 5.0f;
        }
        
        if (e->move_timer <= 0) {
            float dx = game->player_x - e->x;
            float dy = game->player_y - e->y;
            float dist = sqrt(dx * dx + dy * dy);
            
            float move_x = 0, move_y = 0;
            if (dist > 2.0f && dist < 32.0f) { //chasing
                move_x = (dx / dist) * 0.42f;
                move_y = (dy / dist) * 0.42f;
                
                float angle = randf() * 360.0f;
                move_x += cos_deg(angle) * 0.12f;
                move_y += sin_deg(angle) * 0.12f;
            } else {
                float angle = randf() * 360.0f;
                move_x = cos_deg(angle) * 0.2f;
                move_y = sin_deg(angle) * 0.2f;
            }
            
            int next_x = map[(int)(e->x + move_x)][(int)e->y];
            int next_y = map[(int)e->x][(int)(e->y + move_y)];
            
            if (next_x == 0) e->x += move_x;
            if (next_y == 0) e->y += move_y;
            
            e->move_timer = 2.0f + randf() * 1.0f;
        }
    }
}

void update_projectiles(DoomGameState* game, float dt) {
    int (*map)[MAP_HEIGHT] = get_map(game);
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!game->projectiles[i].active) continue;
        
        Projectile* p = &game->projectiles[i];
        p->lifetime -= dt;
        
        if (p->lifetime <= 0) {
            p->active = 0;
            continue;
        }
        
        p->x += p->vx;
        p->y += p->vy;
        
        int tile = map[(int)p->x][(int)p->y];
        if (tile != 0 && tile != 5) {
            p->active = 0;
            continue;
        }
        
        if (p->is_player_bullet) {
            for (int j = 0; j < MAX_ENEMIES; j++) {
                if (!game->enemies[j].active || game->enemies[j].is_dying) continue;
                
                float dx = game->enemies[j].x - p->x;
                float dy = game->enemies[j].y - p->y;
                if (dx * dx + dy * dy < 0.4f * 0.4f) {
                    game->enemies[j].is_dying = 1;
                    game->enemies[j].die_timer = 0.5f;
                    game->enemies_killed++;
                    
                    if (game->enemies_killed >= THISTHATUNLOCK && !game->black_hole_unlocked) {
                        game->black_hole_unlocked = 1;
                    }
                    
                    p->active = 0;
                    break;
                }
            }
        } else {
            float dx = game->player_x - p->x;
            float dy = game->player_y - p->y;
            if (dx * dx + dy * dy < 0.3f * 0.3f) {
                game->health -= 20;
                game->hurt_timer = 0.2f;
                p->active = 0;
            }
        }
    }
}

void setup_stage(DoomGameState* game, int level) {
    game->current_level = level;
    game->player_x = START_X;
    game->player_y = START_Y;
    game->player_a = 0.0f;
    game->health = 100;
    game->hurt_timer = 0;
    game->enemies_killed = 0;
    game->black_hole_unlocked = 0;
    game->black_hole_active = 0;

    init_enemies(game);

    game->dialog_active = 1;
    game->current_dialog_idx = 0;
    if (level == 0) {
        game->current_dialog_ptr = PRE_GAME_DIALOGS;
        game->current_dialog_count = NUM_PRE_GAME_DIALOGS;
    } else {
        game->current_dialog_ptr = STAGE2_DIALOGS;
        game->current_dialog_count = NUM_STAGE2_DIALOGS;
    }
    
    game->current_dialog_icon = texcache_get(game->current_dialog_ptr[0].icon_path);
}

void start_flash(DoomGameState* game, int next_level) {
    game->flash_active = 1;
    game->flash_timer = 0.5f;
    game->flash_stage = next_level;
}

void start_melt(Window* win, DoomGameState* game) {
    int cx = win->x + WIN_BORDER;
    int cy = win->y + TITLEBAR_H;
    int cw = win->width - (2 * WIN_BORDER);
    int ch = win->height - TITLEBAR_H - WIN_BORDER;

    game->melt_active = 1;
    game->melt_cw = cw;
    game->melt_ch = ch;
    
    int num_slices = (cw + MELT_SLICE_WIDTH - 1) / MELT_SLICE_WIDTH;
    if (game->melt_offsets) kfree(game->melt_offsets);
    if (game->melt_buffer) kfree(game->melt_buffer);
    
    game->melt_offsets = (int*)kmalloc(num_slices * sizeof(int));
    game->melt_buffer = (uint32*)kmalloc(cw * ch * sizeof(uint32));
    
    if (game->melt_offsets && game->melt_buffer) {
        for (int y = 0; y < ch; y++) {
            for (int x = 0; x < cw; x++) {
                game->melt_buffer[y * cw + x] = g_back_buffer[(cy + y) * g_width + (cx + x)];
            }
        }
        for (int i = 0; i < num_slices; i++) {
            game->melt_offsets[i] = -(int)(randf() * 140);
        }
    }
}

void update_black_hole(Window* win, DoomGameState* game, float dt) {
    if (!game->black_hole_active) return;
    
    game->black_hole_timer -= dt;
    game->black_hole_radius += dt * 4.0f;
    
    if (game->black_hole_timer <= 0) {
        game->black_hole_active = 0;
        if (game->music_handle != -1) {
            ac97_stop_voice(game->music_handle);
            game->music_handle = -1;
        }
        return;
    }
    
    float player_dx = game->black_hole_x - game->player_x;
    float player_dy = game->black_hole_y - game->player_y;
    float player_dist = sqrt(player_dx * player_dx + player_dy * player_dy);
    
    if (player_dist < game->black_hole_radius) {
        game->game_started = 0;
        game->player_x = START_X;
        game->player_y = START_Y;
        game->player_a = 0.0f;
        game->black_hole_active = 0;

        if (game->music_handle != -1) {
            ac97_stop_voice(game->music_handle);
            game->music_handle = -1;
        }

        for (int i = 0; i < MAX_PROJECTILES; i++) {
            game->projectiles[i].active = 0;
        }
        for (int i = 0; i < MAX_ENEMIES; i++) {
            game->enemies[i].active = 0;
        }
        
        return;
    } else if (player_dist < game->black_hole_radius + 16.0f) {
        float pull_strength = 0.16f; 
        game->player_x += (player_dx / player_dist) * pull_strength;
        game->player_y += (player_dy / player_dist) * pull_strength;
    }
    
    int enemies_remaining = 0;
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!game->enemies[i].active || game->enemies[i].is_dying) continue;
        enemies_remaining++;
        
        float dx = game->black_hole_x - game->enemies[i].x;
        float dy = game->black_hole_y - game->enemies[i].y;
        float dist = sqrt(dx * dx + dy * dy);
        
        if (dist < game->black_hole_radius) {
            game->enemies[i].is_dying = 1;
            game->enemies[i].die_timer = 0.2f;
            game->enemies_killed++;
            enemies_remaining--;
        } else if (dist < game->black_hole_radius + 16.0f) {
            float pull_strength = 0.67f;
            game->enemies[i].x += (dx / dist) * pull_strength;
            game->enemies[i].y += (dy / dist) * pull_strength;
        }
    }

    if (enemies_remaining == 0) {
        game->black_hole_active = 0;
        if (game->current_level == 0) {
            start_flash(game, 1);
        }
    }
}

void handle_input(Window* win, DoomGameState* game) {
    if (!game || !win) return;
    
    char scancode = kb_get_scancode_nb();

    //cheats!!
    if (kb_is_key_pressed(SCAN_CODE_KEY_LEFT_CTRL)) {
        if (scancode == SCAN_CODE_KEY_1) {
            start_flash(game, 0);
            return;
        }
        if (scancode == SCAN_CODE_KEY_2) {
            start_flash(game, 1);
            return;
        }
    }
    
    if (game->dialog_active) {
        if (scancode == SCAN_CODE_KEY_Z) {
            game->current_dialog_idx++;
            
            if (game->current_dialog_icon) {
                texcache_rel(game->current_dialog_ptr[game->current_dialog_idx - 1].icon_path);
                game->current_dialog_icon = NULL;
            }
            
            if (game->current_dialog_idx >= game->current_dialog_count) {
                game->dialog_active = 0;
                if (game->music_handle == -1) {
                    game->music_handle = ac97_play("/SYSTEM/doom_r.mp3");
                }
            } else {
                game->current_dialog_icon = texcache_get(game->current_dialog_ptr[game->current_dialog_idx].icon_path);
            }
        }
        return;
    }

    if (!game->game_started && scancode == SCAN_CODE_KEY_E) {
        int cx = win->x + WIN_BORDER;
        int cy = win->y + TITLEBAR_H;
        int cw = win->width - (2 * WIN_BORDER);
        int ch = win->height - TITLEBAR_H - WIN_BORDER;
        draw_splash_screen(cx, cy, cw, ch, game->textures[TEX_SPLASH]);

        start_melt(win, game);
        setup_stage(game, 0);
        game->game_started = 1;
        return;
    }
    if (!game->game_started) return;

    BOOL key_w = kb_is_key_pressed(SCAN_CODE_KEY_W);
    BOOL key_a = kb_is_key_pressed(SCAN_CODE_KEY_A);
    BOOL key_s = kb_is_key_pressed(SCAN_CODE_KEY_S);
    BOOL key_d = kb_is_key_pressed(SCAN_CODE_KEY_D);

    float dx = 0.0f, dy = 0.0f;
    if (key_w) {
        dx = cos_deg(game->player_a) * 0.08f;
        dy = sin_deg(game->player_a) * 0.08f;
    }
    if (key_s) {
        dx = -cos_deg(game->player_a) * 0.08f;
        dy = -sin_deg(game->player_a) * 0.08f;
    }

    if (key_a) {
        game->player_a -= 3.6f;
        if (game->player_a < 0) game->player_a += 360;
    }
    
    if (key_d) {
        game->player_a += 3.6f;
        if (game->player_a > 360) game->player_a -= 360;
    }

    int (*map)[MAP_HEIGHT] = get_map(game);
    int next_x = map[(int)(game->player_x + dx)][(int)game->player_y];
    int next_y = map[(int)game->player_x][(int)(game->player_y + dy)];
    if (dx != 0 && (next_x == 0 || next_x == 5)) game->player_x += dx;
    if (dy != 0 && (next_y == 0 || next_y == 5)) game->player_y += dy;
    
    if (scancode == SCAN_CODE_KEY_E) {
        if (game->black_hole_unlocked && !game->black_hole_active) {
            game->black_hole_active = 1;
            game->black_hole_x = game->player_x + cos_deg(game->player_a) * 5.0f;
            game->black_hole_y = game->player_y + sin_deg(game->player_a) * 5.0f;
            game->black_hole_timer = 4.0f;
            game->black_hole_radius = 0.5f;
            game->black_hole_unlocked = 0;
        } else {
            for (int i = 0; i < MAX_PROJECTILES; i++) {
                if (!game->projectiles[i].active) {
                    game->projectiles[i].x = game->player_x;
                    game->projectiles[i].y = game->player_y;
                    game->projectiles[i].vx = cos_deg(game->player_a) * 0.12f;
                    game->projectiles[i].vy = sin_deg(game->player_a) * 0.12f;
                    game->projectiles[i].active = 1;
                    game->projectiles[i].lifetime = 8.0f;
                    game->projectiles[i].is_player_bullet = 1;
                    break;
                }
            }
        }
    }
}

#define FOG_TABLE_SIZE 256
static uint32 fog_table[FOG_TABLE_SIZE];
static int fog_table_initialized = 0;

void init_fog_table() {
    if (fog_table_initialized) return;
    for (int i = 0; i < FOG_TABLE_SIZE; i++) {
        float dist = (float)i * 20.0f / FOG_TABLE_SIZE;
        const float fog_scale = (FOG_TABLE_SIZE - 1) / 20.0f;
        int idx = (int)(dist * fog_scale);
        float f = clamp(1.0f / expf(dist * 0.18f), 0.0f, 1.0f);
        fog_table[idx] = (uint32)(f * 255.0f);
    }
    fog_table_initialized = 1;
}

static inline uint32 apply_fog(uint32 color, float dist) {
    int idx = (int)((dist / 20.0f) * (FOG_TABLE_SIZE - 1));
    if (idx >= FOG_TABLE_SIZE) idx = FOG_TABLE_SIZE - 1;
    if (idx < 0) idx = 0;
    
    uint32 fog = fog_table[idx];
    uint32 inv = 255 - fog;
    
    if (fog == 255) return color;
    if (fog == 0) return RGB(24, 24, 24);
    
    uint32 r = (((color >> 16) & 0xFF) * fog + 40 * inv) >> 8;
    uint32 g = (((color >> 8) & 0xFF) * fog + 40 * inv) >> 8;
    uint32 b = ((color & 0xFF) * fog + 40 * inv) >> 8;
    
    return RGB(r, g, b);
}

typedef struct {
    uint32 fog;
    uint32 inv;
    uint32 fog_base; // 40 * inv
} FogInfo;

static inline void get_fog_info(float dist, FogInfo* out) {
    int idx = (int)((dist / 20.0f) * (FOG_TABLE_SIZE - 1));
    if (idx >= FOG_TABLE_SIZE) idx = FOG_TABLE_SIZE - 1;
    if (idx < 0) idx = 0;
    out->fog = fog_table[idx];
    out->inv = 255 - out->fog;
    out->fog_base = 40 * out->inv;
}

static inline uint32 apply_fog_fast(uint32 color, const FogInfo* fi) {
    uint32 r = (((color >> 16) & 0xFF) * fi->fog + fi->fog_base) >> 8;
    uint32 g = (((color >> 8) & 0xFF) * fi->fog + fi->fog_base) >> 8;
    uint32 b = ((color & 0xFF) * fi->fog + fi->fog_base) >> 8;
    return RGB(r, g, b);
}

static inline uint32 sample_texture(Bitmap* tex, float u, float v, float dist) {
    if (!tex) return RGB(100, 150, 200);
    
    u = u - floor(u);
    v = v - floor(v);
    
    int x = (int)(u * tex->width);
    int y = (int)(v * tex->height);
    
    if (x >= tex->width) x = tex->width - 1;
    if (y >= tex->height) y = tex->height - 1;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    
    return apply_fog(tex->data[y * tex->width + x], dist);
}

void draw_dialog(int cx, int cy, int cw, int ch, DoomGameState* game) {
    if (!game->dialog_active) return;
    
    const DialogEntry* d = &game->current_dialog_ptr[game->current_dialog_idx];
    
    int db_h = 100;
    int db_y = cy + ch - db_h - 10;
    int db_x = cx + 10;
    int db_w = cw - 20;
    
    rect(db_x + 4, db_y + 4, db_w, db_h, RGBA(0, 0, 0, 100));
    rect(db_x, db_y, db_w, db_h, RGB(20, 15, 35));
    
    for (int i = 0; i < 2; i++) {
        line(db_x + i, db_y + i, db_x + db_w - 1 - i, db_y + i, RGB(150, 100, 255));
        line(db_x + i, db_y + db_h - 1 - i, db_x + db_w - 1 - i, db_y + db_h - 1 - i, RGB(150, 100, 255));
        line(db_x + i, db_y + i, db_x + i, db_y + db_h - 1 - i, RGB(150, 100, 255));
        line(db_x + db_w - 1 - i, db_y + i, db_x + db_w - 1 - i, db_y + db_h - 1 - i, RGB(150, 100, 255));
    }
    
    if (game->current_dialog_icon) {
        int icon_size = 80;
        int ix = db_x + 10;
        int iy = db_y + (db_h - icon_size) / 2;
        
        Bitmap* icon = game->current_dialog_icon;
        for (int y = 0; y < icon_size; y++) {
            for (int x = 0; x < icon_size; x++) {
                int src_x = (x * icon->width) / icon_size;
                int src_y = (y * icon->height) / icon_size;
                uint32 c = icon->data[src_y * icon->width + src_x];
                
                if (c != RGB(255, 255, 255)) {
                    g_back_buffer[(iy + y) * g_width + (ix + x)] = c;
                }
            }
        }
    }
    
    text(d->speaker_name, db_x + 100, db_y + 10, RGB(255, 200, 100), FONT_KALNIA, true);
    
    int ty = db_y + 35;
    char line_buf[128];
    int line_ptr = 0;
    const char* text_ptr = d->text;
    
    while (*text_ptr) {
        if (*text_ptr == '\n' || line_ptr >= 127) {
            line_buf[line_ptr] = '\0';
            text(line_buf, db_x + 100, ty, RGB(230, 220, 255), FONT_KALNIA, false);
            ty += 15; // nextline
            line_ptr = 0;
        } else {
            line_buf[line_ptr++] = *text_ptr;
        }
        text_ptr++;
    }
    if (line_ptr > 0) {
        line_buf[line_ptr] = '\0';
        text(line_buf, db_x + 100, ty, RGB(230, 220, 255), FONT_KALNIA, false);
    }
    
    text("[Z] NEXT", db_x + db_w - 96, db_y + db_h - 20, RGB(150, 150, 255), FONT_KALNIA, true);
}

void draw_splash_screen(int cx, int cy, int cw, int ch, Bitmap* splash_texture) {
    rect(cx, cy, cw, ch, RGB(20, 20, 30));
    
    if (splash_texture) {
        float scale = (cw * 0.6f / splash_texture->width < ch * 0.6f / splash_texture->height) ?
                      cw * 0.6f / splash_texture->width : ch * 0.6f / splash_texture->height;
        
        int sw = (int)(splash_texture->width * scale);
        int sh = (int)(splash_texture->height * scale);
        int sx = cx + (cw - sw) / 2;
        int sy = cy + (ch - sh) / 2 - 50;
        
        for (int y = 0; y < sh; y++) {
            for (int x = 0; x < sw; x++) {
                int src_x = (x * splash_texture->width) / sw;
                int src_y = (y * splash_texture->height) / sh;
                if (src_x >= splash_texture->width) src_x = splash_texture->width - 1;
                if (src_y >= splash_texture->height) src_y = splash_texture->height - 1;
                
                uint32 c = splash_texture->data[src_y * splash_texture->width + src_x];
                if (c != RGB(255, 255, 255)) {
                    pixel(sx + x, sy + y, c);
                }
            }
        }
    }
    
    text("< PRESS [E] TO PLAY >", cx + (cw - 245) / 2, cy + 205, 
         RGB(200, 200, 255), FONT_KALNIA, true);
}

void draw_game_ui(int cx, int cy, DoomGameState* game) {
    int bar_width = 200;
    int bar_height = 20;
    int bar_x = cx + 10;
    int bar_y = cy + 10;
    
    rect(bar_x, bar_y, bar_width, bar_height, RGB(60, 60, 60));
    
    int health_width = (game->health * bar_width) / 100;
    if (health_width > 0) {
        uint32 health_color;
        if (game->health > 60) {
            health_color = RGB(100, 220, 100);
        } else if (game->health > 30) {
            health_color = RGB(220, 220, 100);
        } else {
            health_color = RGB(220, 100, 100);
        }
        rect(bar_x, bar_y, health_width, bar_height, health_color);
    }
    
    for (int i = 0; i < 2; i++) {
        for (int x = 0; x < bar_width; x++) {
            pixel(bar_x + x, bar_y + i, RGB(200, 200, 200));
            pixel(bar_x + x, bar_y + bar_height - 1 - i, RGB(200, 200, 200));
        }
        for (int y = 0; y < bar_height; y++) {
            pixel(bar_x + i, bar_y + y, RGB(200, 200, 200));
            pixel(bar_x + bar_width - 1 - i, bar_y + y, RGB(200, 200, 200));
        }
    }
    
    char kills_text[32];
    sprintf(kills_text, "KILLS: %d", game->enemies_killed);
    text(kills_text, cx + 10, cy + 40, RGB(255, 255, 255), FONT_KALNIA, true);
    
    if (game->black_hole_unlocked) {
        text("PRESS [E] FOR DOOM", cx + 10, cy + 60, RGB(220, 105, 100), FONT_KALNIA, true);
    } else if (game->enemies_killed < THISTHATUNLOCK) {
        char unlock_text[32];
        sprintf(unlock_text, "DOOM => [%d/%d]", game->enemies_killed, THISTHATUNLOCK);
        text(unlock_text, cx + 10, cy + 60, RGB(100, 100, 150), FONT_KALNIA, true);
    }
}

void render_sprite(int cx, int cy, int ch, int cw, float sprite_x, float sprite_y,
                   float player_x, float player_y, float player_a,
                   Bitmap* sprite_tex, float sprite_scale, float* depth_buffer, float die_timer) {
    float dx = sprite_x - player_x;
    float dy = sprite_y - player_y;
    float dist = sqrt(dx * dx + dy * dy);
    
    if (dist < 0.1f || dist > 20.0f) return;
    
    float sprite_angle = atan2(dy, dx) * 180.0f / 3.14159f;
    float angle_diff = sprite_angle - player_a;
    
    while (angle_diff > 180) angle_diff -= 360;
    while (angle_diff < -180) angle_diff += 360;
    
    float fov = 72.0f;
    if (angle_diff < -fov / 2.0f || angle_diff > fov / 2.0f) return;
    
    float screen_x = (angle_diff / fov + 0.5f) * cw;
    
    float sprite_height = (ch / dist) * sprite_scale;
    int draw_start_y = ch / 2 - (int)(sprite_height / 2);
    int draw_end_y = ch / 2 + (int)(sprite_height / 2);
    
    int sprite_width = (int)sprite_height;
    int draw_start_x = (int)screen_x - sprite_width / 2;
    int draw_end_x = draw_start_x + sprite_width;
    
    if (!sprite_tex) return;
    
    FogInfo fi;
    get_fog_info(dist, &fi);
    
    for (int x = draw_start_x; x < draw_end_x; x++) {
        if (x < 0 || x >= cw) continue;
        
        // check if behind wall
        if (depth_buffer && depth_buffer[x] < dist) continue;
        
        int tex_x = ((x - draw_start_x) * sprite_tex->width) / sprite_width;
        if (tex_x >= sprite_tex->width) tex_x = sprite_tex->width - 1;
        
        uint32* buffer_col = &g_back_buffer[cy * g_width + (cx + x)];
        float v_step = (float)sprite_tex->height / (draw_end_y - draw_start_y);
        float v = (draw_start_y < 0 ? -draw_start_y : 0) * v_step;

        for (int y = (draw_start_y < 0 ? 0 : draw_start_y); y < (draw_end_y >= ch ? ch : draw_end_y); y++, v += v_step) {
            int tex_y = (int)v;
            
            if (die_timer >= 0 && die_timer < 1.0f) {
                float cutoff = (1.0f - die_timer) * sprite_tex->height;
                if (tex_y < cutoff) continue;
            }

            uint32 c = sprite_tex->data[tex_y * sprite_tex->width + tex_x];
            
            uint8 r = (c >> 16) & 0xFF;
            uint8 g = (c >> 8) & 0xFF;
            uint8 b = c & 0xFF;
            if ((r < 5 && g < 5 && b < 5) || (r > 250 && g > 250 && b > 250)) continue;
            
            if (die_timer >= 0 && die_timer < 1.0f) {
                float tint = 1.0f - die_timer;
                r = (uint8)(r + (255 - r) * tint);
                g = (uint8)(g * die_timer);
                b = (uint8)(b * die_timer);
                c = RGB(r, g, b);
            }

            buffer_col[y * g_width] = apply_fog_fast(c, &fi);
        }
    }
}

void render_black_hole(int cx, int cy, int ch, int cw, float hole_x, float hole_y,
                       float player_x, float player_y, float player_a,
                       float radius, float* depth_buffer) {
    float dx = hole_x - player_x;
    float dy = hole_y - player_y;
    float dist = sqrt(dx * dx + dy * dy);
    
    if (dist < 0.1f || dist > 20.0f) return;
    
    float hole_angle = atan2(dy, dx) * 180.0f / 3.14159f;
    float angle_diff = hole_angle - player_a;
    
    while (angle_diff > 180) angle_diff -= 360;
    while (angle_diff < -180) angle_diff += 360;
    
    float fov = 72.0f;
    if (angle_diff < -fov / 2.0f || angle_diff > fov / 2.0f) return;
    
    float screen_x = (angle_diff / fov + 0.5f) * cw;
    
    float hole_height = (ch / dist) * radius * 0.8f;
    
    int vertical_offset = (int)(ch * 0.45f);
    int draw_center_y = ch / 2 - vertical_offset;
    int draw_center_x = (int)screen_x;
    
    int hole_radius = (int)(hole_height / 2);
    
    int num_rings = 4;
    for (int ring = 0; ring < num_rings; ring++) {
        float ring_inner = hole_radius * (1.5f + ring * 0.4f);
        float ring_outer = hole_radius * (1.7f + ring * 0.4f);
        float ring_inner_sq = ring_inner * ring_inner;
        float ring_outer_sq = ring_outer * ring_outer;
        float ring_height_ratio = 0.18f;
        
        for (int y = -(int)ring_outer; y <= (int)ring_outer; y++) {
            int py = draw_center_y + (int)(y * ring_height_ratio);
            if (py < 0 || py >= ch) continue;
            
            uint32* buffer_row = &g_back_buffer[(cy + py) * g_width + cx];
            int yy = y * y;
            
            for (int x = -(int)ring_outer; x <= (int)ring_outer; x++) {
                int px = draw_center_x + x;
                if (px < 0 || px >= cw) continue;
                
                int dist_sq = x * x + yy;
                if (dist_sq < ring_inner_sq || dist_sq > ring_outer_sq) continue;
                
                if (depth_buffer && depth_buffer[px] < dist) continue;
                
                float d = sqrt((float)dist_sq);
                float ring_intensity = 1.0f - ((d - ring_inner) / (ring_outer - ring_inner));
                ring_intensity *= (1.0f - ring * 0.15f);
                
                int r = (int)(160 * ring_intensity);
                int g = (int)(40 * ring_intensity);
                int b = (int)(60 * ring_intensity);
                
                if (((int)(x * 0.5f) + (int)(y * 0.5f) + ring) % 3 == 0) {
                    r += 20;
                    b += 20;
                }
                
                if (r > 255) r = 255;
                if (b > 255) b = 255;
                
                buffer_row[px] = RGB(r, g, b);
            }
        }
    }
    
    float hole_radius_sq = (float)(hole_radius * hole_radius);
    for (int y = -hole_radius; y <= hole_radius; y++) {
        int py = draw_center_y + y;
        if (py < 0 || py >= ch) continue;
        
        uint32* buffer_row = &g_back_buffer[(cy + py) * g_width + cx];
        int yy = y * y;
        
        for (int x = -hole_radius; x <= hole_radius; x++) {
            int px = draw_center_x + x;
            if (px < 0 || px >= cw) continue;
            
            int dist_sq = x * x + yy;
            if (dist_sq > hole_radius_sq) continue;
            
            if (depth_buffer && depth_buffer[px] < dist) continue;
            
            float intensity = 2.0f - (sqrt((float)dist_sq) / hole_radius);
            
            uint32 color;
            if (dist_sq < hole_radius_sq * 0.09f) {
                color = RGB(220, 0, 20);
            } else if (dist_sq < hole_radius_sq * 0.36f) {
                color = RGB(180, 30, 40);
            } else {
                int r = (int)(140 * intensity);
                int g = (int)(20 * intensity);
                int b = (int)(20 * intensity);
                color = RGB(r, g, b);
            }
            
            buffer_row[px] = color;
        }
    }
}

static float row_dist_table[1024];
static int last_ch = -1;

static inline void render_col(int ch, int cy,
                              float ray_dx, float ray_dy, float dist, 
                              int tile, int side, float wall_height,
                              int table_hit, float table_dist, float table_x, 
                              float table_y, int table_side,
                              float player_x, float player_y,
                              Bitmap* wall_texture, Bitmap* window_texture, Bitmap* table_texture,
                              uint32* buffer_ptr, DoomGameState* game) {
    float camera_height = 0.55f;
    float unit_height = ch / dist;
    int draw_start = ch / 2 - (int)((wall_height - camera_height) * unit_height);
    int draw_end = ch / 2 + (int)(camera_height * unit_height);

    uint32 default_wall_color = RGB(40, 30, 20);
    
    uint32 skycol;
    switch (game->current_level) {
        case 0:
            skycol = RGB(42, 24, 64);
            break;
        case 1:
            skycol = RGB(72, 32, 100);
            break;
        default:
            skycol = RGB(42, 24, 64);
            break;
    }

    for (int py = 0; py < (draw_start < 0 ? 0 : draw_start); py++) {
        if (cy + py >= 0 && cy + py < g_height) {
            buffer_ptr[py * g_width] = skycol;
        }
    }

    if (draw_start < 0) draw_start = 0;
    if (draw_end >= ch) draw_end = ch - 1;

    float hit_x = player_x + ray_dx * dist;
    float hit_y = player_y + ray_dy * dist;
    float wall_coord = (side == 0) ? (hit_y - floor(hit_y)) : (hit_x - floor(hit_x));
    
    Bitmap* tex = (tile == 2) ? wall_texture :
                  (tile == 4) ? window_texture : NULL;
    
    FogInfo fi;
    get_fog_info(dist, &fi);

    if (tex) {
        float tiled_coord = wall_coord * 2.0f;
        tiled_coord -= (int)tiled_coord;

        int tex_x = (int)(tiled_coord * tex->width);
        if (tex_x >= tex->width) tex_x = tex->width - 1;
        if (tex_x < 0) tex_x = 0;

        float v_step = wall_height / (unit_height * wall_height);
        float v = (draw_start - (ch / 2 - (int)((wall_height - camera_height) * unit_height))) * (wall_height / (unit_height * wall_height));
        
        uint32* tex_data = tex->data;
        int tex_width = tex->width;
        int tex_height = tex->height;

        for (int py = draw_start; py < draw_end; py++, v += v_step) {
            float tiled_v = v * 2.0f;
            int tex_y = ((int)(tiled_v * tex_height)) % tex_height;
            if (tex_y < 0) tex_y += tex_height;

            uint32 c = tex_data[tex_y * tex_width + tex_x];
            
            if (tile == 4 && wall_height >= 1.25f && (int)v == 0) {
                float grad = 1.0f - (v - floor(v));
                c = RGB(25 + (int)(grad * 80), 20 + (int)(grad * 60), 40 + (int)(grad * 80));
            }
            
            if (cy + py >= 0 && cy + py < g_height) {
                buffer_ptr[py * g_width] = apply_fog_fast(c, &fi);
            }
        }
    } else {
        uint32 fogged_wall = apply_fog_fast(default_wall_color, &fi);
        for (int py = draw_start; py < draw_end; py++) {
            if (cy + py >= 0 && cy + py < g_height) {
                buffer_ptr[py * g_width] = fogged_wall;
            }
        }
    }

    if (draw_end < ch) {
        for (int py = draw_end; py < ch; py++) {
            float row_dist = row_dist_table[py];
            float fx = player_x + ray_dx * row_dist;
            float fy = player_y + ray_dy * row_dist;
            
            uint32 c = (((int)(fx * 4.0f) + (int)(fy * 4.0f)) & 1) ? 
                       RGB(90, 60, 120) : RGB(230, 222, 240);
            
            if (cy + py >= 0 && cy + py < g_height) {
                buffer_ptr[py * g_width] = apply_fog(c, row_dist);
            }
        }
    }
}

void doom(Window* win) {
    if (!win) return;
    
    Process* p = get_process(win->pid);
    if (!p || !p->data) return;
    
    DoomGameState* game = (DoomGameState*)p->data;
    
    handle_input(win, game);

    static uint32 last_ticks = 0;
    uint32 current_ticks = timer_ticks;
    uint32 elapsed = current_ticks - last_ticks;
    last_ticks = current_ticks;

    float dt = (elapsed > 0) ? (float)elapsed / 67.0f : (1.0f / 67.0f);
    if (dt > 0.1f) dt = 0.1f;
    int trigger_transition = 0;
    if (game->flash_active) {
        game->flash_timer -= dt;
        if (game->flash_timer <= 0) {
            game->flash_active = 0;
            trigger_transition = 1;
        }
    }

    int cx = win->x + WIN_BORDER;
    int cy = win->y + TITLEBAR_H;
    int cw = win->width - (2 * WIN_BORDER);
    int ch = win->height - TITLEBAR_H - WIN_BORDER;

    if (ch != last_ch) {
        for (int py = 0; py < ch; py++) {
            float dy = (float)py - (float)ch / 2.0f;
            row_dist_table[py] = (dy <= 0) ? 1000.0f : (float)ch / (2.0f * dy);
        }
        last_ch = ch;
    }

    if (!game->game_started) {
        draw_splash_screen(cx, cy, cw, ch, game->textures[TEX_SPLASH]);
    } else if (game->health <= 0) {
        game->game_started = 0;
        game->player_x = START_X;
        game->player_y = START_Y;
        game->player_a = 0.0f;
        
        if (game->music_handle != -1) {
            ac97_stop_voice(game->music_handle);
            game->music_handle = -1;
        }
        
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            game->projectiles[i].active = 0;
        }
        for (int i = 0; i < MAX_ENEMIES; i++) {
            game->enemies[i].active = 0;
        }
    } else {
        float fov = 75.0f;
        
        float depth_buffer[1024];
        for (int i = 0; i < cw && i < 1024; i++) {
            depth_buffer[i] = 1000.0f;
        }

        uint32* screen_start = &g_back_buffer[cy * g_width + cx];
        int (*map)[MAP_HEIGHT] = get_map(game);

        for (int x = 0; x < cw; x++) {
            int screen_x = cx + x;
            if (screen_x < 0 || screen_x >= g_width) continue;

            float t = (float)x / (float)(cw - 1);
            float ray_angle = game->player_a - fov / 2.0f + t * fov;
            float ray_dx = cos_deg(ray_angle);
            float ray_dy = sin_deg(ray_angle);

            int mapX = (int)game->player_x;
            int mapY = (int)game->player_y;
            
            float deltaDistX = fabs(1.0f / ray_dx);
            float deltaDistY = fabs(1.0f / ray_dy);
            
            int stepX = (ray_dx < 0) ? -1 : 1;
            int stepY = (ray_dy < 0) ? -1 : 1;
            
            float sideDistX = (ray_dx < 0) ? (game->player_x - mapX) * deltaDistX : 
                                              (mapX + 1.0f - game->player_x) * deltaDistX;
            float sideDistY = (ray_dy < 0) ? (game->player_y - mapY) * deltaDistY : 
                                              (mapY + 1.0f - game->player_y) * deltaDistY;
            
            int hit = 0, side = 0, tile = 0;
            int table_hit = 0, table_side = 0;
            float table_dist = 0, table_x = 0, table_y = 0;
            
            while (!hit && (mapX >= 0 && mapX < MAP_WIDTH && mapY >= 0 && mapY < MAP_HEIGHT)) {
                if (sideDistX < sideDistY) {
                    sideDistX += deltaDistX;
                    mapX += stepX;
                    side = 0;
                } else {
                    sideDistY += deltaDistY;
                    mapY += stepY;
                    side = 1;
                }
                
                if (mapX < 0 || mapX >= MAP_WIDTH || mapY < 0 || mapY >= MAP_HEIGHT) break;
                
                tile = map[mapX][mapY];
                
                if (tile == 5 && !table_hit) {
                    table_hit = 1;
                    table_dist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
                    table_x = game->player_x + ray_dx * table_dist;
                    table_y = game->player_y + ray_dy * table_dist;
                    table_side = side;
                } else if (tile > 0 && tile != 5) {
                    hit = 1;
                }
            }
            
            float dist = (side == 0) ? (sideDistX - deltaDistX) : (sideDistY - deltaDistY);
            dist *= cos_deg(ray_angle - game->player_a);
            if (dist < 0.0001f) dist = 0.0001f;

            depth_buffer[x] = dist;

            float wall_height = (tile == 1 || tile == 2 || tile == 4) ? 2.5f : 0.5f;

            render_col(ch, cy, ray_dx, ray_dy, dist, tile, side, wall_height,
                         table_hit, table_dist, table_x, table_y, table_side,
                         game->player_x, game->player_y,
                         game->textures[TEX_WALL], game->textures[TEX_WINDOW], game->textures[TEX_TABLE],
                         &screen_start[x], game);
        }

        if (!game->dialog_active) {
            if (game->hurt_timer > 0) {
                game->hurt_timer -= dt;
            }
            
            update_enemies(game, dt);
            update_projectiles(game, dt);
            update_black_hole(win, game, dt);
        }
        
        if (game->black_hole_active) {
            render_black_hole(cx, cy, ch, cw,
                             game->black_hole_x, game->black_hole_y,
                             game->player_x, game->player_y, game->player_a,
                             game->black_hole_radius, depth_buffer);
        }
        
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (game->enemies[i].active) {
                float die_val = game->enemies[i].is_dying ? game->enemies[i].die_timer : 1.0f;
                render_sprite(cx, cy, ch, cw, 
                             game->enemies[i].x, game->enemies[i].y,
                             game->player_x, game->player_y, game->player_a,
                             game->textures[TEX_ENEMY], 1.0f, depth_buffer, die_val);
            }
        }
        
        for (int i = 0; i < MAX_PROJECTILES; i++) {
            if (game->projectiles[i].active) {
                Bitmap* proj_tex = game->projectiles[i].is_player_bullet ? 
                                game->textures[TEX_PLAYER_PROJECTILE] : 
                                game->textures[TEX_ENEMY_PROJECTILE];
                
                float scale = game->projectiles[i].is_player_bullet ? 0.6f : 0.3f;
                render_sprite(cx, cy, ch, cw,
                            game->projectiles[i].x, game->projectiles[i].y,
                            game->player_x, game->player_y, game->player_a,
                            proj_tex, scale, depth_buffer, 1.0f);
            }
        }
        draw_game_ui(cx, cy, game);
    }
    
    if (game->dialog_active) {
        draw_dialog(cx, cy, cw, ch, game);
    }

    if (trigger_transition) {
        start_melt(win, game);
        setup_stage(game, game->flash_stage);
        game->game_started = 1;
    }
    
    if (game->hurt_timer > 0) {
        float intensity = game->hurt_timer / 0.3f;
        int border = 20;
        for (int y = 0; y < ch; y++) {
            uint32* buffer_row = &g_back_buffer[(cy + y) * g_width + cx];
            for (int x = 0; x < cw; x++) {
                int dist_from_edge = x;
                if (cw - x < dist_from_edge) dist_from_edge = cw - x;
                if (y < dist_from_edge) dist_from_edge = y;
                if (ch - y < dist_from_edge) dist_from_edge = ch - y;
                
                if (dist_from_edge < border) {
                    float edge_intensity = 1.0f - ((float)dist_from_edge / border);
                    uint32 existing = buffer_row[x];
                    
                    uint8 er = (existing >> 16) & 0xFF;
                    uint8 eg = (existing >> 8) & 0xFF;
                    uint8 eb = existing & 0xFF;
                    
                    int r = er + (int)(100 * intensity * edge_intensity);
                    int g = eg - (int)(50 * intensity * edge_intensity);
                    int b = eb - (int)(50 * intensity * edge_intensity);
                    
                    if (r > 255) r = 255;
                    if (g < 0) g = 0;
                    if (b < 0) b = 0;
                    
                    buffer_row[x] = RGB(r, g, b);
                }
            }
        }
    }

    if (game->melt_active) {
        int all_done = 1;
        int num_slices = (cw + MELT_SLICE_WIDTH - 1) / MELT_SLICE_WIDTH;
        
        for (int i = 0; i < num_slices; i++) {
            if (game->melt_offsets[i] < ch) {
                all_done = 0;
                int start_y = (game->melt_offsets[i] < 0) ? 0 : game->melt_offsets[i];

                for (int y = start_y; y < ch; y++) {
                    int src_y = y - game->melt_offsets[i];
                    if (src_y >= 0 && src_y < ch) {
                        uint32* screen_row = &g_back_buffer[(cy + y) * g_width + cx];
                        uint32* melt_row = &game->melt_buffer[src_y * cw];
                        
                        for (int sx = 0; sx < MELT_SLICE_WIDTH; sx++) {
                            int x = i * MELT_SLICE_WIDTH + sx;
                            if (x < cw) {
                                screen_row[x] = melt_row[x];
                            }
                        }
                    }
                }

                game->melt_offsets[i] += (16 + (int)(randf() * 12));
            }
        }

        if (all_done) {
            game->melt_active = 0;
            if (game->melt_offsets) kfree(game->melt_offsets);
            if (game->melt_buffer) kfree(game->melt_buffer);
            game->melt_offsets = NULL;
            game->melt_buffer = NULL;
        }
    }

    if (game->flash_active) {
        float intensity = 0;
        float half_time = 1.0f;
        if (game->flash_timer > half_time) {
            intensity = (2.0f - game->flash_timer) / half_time;
        } else {
            intensity = game->flash_timer / half_time;
        }

        if (intensity > 0) {
            if (intensity > 1.0f) intensity = 1.0f;
            for (int y = 0; y < ch; y++) {
                uint32* buffer_row = &g_back_buffer[(cy + y) * g_width + cx];
                for (int x = 0; x < cw; x++) {
                    uint32 existing = buffer_row[x];
                    uint8 r = (existing >> 16) & 0xFF;
                    uint8 g = (existing >> 8) & 0xFF;
                    uint8 b = existing & 0xFF;

                    r = (uint8)(r + (255 - r) * intensity);
                    g = (uint8)(g + (255 - g) * intensity);
                    b = (uint8)(b + (255 - b) * intensity);

                    buffer_row[x] = RGB(r, g, b);
                }
            }
        }
    }
}

void doom_cleanup(Window* win) {
    if (!win) return;

    kvalid();
    
    Process* p = get_process(win->pid);
    if (!p) return;
    
    kprint("[DOOM] cleanup win PID %d...\n", p->pid);

    if (p->data) {
        DoomGameState* game = (DoomGameState*)p->data;
        
        if (game->music_handle != -1) {
            ac97_stop_voice(game->music_handle);
            game->music_handle = -1;
        }

        if (game->melt_offsets) kfree(game->melt_offsets);
        if (game->melt_buffer) kfree(game->melt_buffer);
        game->melt_offsets = NULL;
        game->melt_buffer = NULL;

        if (game->current_dialog_icon) {
            texcache_rel(game->current_dialog_ptr[game->current_dialog_idx].icon_path);
            game->current_dialog_icon = NULL;
        }

        for (size_t i = 0; i < NUM_DOOM_TEXTURES; i++) {
            if (game->textures[i]) {
                if (DOOM_TEXTURES_DEFS[i].type == TEX_TYPE_BMP) {
                    texcache_rel(DOOM_TEXTURES_DEFS[i].identifier);
                    kprint("[DOOM] released BMP texture '%s' from cache\n", DOOM_TEXTURES_DEFS[i].identifier);
                } else if (DOOM_TEXTURES_DEFS[i].type == TEX_TYPE_PROCEDURAL) {
                    free_bmp(game->textures[i]);
                    kprint("[DOOM] freed procedural texture '%s'\n", DOOM_TEXTURES_DEFS[i].identifier);
                }
                game->textures[i] = NULL;
            }
        }
    }

    if (p->data) {
        kfree(p->data);
        p->data = NULL;
    }
    
    kprint("[DOOM] cleanup is done\n");
}

void launch_doom() {
    init_fog_table();
    
    Process* p = create_process("Doom");
    if (!p) {
        kprint("ERROR: Failed to create process for DOOM\n");
        return;
    }

    DoomGameState* game = (DoomGameState*)kmalloc(sizeof(DoomGameState));
    kvalid();
    if (!game) {
        kprint("could not alloc game state\n");
        cleanup_process(p->pid);
        return;
    }
    
    memset(game, 0, sizeof(DoomGameState));
    game->player_x = START_X;
    game->player_y = START_Y;
    game->player_a = 0.0f;
    game->game_started = 0;
    game->music_handle = -1;
    
    kprint("[DOOM] loading textures...\n");
    
    BOOL all_textures_loaded = TRUE;
    
    for (size_t i = 0; i < NUM_DOOM_TEXTURES; i++) {
        const DoomTextureDef* def = &DOOM_TEXTURES_DEFS[i];
        
        if (def->type == TEX_TYPE_BMP) {
            game->textures[i] = texcache_get(def->identifier);
            kvalid();
            
            if (!game->textures[i]) {
                kprint("ERROR: Failed to load BMP texture '%s'\n", def->identifier);
                all_textures_loaded = FALSE;
            } else {
                kprint("[DOOM] loaded BMP texture '%s'\n", def->identifier);
            }
        } else if (def->type == TEX_TYPE_PROCEDURAL) {
            uint32 prim, sec, acc;
            
            if (strcmp(def->identifier, "enemy") == 0) {
                prim = ENEMY_COLOR_PRIMARY;
                sec = ENEMY_COLOR_SECONDARY;
                acc = ENEMY_COLOR_ACCENT;
            } else if (strcmp(def->identifier, "player_bullet") == 0) {
                prim = PLAYER_PROJ_COLOR_PRIMARY;
                sec = PLAYER_PROJ_COLOR_SECONDARY;
                acc = PLAYER_PROJ_COLOR_ACCENT;
            } else if (strcmp(def->identifier, "inkblot") == 0) {
                prim = ENEMY_PROJ_COLOR_PRIMARY;
                sec = ENEMY_PROJ_COLOR_SECONDARY;
                acc = ENEMY_PROJ_COLOR_ACCENT;
            } else {
                prim = COLOR_PRIMARY;
                sec = COLOR_SECONDARY;
                acc = COLOR_ACCENT;
            }
            
            game->textures[i] = draw_p_bmp(
                def->data.procedural.pattern,
                prim, sec, acc,
                def->data.procedural.transparent_idx
            );
        }
    }

    if (!all_textures_loaded) {
        kprint("ERROR: couldn't load some textures\n");
        
        for (size_t i = 0; i < NUM_DOOM_TEXTURES; i++) {
            if (game->textures[i]) {
                if (DOOM_TEXTURES_DEFS[i].type == TEX_TYPE_BMP) {
                    texcache_rel(DOOM_TEXTURES_DEFS[i].identifier);
                } else if (DOOM_TEXTURES_DEFS[i].type == TEX_TYPE_PROCEDURAL) {
                    free_bmp(game->textures[i]);
                }
            }
        }
        
        kfree(game);
        cleanup_process(p->pid);
        return;
    }
    
    kprint("[DOOM] All textures loaded successfully.\n");
    
    p->data = game;

    Window* w = window(p->pid, "DOOM", -1, -1, 420, 302);
    if (!w) {
        kprint("ERROR: Failed to create window for DOOM\n");
        
        for (size_t i = 0; i < NUM_DOOM_TEXTURES; i++) {
            if (game->textures[i]) {
                if (DOOM_TEXTURES_DEFS[i].type == TEX_TYPE_BMP) {
                    texcache_rel(DOOM_TEXTURES_DEFS[i].identifier);
                } else if (DOOM_TEXTURES_DEFS[i].type == TEX_TYPE_PROCEDURAL) {
                    free_bmp(game->textures[i]);
                }
            }
        }
        
        kfree(game);
        p->data = NULL;
        cleanup_process(p->pid);
        return;
    }

    w->content_renderer = doom;
    w->on_close = doom_cleanup;
    
    register_for_process(p->pid, w);
    
    kprint("[DOOM] Window created successfully (PID: %d)\n", p->pid);
}