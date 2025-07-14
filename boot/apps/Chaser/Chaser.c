#include "../../graphics.h"
#include "../../apps/process_system.h"
#include <stdbool.h>

#define WIN_X 0
#define WIN_Y 1
#define WIN_WIDTH 2
#define WIN_HEIGHT 3
#define GRID_SIZE 14
#define CELL_SIZE 18
#define MAX_LENGTH 100
#define UP 0
#define RIGHT 1
#define DOWN 2
#define LEFT 3

typedef struct {
    int x[MAX_LENGTH], y[MAX_LENGTH], length, orient, food_x, food_y, score;
    bool game_over;
    int update_counter;
} Game;

static Game game;
static int update_rate = 4;

void spawn_food(int seed) {
    bool valid = false;
    srand(seed + game.score * 37 + srand());
    
    for (int tries = 0; !valid && tries < 50; tries++) {
        game.food_x = (srand() % (GRID_SIZE - 2)) + 1;
        game.food_y = (srand() % (GRID_SIZE - 2)) + 1;
        
        valid = true;
        for (int i = 0; i < game.length; i++)
            if (game.x[i] == game.food_x && game.y[i] == game.food_y) {
                valid = false;
                break;
            }
    }
}

void init_game(int seed) {
    int mid = GRID_SIZE / 2;
    game.x[0] = mid; game.y[0] = mid;
    game.x[1] = mid-1; game.y[1] = mid;
    game.x[2] = mid-2; game.y[2] = mid;
    game.length = 3;
    game.orient = RIGHT;
    game.score = 0;
    game.game_over = false;
    game.update_counter = 0;
    spawn_food(seed);
}

void update_game(int seed) {
    if (game.game_over) return;
    int new_x = game.x[0] + (game.orient == RIGHT) - (game.orient == LEFT);
    int new_y = game.y[0] + (game.orient == DOWN) - (game.orient == UP);
    
    if (new_x < 0 || new_x >= GRID_SIZE || new_y < 0 || new_y >= GRID_SIZE) {
        game.game_over = true;
        return;
    }
    
    for (int i = 1; i < game.length; i++)
        if (new_x == game.x[i] && new_y == game.y[i]) {
            game.game_over = true;
            return;
        }
    
    bool ate_food = (new_x == game.food_x && new_y == game.food_y);
    for (int i = game.length - 1; i > 0; i--) {
        game.x[i] = game.x[i-1];
        game.y[i] = game.y[i-1];
    }
    game.x[0] = new_x;
    game.y[0] = new_y;
    
    if (ate_food) {
        if (game.length < MAX_LENGTH) game.length++;
        game.score++;
        if (update_rate > 2) update_rate--;
        spawn_food(seed);
    }
}

int Chaser(int process_inst) {
    int *params = &iparams[process_inst * procparamlen];
    int closeClicked = DrawWindow(
        &params[WIN_X], &params[WIN_Y], &params[WIN_WIDTH], &params[WIN_HEIGHT],
        params[4], params[5], params[6], &params[9], process_inst);
    
    static bool initialized = false;
    int x = params[WIN_X], y = params[WIN_Y];
    int width = params[WIN_WIDTH], height = params[WIN_HEIGHT];
    
    char ProgramTitle[] = "Gonathan's Quest";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             ProgramTitle, x + 5, y, 0, 0, 0);
    
    if (closeClicked) CloseProcess(process_inst);
    
    if (!initialized) {
        init_game(process_inst);
        initialized = true;
    }
    
    int key = Scancode;
    if (!game.game_over) {
        if (key == 0x48 && game.orient != DOWN) game.orient = UP;
        else if (key == 0x4D && game.orient != LEFT) game.orient = RIGHT;
        else if (key == 0x50 && game.orient != UP) game.orient = DOWN;
        else if (key == 0x4B && game.orient != RIGHT) game.orient = LEFT;
    } else if (key > 0) {
        init_game(process_inst);
    }
    
    if (++game.update_counter >= update_rate) {
        update_game(process_inst);
        game.update_counter = 0;
    }
    
    int grid_size = GRID_SIZE * CELL_SIZE;
    int grid_x = x + (width - grid_size) / 2;
    int grid_y = y + 35;
    DrawRect(grid_x - 2, grid_y - 2, grid_size + 4, grid_size + 4, 40, 30, 60);
    DrawRect(grid_x, grid_y, grid_size, grid_size, 20, 20, 30);
    
    for (int i = 0; i < game.length; i++) {
        int cell_x = grid_x + game.x[i] * CELL_SIZE + 2;
        int cell_y = grid_y + game.y[i] * CELL_SIZE + 2;
        int r = (i == 0) ? 240 : (i % 3 == 1) ? 130 : 220;
        int g = (i == 0) ? 120 : (i % 3 == 1) ? 130 : 30;
        int b = (i == 0) ? 0 : (i % 3 == 1) ? 130 : 30;
        DrawRect(cell_x, cell_y, CELL_SIZE - 4, CELL_SIZE - 4, r, g, b);
    }
    
    DrawRect(grid_x + game.food_x * CELL_SIZE + 3, grid_y + game.food_y * CELL_SIZE + 3, 
             CELL_SIZE - 6, CELL_SIZE - 6, 90, 169, 126);
    
    char score_text[10] = "SCORE:";
    score_text[6] = '0' + (game.score % 10);
    if (game.score >= 10) score_text[5] = '0' + ((game.score / 10) % 10);
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             score_text, x + 10, y + height, 230, 220, 255);
    
    char txt_game_over[] = "Gigi hit a wall!\nPRESS ANY KEY TO RESTART";
    if (game.game_over)
        DrawText(getFontCharacter, font_font_width, font_font_height, 
                 txt_game_over, 
                 x + 30, y + height / 2 + 60, 255, 100, 100);
    
    return 0;
}