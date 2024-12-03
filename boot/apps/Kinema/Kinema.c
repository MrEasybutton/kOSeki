#include "../lib_include.h"
#include <stdbool.h>

// Window coords so ball doesn't exceed bounds
#define WIN_X         0
#define WIN_Y         1
#define WIN_WIDTH     2
#define WIN_HEIGHT    3

#define BALL_X        5
#define BALL_Y        6
#define BALL_VX       7
#define BALL_VY       8
#define BALL_RADIUS   10

#define GRAVITY       1     
#define DAMPING       0.7   
#define MIN_VELOCITY  1     

// Placeholder seed
static unsigned int seed = 123456789; 

// My custom srand implemntation (its unused for now)
void srand_custom(unsigned int newSeed) {
    seed = newSeed;
}


int rand_custom() {
    seed = (1103515245 * seed + 12345) & 0x7fffffff; 
    return seed;
}


int rand_range(int low, int high) {
    if (low > high) {
        
        int temp = low;
        low = high;
        high = temp;
    }

    int range = high - low + 1;
    return (rand_custom() % range) + low;
}

// absolute func
int abs(int value) {
    return (value < 0) ? -value : value;
}

// Note: this is total trace points across all windows
#define MAX_TRACE_POINTS 200
int tracePoints[MAX_TRACE_POINTS][2];
int traceCount = 0;
bool traceEnabled = false;

// Track movement to move tracepoints with the window
int previousWinX = 0;
int previousWinY = 0;

int Kinema(int process_inst) {
    int *params = &iparams[process_inst * procparamlen];
    int offsetX = params[WIN_X] - previousWinX;
    int offsetY = params[WIN_Y] - previousWinY;

    previousWinX = params[WIN_X];
    previousWinY = params[WIN_Y];

    
    for (int i = 0; i < traceCount; i++) {
        tracePoints[i][0] += offsetX;
        tracePoints[i][1] += offsetY;
    }
    
    int closeClicked = DrawWindow(
        &params[WIN_X], &params[WIN_Y], &params[WIN_WIDTH], &params[WIN_HEIGHT],
        0, 0, 0, &params[9], process_inst
    );

    if (closeClicked) CloseProcess(process_inst);

    
    char title[] = "Kinema";
    DrawText(getFontCharacter, font_font_width, font_font_height, 
             title, params[WIN_X] + 5, params[WIN_Y], 0, 0, 0);

    
    params[BALL_VY] += GRAVITY;  
    params[BALL_X] += params[BALL_VX];  
    params[BALL_Y] += params[BALL_VY];  

    
    int winWidth = params[WIN_WIDTH];
    int winHeight = params[WIN_HEIGHT];
    int ballRadius = BALL_RADIUS;

    
    if (params[BALL_X] - ballRadius < 0 || params[BALL_X] + ballRadius > winWidth) {
        params[BALL_VX] = -params[BALL_VX] * DAMPING;  
        params[BALL_X] = (params[BALL_X] < ballRadius) ? ballRadius : winWidth - ballRadius;

        
        if (abs(params[BALL_VX]) < MIN_VELOCITY) params[BALL_VX] = (params[BALL_VX] < 0) ? -MIN_VELOCITY : MIN_VELOCITY;
    }

    
    if (params[BALL_Y] - ballRadius < 0 || params[BALL_Y] + ballRadius > winHeight) {
        params[BALL_VY] = -params[BALL_VY] * DAMPING;  
        params[BALL_Y] = (params[BALL_Y] < ballRadius) ? ballRadius : winHeight - ballRadius;

        
        if (abs(params[BALL_VY]) < MIN_VELOCITY) params[BALL_VY] = (params[BALL_VY] < 0) ? -MIN_VELOCITY : MIN_VELOCITY;
    }

    
    if (traceEnabled && traceCount < MAX_TRACE_POINTS) {
        tracePoints[traceCount][0] = params[WIN_X] + params[BALL_X];
        tracePoints[traceCount][1] = params[WIN_Y] + params[BALL_Y];
        traceCount++;
    }

    
    for (int i = 0; i < traceCount; i++) {
        DrawRect(tracePoints[i][0] - 2, tracePoints[i][1] - 2, 2, 2, 5, 235, 20); 
    }

    if (params[BALL_VX] == 0 && params[BALL_VY] == 0) {
        traceEnabled = false;
    }

    
    DrawCircle(params[WIN_X] + params[BALL_X], 
               params[WIN_Y] + params[BALL_Y], 
               ballRadius, 250, 40, 40);
    

    DrawRect(params[WIN_X], params[WIN_Y] + 450, 300, 20, 220, 160, 60);

    
    char startStopText[] = "Repeat";
    if (
        DrawButton(params[WIN_X] + 200, params[WIN_Y] + 40, 80, 30, 100, 80, 110,
            startStopText, 200, 180, 210, process_inst, 200, 180, 210
        ) == true) {
        params[BALL_X] = 5;
        params[BALL_Y] = 6;
        params[BALL_VX] = 7;
        params[BALL_VY] = 8;

        traceCount = 0;
    }

    char toggleTraceText[] = "Trace";
    if (
        DrawButton(params[WIN_X] + 200, params[WIN_Y] + 80, 80, 30, 80, 140, 80,
            toggleTraceText, 200, 180, 210, process_inst, 200, 180, 210
        ) == true) {
        traceEnabled = !traceEnabled;
    }
}
